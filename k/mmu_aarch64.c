#include <k.h>
#include <mmu.h>
#include ARCH_HEADER
#include <err.h>

#define SetTTBR(n, val)			WRITE_SPECIAL(TTBR ## n ## _EL1, (uintptr)(val))

#define TCR_TG1_4KB		(2ULL << 30)
#define TCR_PS_4GB		(0ULL << 32) // Physical size

#define TCR_T0SZ_SHIFT	(0)
#define TCR_T1SZ_SHIFT	(16)

#define KTable			(3)
#define KValidPage		(3)

#define ONE_GB_SHIFT	(30)
#define TWO_MB_SHIFT	(21)

#define DESCRIPTOR_UXN	BIT(54)
#define DESCRIPTOR_PXN	BIT(53)
#define DESCRIPTOR_AF	BIT(10)
#define DESCRIPTOR_RW	BIT(6)


void mmu_init() {

#if 0
	// uintptr* pde = (uintptr*)KPhysicalPdeBase;
	// TODO actually setup PDEs

	SetTTBR(0, KPhysicalPdeBase);
	SetTTBR(1, KPhysicalPdeBase);

	// Configure user/kern split - we only have 1GB RAM so go with 1GB max for user
	// T0SZ = 34, ie TTBR0 size is 2^(64-34) = 2^30 = 1GB
	uintptr tcr = TCR_TG1_4KB | TCR_PS_4GB | (34 << TCR_T0SZ_SHIFT);
	WRITE_SPECIAL(TCR_EL1, tcr);
#endif

}

Process* switch_process(Process* p) {
	if (!p) return NULL;
	Process* oldp = TheSuperPage->currentProcess;
	if (p == oldp) return NULL;

#ifdef HAVE_MMU // todo
	uint64 asid = indexForProcess(p);
	SetTTBR(0, p->pdePhysicalAddress | (asid << 48));
#endif

	TheSuperPage->currentProcess = p;
	return oldp;
}

void mmu_mapSect0Data(uintptr virtualAddress, uintptr physicalAddress, int npages) {
}

uintptr mmu_mapSectionContiguous(PageAllocator* pa, uintptr virtualAddress, uint8 type) {
	return virtualAddress;
}

void mmu_unmapSection(PageAllocator* pa, uintptr virtualAddress) {
}

bool mmu_mapSection(PageAllocator* pa, uintptr sectionAddress, uintptr ptAddress, uint32* ptsPt, uint8 ptPageType) {
	return true;
}

uintptr mmu_mapPageInSection(PageAllocator* pa, uint32* pt, uintptr virtualAddress, uint8 type) {
	return virtualAddress;
}

bool mmu_mapPagesInProcess(PageAllocator* pa, Process* p, uintptr virtualAddress, int numPages) {
	return true;
}

bool mmu_sharePage(PageAllocator* pa, Process* src, Process* dest, uintptr srcUserAddr) {
	return true;
}

void mmu_unmapPagesInProcess(PageAllocator* pa, Process* p, uintptr virtualAddress, int numPages) {
}

void mmu_finishedUpdatingPageTables() {
	asm("DSB SY");
}

Process* mmu_newProcess(PageAllocator* pa) {
	return NULL;
}

void mmu_processExited(PageAllocator* pa, Process* p) {
}

int mmu_processInit(Process* p) {
	return KErrNotSupported;
}
