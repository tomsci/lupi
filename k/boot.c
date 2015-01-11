#include <k.h>
#include <mmu.h>
#include <pageAllocator.h>
#if defined(ARM)
#include <arm.h>
#elif defined(ARMV7_M)
#include <armv7-m.h>
#endif
#include <atags.h>
#include <exec.h>
#include <klua.h>

void uart_init();
void board_init();
#ifdef HAVE_SCREEN
void screen_init();
void screen_drawCrashed();
#endif
void early_printk(const char* fmt, ...) ATTRIBUTE_PRINTF(1, 2);
void dump_atags();
void parseAtags(uint32* atagsPtr, AtagsParams* params);

#if BOOT_MODE == 0
// Then bootmode.c isn't compiled, so stub out the check to always return normal boot
#define checkBootMode(x) (0)
#else
int checkBootMode(int bootMode);
#endif

#ifdef KLUA
void interactiveLuaPrompt();
#endif

static void initSuperPage();

void Boot(uintptr atagsPhysAddr) {
#ifdef ICACHE_IS_STILL_BROKEN
#ifdef ENABLE_DCACHE
	mmu_setCache(false, true);
#endif
#endif
	uart_init();

	early_printk("\n\n" LUPI_VERSION_STRING);

#ifdef HAVE_MPU
	mmu_enable();
#endif

#if defined(ARM) && !defined(ICACHE_IS_STILL_BROKEN)
	// Remove the temporary identity mapping for the first code page
	early_printk("Identity mapping going bye-bye\n");
	*(uint32*)KKernelPdeBase = 0;
	early_printk("Identity mapping gone\n");
#endif

	AtagsParams atags;
#ifdef LUPI_NO_SECTION0
	parseAtags((uint32*)atagsPhysAddr, &atags);
#else
	// Set up data structures that weren't part of mmu_init()
	mmu_mapSect0Data(KKernelAtagsBase, atagsPhysAddr & ~0xFFF, 1);
	parseAtags((uint32*)(KKernelAtagsBase + (atagsPhysAddr & 0xFFF)), &atags);
#endif
	const char* units;
	int amt;
	if (atags.totalRam > 2*1024*1024) {
		units = "MB";
		amt = atags.totalRam >> 20;
	} else {
		units = "KB";
		amt = atags.totalRam >> 10;
	}
	early_printk(" (RAM = %d %s, board = %X, bootMode = %d)\n", amt, units, atags.boardRev, BOOT_MODE);

#ifdef LUPI_NO_SECTION0
	initSuperPage(&atags);
	Process* firstProcess = GetProcess(0);
#else
	const int numPagesRam = atags.totalRam >> KPageShift;
	const uint paSizePages = PAGE_ROUND(pageAllocator_size(numPagesRam)) >> KPageShift;
	ASSERT(paSizePages << KPageShift <= KPageAllocatorMaxSize);
	mmu_mapSect0Data(KPageAllocatorAddr, KPhysPageAllocator, paSizePages);
	mmu_finishedUpdatingPageTables(); // So the pageAllocator's mem is visible

	// This is for tracking use of physical pages
	pageAllocator_init(Al, numPagesRam);
	// We pack everything static in between zero and KPhysPageAllocator
	pageAllocator_alloc(Al, KPageSect0, KPhysPageAllocator >> KPageShift);
	pageAllocator_alloc(Al, KPageAllocator, paSizePages);

	// From here on all physical memory should be allocated via the mmu_* functions that take a
	// PageAllocator* argument

	mmu_mapPageInSection(Al, (uint32*)KSectionZeroPt, KSuperPageAddress, KPageSect0);

	Process* firstProcess = GetProcess(0);

	// We want a new section for Process pages (this doesn't actually map in the Process pages,
	// only sets up the kernel PDE for them)
	mmu_createSection(Al, KProcessesSection);
	// Likewise each Process's (user) PDE
	mmu_createSection(Al, KProcessesPdeSection);
	// And a section for the PTs for all the processes' user PTs...
	mmu_createSection(Al, KKernPtForProcPts);
	// And the DFC thread stack
	mmu_mapPageInSection(Al, (uint32*)KSectionZeroPt, KDfcThreadStack, KPageSect0);

	// One Process page for first proc
	mmu_mapPageInSection(Al, (uint32*)KProcessesSection_pt, (uintptr)firstProcess, KPageProcess);

	mmu_finishedUpdatingPageTables();

	initSuperPage(&atags);
#endif

	board_init(); // Enables interrupts

#ifdef HAVE_SCREEN
	screen_init(); // Must be after board_init's enableInterrupts because it uses kern_sleep
#endif

	// Start first process (so exciting!)
	firstProcess->pid = 0;
#ifdef HAVE_MMU
	firstProcess->pdePhysicalAddress = 0;
#endif

#if defined(KLUA)
	kern_disableInterrupts(); // They get in the way...
	klua_runInterpreter();
#elif defined(LUPI_NO_PROCESS)
	printk("Nothing to do...\n");
	hang();
#else
	Process* p;
	int err = process_new("init", &p);
	ASSERT(p == firstProcess && err == 0, err, (uint32)p);
	process_start(firstProcess);
#endif // LUPI_NO_PROCESS
}

