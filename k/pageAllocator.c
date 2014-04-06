#include <k.h>
#include <pageAllocator.h>

//TODO shouldn't have to redefine this...
#define zeroPages(ptr, n) \
	for (uint32 *p = (uint32*)(ptr), *end = (uint32*)(((uint8*)ptr) + (n << KPageShift)); p != end; p++) {\
		*p = 0; \
	}

void pageAllocator_init(PageAllocator* allocator, int numPages) {
	// We're assuming the page allocator starts on a page boundary, here
	zeroPages(allocator, (pageAllocator_size(numPages) >> KPageShift));
	allocator->numPages = numPages;
}

// firstFreePage must not be -1
// returns index of first free region that is at least num pages, or -1 if none available
// if return index is not -1, allocator->firstFreePage will be updated with the next available
// free page after index.
static int pageAllocator_findNextFreePage(PageAllocator* allocator, int num, int alignment) {
	//printk("findNextFreePage num=%d align=%d\n", num, alignment);
	uint8* start = &allocator->pageInfo[0];
	const uint8* end = start + allocator->numPages - (num - 1);
	uint8* p = start + allocator->firstFreePage;
	uint8* ffp = NULL;
	const uintptr alignmentMask = (alignment >> KPageShift)-1;

	for (; p != end; p++) {
		if (*p == KPageFree) {
			if (!ffp) {
				// Even if we find out in the for loop below that the free region isn't big
				// enough, we should still update the first free page in the allocator struct
				ffp = p;
			}
			// Not the most efficient way of establishing alignment, but meh
			if (alignmentMask && ((p-start) & alignmentMask)) continue;
			if (num == 1) break; // Quick exit for common case
			const uint8* endRange = p + num;
			p++; // No point rechecking the first page, we already know it's free
			for (; p != endRange; p++) {
				if (*p) break; // If it's in use, bail
			}
			if (p == endRange) {
				// We got all the way through the check, so this is long enough
				p = p - num;
				break;
			}
		}
	}
	// Ok we have an index or we've failed
	if (p == end) {
		return -1;
	}
	const int idx = p - start;
	if (ffp == p) {
		// Most likely, we found a free page but that's the one we're going to use
		for (p = p + num; p != start + allocator->numPages; p++) {
			if (*p == KPageFree) {
				ffp = p;
				break;
			}
		}
	}
	allocator->firstFreePage = ffp ? (ffp - start) : 0;
	//printk("findNextFreePage got idx %d (%X) ffp=%d\n", idx, ((uint)idx)<<KPageShift, allocator->firstFreePage);
	return idx;
}

// Mark an entry in the PageAllocator as in use. Does not actually do anything
// with page tables or mapping. Returns the physical address.
// If num > 1 then pages will be physically contiguous
uintptr pageAllocator_allocAligned(PageAllocator* allocator, uint8 type, int num, int alignment) {
	if (alignment == 0) alignment = KPageSize;
	ASSERT(IS_POW2(alignment));

	int idx = pageAllocator_findNextFreePage(allocator, num, alignment);
	if (idx == -1) return 0;

	// Mark pages as used
	const int end = idx + num;
	for (int i = idx; i < end; i++) {
		allocator->pageInfo[i] = type;
	}

	return KPhysicalRamBase + (idx << KPageShift);
}


int pageAllocator_pagesInUse(PageAllocator* pa) {
	int result = 0;
	for (int i = 0; i < pa->numPages; i++) {
		if (pa->pageInfo[i]) result++;
	}
	return result;
}

static void pageAllocator_doFree(PageAllocator* allocator, int idx, int num) {
	uint8* p = &allocator->pageInfo[idx];
	const uint8* endp = p + num;
	for (; p != endp; p++) {
		*p = KPageFree;
	}
	if (idx < allocator->firstFreePage) {
		allocator->firstFreePage = idx;
	}
}

void pageAllocator_free(PageAllocator* pa, uintptr addr) {
	int idx = addr >> KPageShift;
	pageAllocator_doFree(pa, idx, 1);
}
