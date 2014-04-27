#ifndef PAGEALLOCATOR_H
#define PAGEALLOCATOR_H

#define KPageFree 0 // must be zero
#define KPageSect0 1
#define KPageAllocator 2
#define KPageProcess 3
#define KPageUserPde 4
#define KPageUserPt 5
#define KPageUser 6
#define KPageKluaHeap 7
#define KPageKernPtForProcPts 8
#define KPageNumberOfTypes 9

typedef struct PageAllocator {
	int numPages;
	int firstFreePage;
	// Don't add anything here without also updating funciton pageStats()
	uint8 pageInfo[1]; // Extends beyond struct, up to numPages
} PageAllocator;

void pageAllocator_init(PageAllocator* allocator, int numPages);
uintptr pageAllocator_allocAligned(PageAllocator* allocator, uint8 type, int num, int alignment);
void pageAllocator_free(PageAllocator* pa, uintptr addr);
int pageAllocator_pagesInUse(PageAllocator* allocator);

// Returns the size in bytes of a PageAllocator object that is configured to track numPages's worth
// of pages.
#define pageAllocator_size(numPages) (offsetof(PageAllocator, pageInfo) + numPages)

#define pageAllocator_alloc(allocator, type, num) pageAllocator_allocAligned(allocator, type, num, 0)

#endif
