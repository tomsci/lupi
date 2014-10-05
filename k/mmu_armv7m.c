#include <k.h>
#include <mmu.h>
#include <armv7-m.h>
#include <pageAllocator.h>

/**
MMU-like APIs for the ARMv7-M MPU.
*/

#define MPU_TYPE		0xE000ED90u
#define MPU_CTRL		0xE000ED94u
#define MPU_RNR			0xE000ED98u
#define MPU_RBAR		0xE000ED9Cu
#define MPU_RASR		0xE000EDA0u

/**
Creates a new section map for the given index (where `sectionIdx` is the virtual address right-
shifted by `KPageShift`) in the given Process.
*/
// static bool mmu_createUserSection(PageAllocator* pa, Process* p, int sectionIdx);

// Macro for when we're too early to call zeroPage()
#define init_zeroPages(ptr, n) \
	for (uint32 *p = ptr, *end = (uint32*)(((uint8*)ptr) + (n << KPageShift)); p != end; p++) {\
		*p = 0; \
	}

/*
Enter and exit with MPU disabled
*/
void mmu_init() {
}

void NAKED mmu_enable() {
	asm("BX lr");
}

// static void mmu_doMapPagesInSection(uintptr virtualAddress, uint32* sectPte, uintptr physicalAddress, int numPages) {
// }

void mmu_mapSect0Data(uintptr virtualAddress, uintptr physicalAddress, int npages) {
}

uintptr mmu_mapSectionContiguous(PageAllocator* pa, uintptr virtualAddress, uint8 type) {
	return 0;
}

void mmu_unmapSection(PageAllocator* pa, uintptr virtualAddress) {
}

bool mmu_mapSection(PageAllocator* pa, uintptr sectionAddress, uintptr ptAddress, uint32* ptsPt, uint8 ptPageType) {
	return 0;
}

uintptr mmu_mapPageInSection(PageAllocator* pa, uint32* pt, uintptr virtualAddress, uint8 type) {
	return 0;
}

void mmu_unmapPageInSection(PageAllocator* pa, uint32* pt, uintptr virtualAddress) {
}

void NAKED mmu_finishedUpdatingPageTables() {
	asm("MOV r0, #0");
	DSB(r0);
	asm("BX lr");
}

// Note doesn't invalidate the user process TLB
void mmu_freeUserSection(PageAllocator* pa, Process* p, int sectionIdx) {
}

bool mmu_mapPagesInProcess(PageAllocator* pa, Process* p, uintptr virtualAddress, int numPages) {
	return false;
}

bool mmu_sharePage(PageAllocator* pa, Process* src, Process* dest, uintptr sharedPage) {
	return false;
}

void mmu_unmapPagesInProcess(PageAllocator* pa, Process* p, uintptr virtualAddress, int numPages) {
}

Process* switch_process(Process* p) {
	if (!p) return NULL;
	Process* oldp = TheSuperPage->currentProcess;
	if (p == oldp) return NULL;
	//TODO stuff
	return oldp;
}

void mmu_processExited(PageAllocator* pa, Process* p) {
}
