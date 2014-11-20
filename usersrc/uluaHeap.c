#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <lua.h>

#include <lupi/uluaHeap.h>

/**
A simple allocator that is heavily biased towards minimal memory footprint. It
relies on the fact that Lua keeps track of the size of all its objects itself,
so there is no need for the allocator to add headers to keep track of allocation
sizes. This reduces RAM usage by roughly 5% on the TiLDA, at the cost of not
being able to allocate memory outside of Lua without manually including length
information in the allocated cell itself. Since we don't do that anyway it's not
much of a restriction.

This allocator keeps a singly-linked list of free cells, each of which is a
minimum of 8 bytes long. An allocation request is satisfied by iterating the
free list for either: a cell which exactly matches the requested size (aligned
to 8 bytes); or for the largest free cell ("worst fit") which is then split and
the remainder retained on the free list. If no suitable free cell can be found,
a call to `sbrk()` is made to allocate an additional page of memory from the
kernel.

The free list is kept in address order, so that free cells can be coelesced
whenever a cell is freed.
*/


// #define DEBUG_LOGGING
// #define VERBOSE_LOGGING

#ifdef DEBUG_LOGGING
#define DBG(args...) printf(args)
#define ASSERT_DBG(cond, args...) do { if (!(cond)) { printf(args); abort(); } } while(0)
#else
#define DBG(args...)
#define DBGV(args...)
#define ASSERT_DBG(cond, args...)
#endif // DEBUG_LOGGING

#ifdef VERBOSE_LOGGING
#define DBGV(args...) printf(args)
#else
#define DBGV(args...)
#endif

void* sbrk(ptrdiff_t inc);
int memStats_lua(lua_State* L);


// Free cells are always aligned to an 8-byte boundary and are always a min of
// 8 bytes long
struct FreeCell {
	FreeCell* next;
	int __len;
};
ASSERT_COMPILE(sizeof(FreeCell) == 8);

#define setLen(fc, len) (fc)->__len = (len)
#define getLen(fc) ((fc)->__len)

#ifdef DEBUG_LOGGING
static void dumpFreeList(Heap* h) {
	for (FreeCell* fc = h->freeList; fc != NULL; fc = fc->next) {
		DBG("%08lX-%08lX len %d\n", (uintptr)fc, (uintptr)fc + getLen(fc), getLen(fc));
	}
}
#endif

/**
* If `b` is `NULL`, sets `h->topCell` to `a`.
*/
static inline void link(Heap* h, FreeCell* a, FreeCell* b) {
	if (b) ASSERT_DBG(a < b, "Freecell %p not < %p!", a, b);
	a->next = b;

	if (!b) {
		DBGV("topCell was %p now %p\n", h->topCell, a);
		h->topCell = a;
	}
}

#define align(ptr) ((((uintptr)(ptr)) + 0x7) & ~0x7)

void* uluaHeap_init() {
	Heap* h = (Heap*)sbrk(4096);
	h->totalAllocs = 0;
	h->totalFrees = 0;
	h->topCell = (FreeCell*)align(h+1);
	h->topCell->next = 0;
	setLen(h->topCell, (uintptr)h + 4096 - (uintptr)h->topCell);
	h->freeList = h->topCell;

	DBG("Heap init %p\n", h);
	return h;
}

// Returns the new cell for the region no longer covered by cell
// cellNewSize must be 8-byte aligned
static FreeCell* shrinkCell(FreeCell* cell, int cellNewSize) {
	int remainder = getLen(cell) - cellNewSize;
	FreeCell* cellNext = cell->next;
	FreeCell* new = (FreeCell*)((uintptr)cell + cellNewSize);
	cell->next = new;
	new->next = cellNext;
	setLen(cell, cellNewSize);
	setLen(new, remainder);
	return new;
}

