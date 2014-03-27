#include <k.h>
#include <pageAllocator.h>

// firstFreePage must not be -1
// returns index of first free region that is at least num pages, or -1 if none available
// if return index is not -1, allocator->firstFreePage will be updated with the next available
// free page after index.
static int pageAllocator_findNextFreePage(PageAllocator* allocator, int num) {
	uint8* start = &allocator->pageInfo[0];
	const uint8* end = start + KNumPhysicalRamPages;
	uint8* p = start + allocator->firstFreePage;
	uint8* ffp = NULL;

	for (; p != end; p++) {
		if (*p == KPageFree) {
			if (num == 1) break; // Quick exit for common case
			if (!ffp) {
				// Even if we find out in the for loop below that the free region isn't big
				// enough, we should still update the first free page in the allocator struct
				ffp = p;
			}
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
	if (ffp == p) ffp = NULL; // We found a free page but that's the one we're using
	if (ffp == NULL) {
		for (; p != end; p++) {
			if (*p == KPageFree) {
				ffp = p;
				break;
			}
		}
	}
	allocator->firstFreePage = ffp ? (ffp - start) : -1;
	return idx;
}

// Mark an entry in the PageAllocator as in use. Does not actually do anything
// with page tables or mapping
// return the starting page number. If num > 1 then pages will be physically contiguous
int pageAllocator_doAlloc(PageAllocator* allocator, uint8 type, int num) {
	int idx;
	if (allocator->firstFreePage < 0) {
		// We knew we were OOM at the end of the last alloc
		return -1;
	} else {
		idx = pageAllocator_findNextFreePage(allocator, num);
		if (idx == -1) return idx;

		// Mark pages as used
		const int end = idx + num;
		for (int i = idx; i < end; i++) {
			allocator->pageInfo[i] = type;
		}
	}
	return idx;
}

void pageAllocator_doFree(PageAllocator* allocator, int idx, int num) {
	uint8* p = &allocator->pageInfo[idx];
	const uint8* endp = p + num;
	for (; p != endp; p++) {
		*p = KPageFree;
	}
	if (idx < allocator->firstFreePage) {
		allocator->firstFreePage = idx;
	}
}