static void initSuperPage(const AtagsParams* atags) {
	zeroPage(TheSuperPage);
	SuperPage* s = TheSuperPage;
	s->totalRam = atags->totalRam;
	s->boardRev = atags->boardRev;
	s->bootMode = checkBootMode(BOOT_MODE);
	s->nextPid = 1;
	s->numValidProcessPages = 1;
#ifdef ARM
	s->dfcThread.state = EBlockedFromSvc;
	thread_setBlockedReason(&s->dfcThread, EBlockedWaitingForDfcs);
	s->svcPsrMode = KPsrModeSvc | KPsrFiqDisable /*| KPsrIrqDisable*/;
#endif

}

//TODO move this stuff

void NAKED zeroPage(void* page) {
	asm("ADD r1, r0, #4096"); // r1 has end pos
	asm("PUSH {r4-r9}");
	asm("MOV r2, #0");
	asm("MOV r3, #0");
	asm("MOV r4, #0");
	asm("MOV r5, #0");
	asm("MOV r6, #0");
	asm("MOV r7, #0");
	asm("MOV r8, #0");
	asm("MOV r9, #0");

	asm("1:");
	asm("STMIA r0!, {r2-r9}");
	asm("CMP r0, r1");
	asm("BNE 1b");

	asm("POP {r4-r9}");
	asm("BX lr");
}

void zeroPages(void* addr, int num) {
	uintptr ptr = ((uintptr)addr);
	const uintptr endAddr = ptr + (num << KPageShift);
	while (ptr != endAddr) {
		zeroPage((void*)ptr);
		ptr += KPageSize;
	}
}

#ifdef KLUA_DEBUGGER

void iThinkYouOughtToKnowImFeelingVeryDepressed() {
	TheSuperPage->quiet = false;
	uint32 far = getFAR();
	if (!TheSuperPage->marvin) {
		TheSuperPage->marvin = true;
		// Make sure IRQs remain disabled in subsequent SVC calls by the klua debugger
#if defined(ARM)
		TheSuperPage->svcPsrMode |= KPsrIrqDisable;
		TheSuperPage->rescheduleNeededOnSvcExit = false;
#elif defined(ARMV7_M)
		// Stop the clock
		PUT32(SYSTICK_CTRL, GET32(SYSTICK_CTRL) & ~SYSTICK_CTRL_ENABLE);
		// Prevent anything else from running
		uint32 pri = KCrashedBasePri;
		WRITE_SPECIAL(BASEPRI, pri);
		// Allow bouncing from nested exception directly to thread mode
		PUT32(SCB_CCR, GET32(SCB_CCR) | CCR_NONBASETHRDENA);
#endif
#ifdef HAVE_SCREEN
		screen_drawCrashed();
#endif
#ifdef HAVE_MMU
		if (!mmu_mapSectionContiguous(Al, KLuaDebuggerSection, KPageKluaHeap)) {
			printk("Failed to allocate memory for klua debugger heap, sorry.\n");
			hang();
		}
#endif
	}
	if (TheSuperPage->trapAbort) {
		TheSuperPage->exception = true;
		TheSuperPage->trapAbort = false;
		//printk("Returning from abort\n");
		return;
	} else if (TheSuperPage->crashFar && far == TheSuperPage->crashFar) {
		printk("Crash handler appears to be in a loop, gonna hang now...\n");
		hang();
	} else {
		TheSuperPage->crashFar = far;
		// We use a custom stack at the start of the debugger heap section
		switchToKluaDebuggerMode();

		// Now we're in debugger mode there's some more cleanup we need to do
#ifdef ARMV7_M
		// Clear SVCALLACT and SVCALLPENDED as we are bouncing out of svc if we were in it
		uint32 shcsr = GET32(SCB_SHCSR);
		PUT32(SCB_SHCSR, shcsr & ~(SHCSR_SVCALLPENDED | SHCSR_SVCALLACT));
		// Promote SVC to high priority
		PUT32(SCB_SHPR2, KCrashedPrioritySvc << 24);
		asm("DSB"); asm("ISB"); // after changing priorities
#endif
		klua_runInterpreterModule();
	}
}