static void addToFreeList(Heap* h, FreeCell* cell) {
	// Find the freeCell immediately before where this should go
	FreeCell* prev = (FreeCell*)&h->freeList;
	for (FreeCell* fc = h->freeList; fc != NULL; fc = fc->next) {
		if (fc > cell) {
			break;
		}
		prev = fc;
	}

	FreeCell* next = prev->next;
	link(h, prev, cell);
	link(h, cell, next);

	// Now check if we can coelsce cell with either its prev or its next
	if (prev && ((uintptr)prev + getLen(prev) == (uintptr)cell)) {
		DBGV("Merging cell %p with prev %p len %d\n", cell, prev, getLen(prev));
		setLen(prev, getLen(prev) + getLen(cell));
		link(h, prev, next);
		cell = prev;
	}
	if (next && ((uintptr)cell + getLen(cell) == (uintptr)next)) {
		DBGV("Merging cell %p len %d with next %p\n", cell, getLen(cell), next);
		setLen(cell, getLen(cell) + getLen(next));
		link(h, cell, next->next);
	}
}

void* uluaHeap_allocFn(void *ud, void *ptr, size_t osize, size_t nsize) {
	// We always return 8-byte aligned pointers and sizes whatever Lua thinks internally
	nsize = (nsize + 7) & ~7;
	osize = (osize + 7) & ~7;

	Heap* h = (Heap*)ud;
	if (nsize == 0) {
		if (ptr) {
			DBGV("Freeing %p len %ld\n", ptr, osize);
			FreeCell* cell = (FreeCell*)ptr;
			setLen(cell, osize);
			addToFreeList(h, cell);
			h->totalFrees++;
		}
		return NULL;
	}

	if (ptr) {
		DBGV("Realloc osize=%ld nsize=%ld ptr=%p\n", osize, nsize, ptr);
		if (nsize == osize) {
			return ptr;
		} else if (nsize <= osize) {
			// Shrink
			FreeCell* newCell = (FreeCell*)((uintptr)ptr + nsize);
			int newLen = osize - nsize;
			setLen(newCell, newLen);
			addToFreeList(h, newCell);
			return ptr;
		} else {
			// Grow - for now assume we can never do in place
			void* newCell = uluaHeap_allocFn(h, NULL, 0, nsize); // alloc
			if (newCell) {
				memcpy(newCell, ptr, osize);
				uluaHeap_allocFn(h, ptr, osize, 0); // free
				return newCell;
			} else {
				return NULL;
			}
		}
	}

	// alloc: find a free cell. If a traversal doesn't find an exact match,
	// go with worst-fit - although don't eat into the topCell unless absolutely necessary
	FreeCell* found = NULL;
	FreeCell* foundPrev = (FreeCell*)&h->freeList;
	FreeCell* prev = foundPrev;
	for (FreeCell* fc = h->freeList; fc != NULL; fc = fc->next) {
		if (getLen(fc) == nsize) {
			// Exact match, use it
			found = fc;
			foundPrev = prev;
			break;
		} else if (fc != h->topCell && getLen(fc) >= nsize && (!found || getLen(fc) > getLen(found))) {
			// Candidate, use it unless we find a bigger cell
			found = fc;
			foundPrev = prev;
		}
		if (!found && !fc->next) {
			ASSERT_DBG(h->topCell == NULL || fc == h->topCell, "Last cell %p not topCell %p\n", fc, h->topCell);
			// Break before we set prev, so that on exit from the loop, prev
			// will point to the cell prior to the topcell
			break;
		}
		prev = fc;
	}

	if (!found && h->topCell && getLen(h->topCell) >= nsize) {
		// Use the topCell if there wasn't anything suitable in the freeList
		found = h->topCell;
		foundPrev = prev;
	}

	if (found) {
		int remainder = getLen(found) - nsize;
		FreeCell* newCell = NULL;
		if (remainder) {
			// Split the cell
			newCell = shrinkCell(found, nsize);
			foundPrev->next = newCell;
		} else {
			foundPrev->next = found->next;
		}
		if (found == h->topCell) {
			h->topCell = newCell;
			DBGV("topCell now %p, last freeCell prior %p\n", h->topCell, foundPrev);
		}
		h->totalAllocs++;
		DBGV("Alloc found cell %p len=%d remainder=%d\n", found, getLen(found), remainder);
		return found;
	}

	// Need to expand
	int growBy = max(4096, nsize - (h->topCell ? getLen(h->topCell) : 0));
	void* topPtr = sbrk(growBy);
	// Kernel may be able to give us 2KB due to how the mem map is laid out,
	// if there are an odd number of threads (thread stacks are only 2KB)
	if (topPtr == (void*)-1) {
		growBy = 2048;
		topPtr = sbrk(growBy);
	}
	if (topPtr == (void*)-1) {
		// OOM - dump some stats
#ifdef DEBUG_LOGGING
		DBG("Failed alloc for %ld\n", nsize);
		// The lua_State arg is not used if DEBUG_LOGGING, so can be NULL
		memStats_lua(NULL);
		dumpFreeList(h); // Full dump of freelist
#endif
		return NULL;
	}
	if (!h->topCell) {
		DBG("Reinstating topCell %p\n", topPtr);
		h->topCell = (FreeCell*)topPtr;
		h->topCell->next = 0;
		setLen(h->topCell, 0);
		if (h->freeList == NULL) {
			ASSERT_DBG(prev == (void*)&h->freeList, "prev %p != h->freeList!", prev);
			h->freeList = h->topCell;
		} else {
			ASSERT_DBG(prev->next && !prev->next->next, "prev->next %p not last free cell (prev=%p)\n", prev->next, prev);
			prev->next->next = h->topCell;
			prev = prev->next;
		}
	} else {
		ASSERT_DBG(prev->next == h->topCell, "ERROR: prev->next %p != h->topCell %p\n", prev->next, h->topCell);
	}
	setLen(h->topCell, getLen(h->topCell) + growBy);
	void* result = h->topCell;
	FreeCell* newTop = shrinkCell(h->topCell, nsize);
	// If we reach here, prev->next should be pointing to the topCell, unless the topCell was null
	h->topCell = newTop;
	prev->next = newTop;
	DBG("Alloc grew heap by %d and returned %p newTop=%p len=%d\n", growBy, result, newTop, getLen(newTop));
	return result;
}

