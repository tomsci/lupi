#include <k.h>
#include <mmu.h>
#include <pageAllocator.h>
#include <arm.h>

void uart_init();
void irq_init();
void irq_enable();
void runLuaIntepreterModule(uintptr heapPtr);

void Boot() {
#ifdef ENABLE_DCACHE
	mmu_setCache(false, true);
#endif
	uart_init();
	printk("\n\n" LUPI_VERSION_STRING "\n");

	//printk("Start of code base is:\n");
	//printk("%X = %X\n", KKernelCodeBase, *(uint32*)KKernelCodeBase);

	// Set up data structures that weren't part of mmu_init()

	const int numPagesRam = KPhysicalRamSize >> KPageShift;
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

	// One Process page for first proc
	Process* firstProcess = GetProcess(0);
	mmu_mapPageInSection(Al, (uint32*)KProcessesSection_pt, (uintptr)firstProcess, KPageProcess);

	mmu_finishedUpdatingPageTables();

	zeroPage(TheSuperPage);
	irq_init();
	irq_enable();

#ifdef KLUA
	mmu_mapSectionContiguous(Al, KLuaHeapBase, KPageKluaHeap);
	mmu_finishedUpdatingPageTables();
	//interactiveLuaPrompt();
	runLuaIntepreterModule(KLuaHeapBase);
#else

	// Start first process (so exciting!)
	SuperPage* s = TheSuperPage;
	s->nextPid = 1;
	s->numValidProcessPages = 1;

	firstProcess->pid = 0;
	firstProcess->pdePhysicalAddress = 0;

	Process* p;
	int err = process_new("interpreter", &p);
	ASSERT(p == firstProcess && err == 0, err, (uint32)p);
	process_start(firstProcess);
#endif // KLUA
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
static void NAKED switchToKluaDebuggerMode(uintptr sp) {
	// for the klua debugger, use a custom stack and run in system mode.
	// This allows us access to all memory, but also means we can still do SVCs without
	// corrupting registers. This is required because Lua is still in user config so expects
	// to be able to do an SVC to print, for example, and System is the only mode that allows
	// this combination
	asm("MOV r1, r14");
	ModeSwitch(KPsrModeSystem | KPsrIrqDisable | KPsrFiqDisable);
	asm("MOV r13, r0");
	asm("BX r1");
}

void iThinkYouOughtToKnowImFeelingVeryDepressed() {
	if (!TheSuperPage->marvin) {
		if (!mmu_mapSectionContiguous(Al, KLuaDebuggerSection, KPageKluaHeap)) {
			printk("Failed to allocate memory for klua debugger heap, sorry.\n");
			hang();
		}
		TheSuperPage->marvin = true;
	}
	if (TheSuperPage->trapAbort) {
		TheSuperPage->exception = true;
		TheSuperPage->trapAbort = false;
		//printk("Returning from abort\n");
		return;
	} else {
		// We use a custom 8KB stack at the start of the debugger heap section
		switchToKluaDebuggerMode(KLuaDebuggerStackBase + KLuaDebuggerStackSize);
		runLuaIntepreterModule(KLuaDebuggerHeap);
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

static inline uint32 getFAR() {
	uint32 ret;
	asm("MRC p15, 0, %0, c6, c0, 0" : "=r" (ret));
	return ret;
}

static inline uint32 getDFSR() {
	uint32 ret;
	asm("MRC p15, 0, %0, c5, c0, 0" : "=r" (ret));
	return ret;
}

static inline uint32 getIFSR() {
	uint32 ret;
	asm("MRC p15, 0, %0, c5, c0, 1" : "=r" (ret));
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

void NAKED svc() {
	// Save onto supervisor mode stack.
	asm("PUSH {r4-r12, r14}");
	asm("MOV r3, sp"); // Full descending stack means sp now points to the regs we saved
	// r0, r1, r2 already have the correct data in them for handleSvc()
	asm("BL handleSvc");
	// Avoid leaking kernel info into user space (like we really care!)
	asm("MOV r2, #0");
	asm("MOV r3, #0");

	asm("POP {r4-r12, r14}");
	asm("MOVS pc, r14");
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

void NAKED kabort4(uint32 r0, uint32 r1, uint32 r2, uint32 r3) {
	asm("PUSH {r4}");
	asm("MOV r4, %0" : : "i" (KSuperPageAddress));
	asm("ADD r4, r4, %0" : : "i" (offsetof(SuperPage, crashRegisters)));

	asm("STMIA r4!, {r0-r3}");
	asm("STR r13, [r4], #4"); // Saves r4 itself which was pushed onto stack
	asm("STMIA r4!, {r5-r14}");

	asm("LDR r0, .notSavedValue");
	asm("STR r0, [r4], #4"); // r15 not stored
	asm("MRS r2, cpsr");
	asm("STR r2, [r4], #4");

	// marvin expects us to be in abort mode
	ModeSwitch(KPsrModeAbort | KPsrIrqDisable | KPsrFiqDisable);
	asm("LDR r13, .abortStackBase");

	asm("B iThinkYouOughtToKnowImFeelingVeryDepressed");
	LABEL_WORD(.notSavedValue, KRegisterNotSaved);
	LABEL_WORD(.abortStackBase, KAbortStackBase + KPageSize);
}
