#ifndef MMU_H
#define MMU_H

#include <std.h>

#define MB *1024*1024
#define KSectionShift 20
#define KAddrToPdeIndexShift KSectionShift
#define KAddrToPdeAddrShift (KAddrToPdeIndexShift - 2)
#define KPageTableSize		4096
#define KSectionMask		0x000FFFFFu
#define KPagesInSection		(1 << (KSectionShift-KPageShift)) // ie 256

#define PTE_IDX(virtAddr)	(((virtAddr) & KSectionMask) >> KPageShift)
#define PAGE_ROUND(addr)	((addr + KPageSize - 1) & ~(KPageSize-1))

#define PDE_FOR_PROCESS(p) (((uintptr)(p)) | 0x00200000) // Whee for crazy hacks
#define PT_FOR_PROCESS(p, sectionNum) ((uint32*)(KProcessPtBase | (indexForProcess(p) << KSectionShift) | (sectionNum << KPageShift)))

// masks off all bits except those that distinguish one Process from another
#define MASKED_PROC_PTR(p) (((uintptr)(p)) & 0x000FF000)

#define KERN_PT_FOR_PROCESS_PTS(p) (KKernPtForProcPts | MASKED_PROC_PTR(p))

typedef struct PageAllocator PageAllocator;

void mmu_init();
void mmu_enable(uintptr returnAddr);
void mmu_mapSect0Data(uintptr virtualAddress, uintptr physicalAddress, int size);

/*
 * Maps 1MB of physically contiguous memory in the top-level PDE at the given virtual address.
 */
uintptr mmu_mapSectionContiguous(PageAllocator* pa, uintptr virtualAddress, uint8 type);


/*
 * Let 'PTS' be the section where the page table for the new section is going to go.
 * This function does 3 things.
 * 1) It allocates a physical page to be a page table, and maps this into PTS at pteAddr (using PTS's page table, 'ptsPt').
 * 2) It tells the top-level PDE that virtualAddress is a 1MB section defined by the newly-allocated
 *    page table.
 * 3) zeros the new page table so all of the section starts out unmapped
 */

bool mmu_mapSection(PageAllocator* pa, uintptr sectionAddress, uintptr ptAddress, uint32* ptsPt);

/*
 * Convenience function to create a new section whose page table is in section zero and is called
 * '<sectionName>_pt'.
 */
#define mmu_createSection(pa, sectionName) \
	mmu_mapSection(pa, sectionName, sectionName ## _pt, (uint32*)KSectionZeroPt)

/*
 * Allocates a new physical page with the specified type, and maps it into the section whose page
 * table is located at 'pt', giving the page the virtual address 'virtualAddress'. virtualAddress
 * really better point into the section given by pt otherwise BAD THINGS will happen.
 * Returns the physical address of the new page
 */
uintptr mmu_mapPageInSection(PageAllocator* pa, uint32* pt, uintptr virtualAddress, uint8 type);

bool mmu_createUserSection(PageAllocator* pa, Process* p, int sectionIdx);

/*
 * Map pages into the user process p. Will call mmu_createUserSection if necessary.
 */
bool mmu_mapPagesInProcess(PageAllocator* pa, Process* p, uintptr virtualAddress, int numPages);

bool mmu_mapKernelPageInProcess(Process* p, uintptr physicalAddress, uintptr virtualAddress, bool readWrite);

/*
 * Pages need not be in same section, although behaviour is
 * undefined if any of the PTs involved don't actually exist
 */
void mmu_unmapPagesInProcess(PageAllocator* pa, Process* p, uintptr virtualAddress, int numPages);

void mmu_finishedUpdatingPageTables();

/*
 * Sets TTBR0 and contextId register to the values appropriate for process p. Also sets
 * TheSuperPage->currentProcess to p.
 */
void switch_process(Process* p);

#endif
