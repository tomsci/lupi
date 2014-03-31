#include <k.h>
#include <mmu.h>
#include <pageAllocator.h>

void uart_init();
//void goDoLuaStuff();
void interactiveLuaPrompt();

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
	PageAllocator* pa = (PageAllocator*)KPageAllocatorAddr;
	pageAllocator_init(pa, numPagesRam);
	// We pack everything static in between zero and KPhysPageAllocator
	pageAllocator_alloc(pa, KPageUsed, KPhysPageAllocator >> KPageShift);
	pageAllocator_alloc(pa, KPageUsed, paSizePages);

	//printk("PA: firstFreePage = %d (0x%x)\n", pa->firstFreePage, (pa->firstFreePage << KPageShift));
	//ASSERT((pa->firstFreePage << KPageShift) == PAGE_ROUND(KPhysPageAllocator + pageAllocator_size(numPagesRam)));

	// From here on all physical memory should be via mmu_mapSectionAsPages() etc

	// We want a new section for Process pages (this doesn't actually map in the Process pages,
	// only sets up the PDE for them)
	mmu_mapSectionAsPages(pa, KProcessesBase, KProcessesPte);
	mmu_finishedUpdatingPageTables();

#ifdef KLUA
	mmu_mapSection(pa, KLuaHeapBase);
	mmu_finishedUpdatingPageTables();
	interactiveLuaPrompt();
#endif
}
