#include <k.h>
#include <mmu.h>
#include <pageAllocator.h>
#include <arm.h>
#include <atags.h>
#include <exec.h>
#include <klua.h>

void uart_init();
void irq_init();
#ifdef HAVE_PITFT
void tft_init();
void tft_drawCrashed();
#endif
void dump_atags();
void parseAtags(uint32* atagsPtr, AtagsParams* params);
static inline uint32 getFAR();

#if BOOT_MODE == 0
// Then bootmode.c isn't compiled, so stub out the check to always return normal boot
#define checkBootMode(x) (0)
#else
int checkBootMode(int bootMode);
#endif

void Boot(uintptr atagsPhysAddr) {
#ifdef ICACHE_IS_STILL_BROKEN
#ifdef ENABLE_DCACHE
	mmu_setCache(false, true);
#endif
#endif
	uart_init();

	printk("\n\n" LUPI_VERSION_STRING);

#ifndef ICACHE_IS_STILL_BROKEN
	// Remove the temporary identity mapping for the first code page
	printk("Identity mapping going bye-bye\n");
	*(uint32*)KKernelPdeBase = 0;
	printk("Identity mapping gone\n");
#endif

	// Set up data structures that weren't part of mmu_init()
	mmu_mapSect0Data(KKernelAtagsBase, atagsPhysAddr & ~0xFFF, 1);
	AtagsParams atags;
	parseAtags((uint32*)(KKernelAtagsBase + (atagsPhysAddr & 0xFFF)), &atags);
	printk(" (RAM = %d MB, board = %X, bootMode = %d)\n", atags.totalRam >> 20, atags.boardRev, BOOT_MODE);

	const int numPagesRam = atags.totalRam >> KPageShift;
	const uint paSizePages = PAGE_ROUND(pageAllocator_size(numPagesRam)) >> KPageShift;
	ASSERT_COMPILE(paSizePages<<KPageShift <= KPageAllocatorMaxSize);
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
	Process* firstProcess = GetProcess(0);
	mmu_mapPageInSection(Al, (uint32*)KProcessesSection_pt, (uintptr)firstProcess, KPageProcess);

	mmu_finishedUpdatingPageTables();

	zeroPage(TheSuperPage);
	TheSuperPage->totalRam = atags.totalRam;
	TheSuperPage->boardRev = atags.boardRev;
	TheSuperPage->bootMode = checkBootMode(BOOT_MODE);
	TheSuperPage->dfcThread.state = EBlockedFromSvc;
	thread_setBlockedReason(&TheSuperPage->dfcThread, EBlockedWaitingForDfcs);

	irq_init();
	kern_enableInterrupts();

#ifdef HAVE_PITFT
	tft_init(); // Must be after irq_init and enableInterrupts because it uses kern_sleep
#endif

	// Start first process (so exciting!)
	SuperPage* s = TheSuperPage;
	s->nextPid = 1;
	s->numValidProcessPages = 1;
	s->svcPsrMode = KPsrModeSvc | KPsrFiqDisable /*| KPsrIrqDisable*/;

	firstProcess->pid = 0;
	firstProcess->pdePhysicalAddress = 0;

	Process* p;
	int err = process_new("init", &p);
	ASSERT(p == firstProcess && err == 0, err, (uint32)p);
	process_start(firstProcess);
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

void NAKED hang() {
	asm("MOV r0, #0");
	WFI(r0); // Stops us spinning like crazy
	asm("B hang");
}

#ifdef KLUA_DEBUGGER

void iThinkYouOughtToKnowImFeelingVeryDepressed() {
	uint32 far = getFAR();
	if (!TheSuperPage->marvin) {
		TheSuperPage->marvin = true;
		// Make sure IRQs remain disabled in subsequent SVC calls by the klua debugger
		TheSuperPage->svcPsrMode |= KPsrIrqDisable;
		TheSuperPage->rescheduleNeededOnSvcExit = false;
#ifdef HAVE_PITFT
		tft_drawCrashed();
#endif
		if (!mmu_mapSectionContiguous(Al, KLuaDebuggerSection, KPageKluaHeap)) {
			printk("Failed to allocate memory for klua debugger heap, sorry.\n");
			hang();
		}
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
		switchToKluaDebuggerMode(KLuaDebuggerStackBase + 0x1000);
		klua_runIntepreterModule(KLuaDebuggerHeap);
	}
}

#else

void iThinkYouOughtToKnowImFeelingVeryDepressed() {
	hang();
}

#endif

void NAKED undefinedInstruction() {
	asm("PUSH {r0-r12}");
	uint32* regs;
	asm("MOV %0, sp" : "=r" (regs));
	uint32 addr;
	asm("MOV %0, r14" : "=r" (addr));
	addr -= 4; // r14_und is the instruction after
	printk("Undefined instruction at %X\n", addr);
	dumpRegisters(regs, addr, 0);
	iThinkYouOughtToKnowImFeelingVeryDepressed();
}

uint32 getCpsr() {
	uint32 ret;
	GetCpsr(ret);
	return ret;
}

uint32 getSpsr() {
	uint32 ret;
	GetSpsr(ret);
	return ret;
}

void NAKED prefetchAbort() {
	asm("PUSH {r0-r12}");
	uint32* regs;
	asm("MOV %0, sp" : "=r" (regs));
	uint32 addr;
	asm("MOV %0, r14" : "=r" (addr));
	addr -= 4; // r14_abt is the instruction after
	printk("Prefetch abort at %X ifsr=%X\n", addr, getIFSR());
	dumpRegisters(regs, addr, 0);
	iThinkYouOughtToKnowImFeelingVeryDepressed();
}

//#define STACK_DEPTH_DEBUG

#ifdef STACK_DEPTH_DEBUG
#define UNUSED_STACK 0x1A1A1A1A

void svc_cleanstack() {
	uint32 p = svcStackBase(TheSuperPage->currentThread->index);
	uint32 endp = (uint32)&p; // Don't trash anything above us otherwise we'll break svc()
	for (; p != endp; p += sizeof(uint32)) {
		*(uint32*)p = UNUSED_STACK;
	}
}

void svc_checkstack(uint32 execId) {
	// Find low-water mark of stack
	uint32 p = svcStackBase(TheSuperPage->currentThread->index);
	const uint32 endp = p + KPageSize;
	for (; p != endp; p += sizeof(uint32)) {
		if (*(uint32*)p != UNUSED_STACK) {
			// Found it
			printk("Exec %d used %d bytes of stack\n", execId, endp - p);
			break;
		}
	}
}
#endif

void NAKED svc() {
	// user r4-r12 has already been saved user-side so we can use them for temps

	// r4 = TheSuperPage
	asm("MOV r4, %0" : : "i" (KSuperPageAddress));

	// r8 = svcPsrMode
	asm("LDRB r8, [r4, %0]" : : "i" (offsetof(SuperPage, svcPsrMode)));

	// If we've crashed, we must be being called from the debugger so use the
	// debugger svc stack
	asm("LDRB r9, [r4, %0]" : : "i" (offsetof(SuperPage, marvin)));
	asm("CMP r9, #0");
	asm("BNE .loadDebuggerStack");

	// Reenable interrupts (depending on what svcPsrMode says)
	ModeSwitchReg(r8);

	// Now setup the right stack
	asm("LDR r5, [r4, %0]" : : "i" (offsetof(SuperPage, currentThread)));
	asm("LDRB r6, [r5, %0]" : : "i" (offsetof(Thread, index)));
	asm("MOV r7, %0" : : "i" (KUserStacksBase));
	asm("ADD r13, r7, r6, LSL %0" : : "i" (USER_STACK_AREA_SHIFT));
	asm("ADD r13, r13, #4096"); // So r13 points to top of stack not base

	asm(".postStackSet:");
	asm("MOV r3, r14"); // r14_svc is address to return to user side
	// Also save it for ourselves in the case where we don't get preempted
	asm("MOV r4, r14");

	#ifdef STACK_DEPTH_DEBUG
		asm("PUSH {r0-r3}");
		asm("MOV r11, r0"); // Save the exec id for later
		asm("BL svc_cleanstack");
		asm("POP {r0-r3}");
	#endif
	// r0, r1, r2 already have the correct data in them for handleSvc()
	asm("BL handleSvc");
	#ifdef STACK_DEPTH_DEBUG
		asm("PUSH {r0-r1}");
		asm("MOV r0, r11");
		asm("BL svc_checkstack");
		asm("POP {r0-r1}");
	#endif
	// Avoid leaking kernel info into user space (like we really care!)
	asm("MOV r2, #0");
	asm("MOV r3, #0");

	asm("MOVS pc, r4"); // r4 is where we stashed the user return address

	asm(".loadDebuggerStack:");
	asm("LDR r13, .debuggerStackTop");
	asm("B .postStackSet");
	LABEL_WORD(.debuggerStackTop, KLuaDebuggerSvcStackBase + 0x1000);
}

void NAKED dataAbort() {
	asm("PUSH {r0-r12, r14}");
	uint32* regs;
	asm("MOV %0, sp" : "=r" (regs));
	uint32 addr;
	asm("MOV %0, r14" : "=r" (addr));
	addr -= 8; // r14_abt is 8 bytes after (PC always 2 ahead for mem access)
	printk("Data abort at %X dfsr=%X far=%X\n", addr, getDFSR(), getFAR());
	dumpRegisters(regs, addr, getFAR());
	iThinkYouOughtToKnowImFeelingVeryDepressed();
	// We might want to return from this if we were already aborted - note we return to
	// r14-4 not r14-8, ie we skip over the instruction that caused the exception
	asm("POP {r0-r12, r14}");
	asm("SUBS pc, r14, #4");
}

const char KAssertionFailed[] = "\nASSERTION FAILURE at %s:%d\nASSERT(%s)\n";

// Some careful crafting here so the top of the stack and registers are nice and
// clean-looking in the debugger. This works quite nicely with the fixed args in
// registers r0-r3 and the variadic extras spilling onto the stack. The order
// of the arguments means that r1-r3 are already correct for the call to printk.
void NAKED assertionFail(int nextras, const char* file, int line, const char* condition, ...) {
	// Make sure we preserve r14 across the call to printk
	asm("PUSH {r0, r14}");
	asm("LDR r0, =KAssertionFailed");
	asm("BL printk");

	asm("POP {r0, r14}"); // r0 = nextras
	// We can't handle more than 4 extras. Any more get left on the stack
	asm("CMP r0, #4");
	asm("MOVGT r0, #4");

	// On stack now are zero or more extra args (as given by nextras)
	// We want to end with sp unwound to pointing above the extra args (ie how
	// it was before calling assertionFail)

	asm("MOV r1, %0" : : "i" (KSuperPageAddress));
	asm("ADD r1, r1, %0" : : "i" (offsetof(SuperPage, crashRegisters)));
	// r1 = &crashRegisters
	asm("RSB r2, r0, #4"); // r2 = numNotSaved, ie 4-nextras

	// r3 = scratch
	asm(".saveReg:");
	asm("CMP r0, #0");
	asm("LDRNE r3, [sp], #4");
	asm("STRNE r3, [r1], #4");
	asm("SUBNE r0, #1");
	asm("BNE .saveReg");

	// r3 = KRegisterNotSaved
	asm("LDR r3, .notSavedValue");
	asm(".notSaved:");
	asm("CMP r2, #0");
	asm("STRNE r3, [r1], #4");
	asm("SUBNE r2, #1");
	asm("BNE .notSaved");

	asm("STMIA r1!, {r4-r14}");
	asm("STR r3, [r1], #4"); // For r15, which is not saved
	// Finally, save CPSR
	asm("MRS r3, cpsr");
	asm("STR r3, [r1], #4");

	// Write fault address register to be r14
	asm("MCR p15, 0, r14, c6, c0, 0");

	// marvin expects us to be in abort mode
	ModeSwitch(KPsrModeAbort | KPsrIrqDisable | KPsrFiqDisable);
	asm("LDR r13, .abortStackBase");

	asm("B iThinkYouOughtToKnowImFeelingVeryDepressed");
	LABEL_WORD(.notSavedValue, KRegisterNotSaved);
	LABEL_WORD(.abortStackBase, KAbortStackBase + KPageSize);
}

// void dumpATAGS() {
// 	mmu_mapSect0Data(KKernelAtagsBase, KPhysicalAtagsBase, 1);
// 	worddump((const void*)(KKernelAtagsBase + 0x100), 0x2E00);
// }