void uluaHeap_stats(Heap* h, HeapStats* stats) {
	memset(stats, 0, sizeof(HeapStats));
	stats->totalAllocs = h->totalAllocs;
	stats->totalFrees = h->totalFrees;
	for (FreeCell* fc = h->freeList; fc != NULL; fc = fc->next) {
		stats->freeSpace += getLen(fc);
		stats->numFreeCells++;
		if (getLen(fc) > stats->largestFreeCell) stats->largestFreeCell = getLen(fc);
	}
	uintptr top = (uintptr)sbrk(0);
	stats->used = top - (uintptr)h;
	stats->alloced = stats->used - stats->freeSpace - sizeof(Heap);
}

void uluaHeap_reset(Heap* h) {
	uintptr top = (uintptr)sbrk(0);
	h->totalAllocs = 0;
	h->totalFrees = 0;
	h->topCell = (FreeCell*)align(h+1);
	h->topCell->next = 0;
	setLen(h->topCell, top - (uintptr)h->topCell);
	h->freeList = h->topCell;

	DBG("Heap reset %p\n", h);
}

#define MEMSTAT_PRINT(fmt, args...) printf(fmt "\n", args)

int memStats_lua(lua_State* L) {
	// lua_gc(L, LUA_GCCOLLECT, 0);
	Heap* h = (Heap*)0x20070000;

	HeapStats stats;
	uluaHeap_stats(h, &stats);

	MEMSTAT_PRINT("total counts: allocs = %d frees = %d", stats.totalAllocs, stats.totalFrees);
	MEMSTAT_PRINT("free: cells = %d space = %d largestCell = %d", stats.numFreeCells, stats.freeSpace, stats.largestFreeCell);
	MEMSTAT_PRINT("used: alloced = %d total = %d", stats.alloced, stats.used);
	if (L) {
		int luaAlloced = lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);
		MEMSTAT_PRINT("Lua: alloced = %d", luaAlloced);
	}
	return 0;
}
