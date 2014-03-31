#ifndef PAGEALLOCATOR_H
#define PAGEALLOCATOR_H

#define KPageFree 0 // must be zero
#define KPageUsed 1 // TODO more info needed here I think!

typedef struct PageAllocator {
	int numPages;
	int firstFreePage;
	uint8 pageInfo[1]; // Extends beyond struct, up to numPages
} PageAllocator;

void pageAllocator_init(PageAllocator* allocator, int numPages);
uintptr pageAllocator_allocAligned(PageAllocator* allocator, uint8 type, int num, int alignment);
void pageAllocator_doFree(PageAllocator* allocator, int idx, int num);


// Returns the size in bytes of a PageAllocator object that is configured to track numPages's worth
// of pages.
#define pageAllocator_size(numPages) (offsetof(PageAllocator, pageInfo) + numPages)

static inline uintptr pageAllocator_alloc(PageAllocator* allocator, uint8 type, int num) {
	return pageAllocator_allocAligned(allocator, type, num, 0);
}


#endif