#else

void iThinkYouOughtToKnowImFeelingVeryDepressed() {
	hang();
}

#endif // KLUA_DEBUGGER

const char KAssertionFailed[] = "\nASSERTION FAILURE at %s:%d\nASSERT(%s)\n";

// Some careful crafting here so the top of the stack and registers are nice and
// clean-looking in the debugger. This works quite nicely with the fixed args in
// registers r0-r3 and the variadic extras spilling onto the stack. The order
// of the arguments means that r1-r3 are already correct for the call to printk.
void NAKED assertionFail(int nextras, const char* file, int line, const char* condition, ...) {
	// Make sure we preserve r14 across the call to printk
	asm("PUSH {r0, r14}");
	// Disable quiet just in case
	LoadSuperPageAddress(r0);
	asm("MOV r14, #0");
	asm("STRB r14, [r0, %0]" : : "i" (offsetof(SuperPage, quiet)));
	asm("LDR r0, =KAssertionFailed");
	asm("BL printk");

	asm("POP {r0, r14}"); // r0 = nextras
	// We can't handle more than 4 extras. Any more get left on the stack
	asm("CMP r0, #4");
	asm("IT GT");
	asm("MOVGT r0, #4");

	// On stack now are zero or more extra args (as given by nextras)
	// We want to end with sp unwound to pointing above the extra args (ie how
	// it was before calling assertionFail)

	LoadSuperPageAddress(r1);
	asm("ADD r1, r1, %0" : : "i" (offsetof(SuperPage, crashRegisters)));
	// r1 = &crashRegisters
	asm("RSB r2, r0, #4"); // r2 = numNotSaved, ie 4-nextras

	// r3 = scratch
	asm(".saveReg:");
	asm("CMP r0, #0");
	asm("ITTTT NE");
	asm("LDRNE r3, [sp], #4");
	asm("STRNE r3, [r1], #4");
	asm("SUBNE r0, #1");
	asm("BNE .saveReg");

	// r3 = KRegisterNotSaved
	asm("LDR r3, .notSavedValue");
	asm(".notSaved:");
	asm("CMP r2, #0");
	asm("ITTT NE");
	asm("STRNE r3, [r1], #4");
	asm("SUBNE r2, #1");
	asm("BNE .notSaved");

#ifdef ARMV7_M
	asm("STMIA r1!, {r4-r12}");
	asm("STR r13, [r1], #4");
	asm("STR r14, [r1], #4");
#else
	asm("STMIA r1!, {r4-r14}");
#endif
	asm("STR r3, [r1], #4"); // For r15, which is not saved

#ifdef ARM //TODO
	// Finally, save CPSR
	asm("MRS r3, cpsr");
	asm("STR r3, [r1], #4");

	// Write fault address register to be r14
	asm("MCR p15, 0, r14, c6, c0, 0");

	// marvin expects us to be in abort mode
	ModeSwitch(KPsrModeAbort | KPsrIrqDisable | KPsrFiqDisable);
	asm("LDR r13, .abortStackBase");
#endif

	asm("B iThinkYouOughtToKnowImFeelingVeryDepressed");
	LABEL_WORD(.notSavedValue, KRegisterNotSaved);
#ifdef ARM
	LABEL_WORD(.abortStackBase, KAbortStackBase + KPageSize);
#endif
}

// void dumpATAGS() {
// 	mmu_mapSect0Data(KKernelAtagsBase, KPhysicalAtagsBase, 1);
// 	worddump((const void*)(KKernelAtagsBase + 0x100), 0x2E00);
// }
