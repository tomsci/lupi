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
#define KPageSharedPage 9
#define KPageNumberOfTypes 10

typedef struct PageAllocator {
	int numPages;
	int firstFreePage;
	// Don't add anything here without also updating function pageStats()
	uint8 pageInfo[1]; // Extends beyond struct, up to numPages
} PageAllocator;

void pageAllocator_init(PageAllocator* allocator, int numPages);

/**
Allocates `num` pages of physical memory, and assigns them the given `type`,
which must be one of the `KPageXxx` variables defined above.

*Note:* This function does not actually change any page tables, it merely marks
the pages as in use, such that a subsequent call to `pageAllocator_alloc` will
not return them.

Returns the physical address of the pages, or zero if there was insufficient
contiguous free memory.
*/
#define pageAllocator_alloc(allocator, type, num) \
	pageAllocator_allocAligned(allocator, type, num, 0)

/**
Like [pageAllocator_alloc()](#pageAllocator_alloc) but also takes an
`alignment` parameter which specifies a an alignment constraint on the desired
memory. For example to allocate memory that must be on a 1MB boundary, you'd
pass in `1024*1024` as the alignment.
*/
uintptr pageAllocator_allocAligned(PageAllocator* allocator, uint8 type, int num, int alignment);
void pageAllocator_free(PageAllocator* pa, uintptr addr);
int pageAllocator_pagesInUse(PageAllocator* allocator);

/**
Returns the size in bytes of a PageAllocator object that is configured to track
numPages's worth of pages.
*/
#define pageAllocator_size(numPages) (offsetof(PageAllocator, pageInfo) + numPages)

#endif
