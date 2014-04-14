#include <k.h>
#include <mmu.h>
#include <pageAllocator.h>
#include <arm.h>

#ifndef HOSTED

void uart_init();
void irq_init();
void irq_enable();
//void goDoLuaStuff();
//void interactiveLuaPrompt();
void runLuaIntepreterModule();
const char* getLuaModule(const char* moduleName, int* modSize);

void Boot() {
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
	runLuaIntepreterModule();
#else

	// Start first process (so exciting!)
	SuperPage* s = TheSuperPage;
	s->nextPid = 1;
	s->numValidProcessPages = 1;

	firstProcess->pid = 0;
	firstProcess->pdePhysicalAddress = 0;

	Process* p = process_new("interpreter");
	ASSERT(p == firstProcess);
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

void dumpRegisters(uint32* regs, uint32 pc) {
	uint32 bnked[2];
	uint32* bankedStart = bnked;
	// The compiler will 'optimise' out the STM into a single "str %0, [sp]" unless
	// I include the volatile. The fact there's the small matter of the '^' which it is
	// IGNORING when making that decision... aaargh!
	ASM_JFDI("STM %0, {r13, r14}^" : : "r" (bankedStart));
	printk("r0:  %X r1:  %X r2:  %X r3:  %X\n", regs[0],  regs[1],  regs[2],  regs[3]);
	printk("r4:  %X r5:  %X r6:  %X r7:  %X\n", regs[4],  regs[5],  regs[6],  regs[7]);
	printk("r8:  %X r9:  %X r10: %X r11: %X\n", regs[8],  regs[9],  regs[10], regs[11]);
	printk("r12: %X r13: %X r14: %X r15: %X\n", regs[12], bnked[0], bnked[1], pc);
	uint32 spsr;
	asm("MRS %0, spsr" : "=r" (spsr));
	printk("CPSR was %X\n", spsr);
}


void NAKED undefinedInstruction() {
	asm("PUSH {r0-r12}");
	uint32* regs;
	asm("MOV %0, sp" : "=r" (regs));
	uint32 addr;
	asm("MOV %0, r14" : "=r" (addr));
	addr -= 4; // r14_und is the instruction after
	printk("Undefined instruction at 0x%X\n", addr);
	dumpRegisters(regs, addr);
	hang();
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
	printk("Prefetch abort at 0x%X ifsr=%X\n", addr, getIFSR());
	dumpRegisters(regs, addr);
	hang();
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
	asm("PUSH {r0-r12}");
	uint32* regs;
	asm("MOV %0, sp" : "=r" (regs));
	uint32 addr;
	asm("MOV %0, r14" : "=r" (addr));
	addr -= 8; // r14_abt is 8 bytes after (PC always 2 ahead for mem access)
	printk("Data abort at %X dfsr=%X far=%X\n", addr, getDFSR(), getFAR());
	dumpRegisters(regs, addr);
	hang();
}

#endif // HOSTED
