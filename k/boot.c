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

	int numPagesRam = KPhysicalRamSize >> KPageShift;
	int paSizePages = PAGE_ROUND(pageAllocator_size(numPagesRam)) >> KPageShift;
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
	mmu_mapPageInSection(Al, (uint32*)KProcessesSection_pt, (uintptr)GetProcess(0), KPageProcess);

	mmu_finishedUpdatingPageTables();

	irq_init();

#ifdef KLUA
	mmu_mapSectionContiguous(Al, KLuaHeapBase, KPageKluaHeap);
	mmu_finishedUpdatingPageTables();
	//interactiveLuaPrompt();
	runLuaIntepreterModule();
#endif

	irq_enable();

	// Start first process (so exciting!)
	SuperPage* s = TheSuperPage;
	s->currentProcess = NULL;
	s->currentThread = NULL;
	s->nextPid = 1;
	Process* p = GetProcess(0); // The only one mapped, which we did a few lines up
	p->pdePhysicalAddress = 0;

	process_init(p); // This also does a switch_process()
	TheSuperPage->currentThread = firstThreadForProcess(p);
	printk("process_start\n");
	process_start("interpreter", firstThreadForProcess(p)->savedRegisters[13]);
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


#endif // HOSTED
