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
#define DBG(h, args...) do { if (((uintptr)(h)->luaState & 1) == 0) { printf(args); } } while(0)
#define ASSERT_DBG(cond, args...) do { if (!(cond)) { printf(args); abort(); } } while(0)
#include <lua.h>
#include <lauxlib.h>
#else
#define DBG(h, args...)
#define DBGV(h, args...)
#define ASSERT_DBG(cond, args...)
#endif // DEBUG_LOGGING

#ifdef VERBOSE_LOGGING
#define DBGV(h, args...) DBG(h, args)
#else
#define DBGV(h, args...)
#endif

void* sbrk(ptrdiff_t inc);
int memStats_lua(lua_State* L);


// Free cells are always aligned to an 8-byte boundary and are always a min of
// 8 bytes long. On LP64 we require that the heap is in the bottom 32-bits of
// the address space
struct FreeCell {
#ifdef __LP64__
	uint32 __next;
#else
	FreeCell* next;
#endif
	int __len;
};
ASSERT_COMPILE(sizeof(FreeCell) == 8);

#ifdef __LP64__
#define setNext(fc, _next) (fc)->__next = (uint32)(uintptr)(_next)
#define getNext(fc) ((FreeCell*)(uintptr)((fc)->__next))
#else
#define setNext(fc, _next) (fc)->next = (_next)
#define getNext(fc) ((fc)->next)
#endif

#define setLen(fc, len) (fc)->__len = (len)
#define getLen(fc) ((fc)->__len)

#ifdef DEBUG_LOGGING
static void dumpFreeList(Heap* h) {
	for (FreeCell* fc = h->freeList; fc != NULL; fc = getNext(fc)) {
		DBG(h, "%08lX-%08lX len %d\n", (uintptr)fc, (uintptr)fc + getLen(fc), getLen(fc));
	}
}
#endif

/**
* If `b` is `NULL`, sets `h->topCell` to `a`.
*/
static inline void link(Heap* h, FreeCell* a, FreeCell* b) {
	if (b) ASSERT_DBG(a < b, "Freecell %p not < %p!", a, b);
	setNext(a, b);

	if (!b) {
		DBGV(h, "topCell was %p now %p\n", h->topCell, a);
		h->topCell = a;
	}
}

#define align(ptr) ((((uintptr)(ptr)) + 0x7) & ~0x7)

void* uluaHeap_init() {
	Heap* h = (Heap*)sbrk(4096);
	h->totalAllocs = 0;
	h->totalFrees = 0;
	h->topCell = (FreeCell*)align(h+1);
	setNext(h->topCell, 0);
	setLen(h->topCell, (uintptr)h + 4096 - (uintptr)h->topCell);
	h->freeList = h->topCell;
	h->luaState = NULL;

	DBG(h, "Heap init %p\n", h);
	return h;
}

// Returns the new cell for the region no longer covered by cell
// cellNewSize must be 8-byte aligned
static FreeCell* shrinkCell(FreeCell* cell, int cellNewSize) {
	int remainder = getLen(cell) - cellNewSize;
	FreeCell* cellNext = getNext(cell);
	FreeCell* new = (FreeCell*)((uintptr)cell + cellNewSize);
	setNext(cell, new);
	setNext(new, cellNext);
	setLen(cell, cellNewSize);
	setLen(new, remainder);
	return new;
}

static void addToFreeList(Heap* h, FreeCell* cell) {
	// Find the freeCell immediately before where this should go
	FreeCell* prev = (FreeCell*)&h->freeList;
	for (FreeCell* fc = h->freeList; fc != NULL; fc = getNext(fc)) {
		if (fc > cell) {
			break;
		}
		prev = fc;
	}

	FreeCell* next = getNext(prev);
	link(h, prev, cell);
	link(h, cell, next);

	// Now check if we can coelsce cell with either its prev or its next
	if (prev && ((uintptr)prev + getLen(prev) == (uintptr)cell)) {
		DBGV(h, "Merging cell %p with prev %p len %d\n", cell, prev, getLen(prev));
		setLen(prev, getLen(prev) + getLen(cell));
		link(h, prev, next);
		cell = prev;
	}
	if (next && ((uintptr)cell + getLen(cell) == (uintptr)next)) {
		DBGV(h, "Merging cell %p len %d with next %p\n", cell, getLen(cell), next);
		setLen(cell, getLen(cell) + getLen(next));
		link(h, cell, getNext(next));
	}
}

void* uluaHeap_allocFn(void *ud, void *ptr, size_t osize, size_t nsize) {
	// We always return 8-byte aligned pointers and sizes whatever Lua thinks internally
	nsize = (nsize + 7) & ~7;
	osize = (osize + 7) & ~7;

	Heap* h = (Heap*)ud;
	if (nsize == 0) {
		if (ptr) {
			DBGV(h, "Freeing %p len %ld\n", ptr, osize);
			FreeCell* cell = (FreeCell*)ptr;
			setLen(cell, osize);
			addToFreeList(h, cell);
			h->totalFrees++;
		}
		return NULL;
	}

	if (ptr) {
		DBGV(h, "Realloc osize=%ld nsize=%ld ptr=%p\n", osize, nsize, ptr);
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
	for (FreeCell* fc = h->freeList; fc != NULL; fc = getNext(fc)) {
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
		if (!found && !getNext(fc)) {
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
			setNext(foundPrev, newCell);
		} else {
			setNext(foundPrev, getNext(found));
		}
		if (found == h->topCell) {
			h->topCell = newCell;
			DBGV(h, "topCell now %p, last freeCell prior %p\n", h->topCell, foundPrev);
		}
		h->totalAllocs++;
		DBGV(h, "Alloc found cell %p len=%d remainder=%d\n", found, getLen(found), remainder);
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
#ifdef DEBUG_LOGGING
		// OOM - dump some stats
		DBG(h, "Failed alloc for %ld\n", nsize);
		// The lua_State arg is not used if DEBUG_LOGGING, so can be NULL
		memStats_lua(NULL);
		// dumpFreeList(h); // Full dump of freelist
		// Enable this to stop at a particularly large alloc
#if 0
		if ((uintptr)h->luaState > 1 && nsize > 1000) {
			lua_State* L = *(lua_State**)((uintptr)h->luaState & ~1);
			// Free it to get some space for the traceback
			uluaHeap_allocFn(h, h->luaState, 1024, 0);
			h->luaState = NULL;
			// Try and get a stacktrace
			luaL_traceback(L, L, NULL, 1);
			printf("OOM DBG: %s", lua_tostring(L, -1));
			// Now abort so we can get the C stack too
			abort();
			lua_pop(L, 1);
		}
#endif
#endif // DEBUG_LOGGING
		return NULL;
	}
	if (!h->topCell) {
		DBG(h, "Reinstating topCell %p\n", topPtr);
		h->topCell = (FreeCell*)topPtr;
		setNext(h->topCell, 0);
		setLen(h->topCell, 0);
		if (h->freeList == NULL) {
			ASSERT_DBG(prev == (void*)&h->freeList, "prev %p != h->freeList!", prev);
			h->freeList = h->topCell;
		} else {
			ASSERT_DBG(getNext(prev) && !getNext(getNext(prev)), "prev->next %p not last free cell (prev=%p)\n", getNext(prev), prev);
			setNext(getNext(prev), h->topCell);
			prev = getNext(prev);
		}
	} else {
		ASSERT_DBG(getNext(prev) == h->topCell, "ERROR: prev->next %p != h->topCell %p\n", getNext(prev), h->topCell);
	}
	setLen(h->topCell, getLen(h->topCell) + growBy);
	void* result = h->topCell;
	FreeCell* newTop = shrinkCell(h->topCell, nsize);
	// If we reach here, prev->next should be pointing to the topCell, unless the topCell was null
	h->topCell = newTop;
	setNext(prev, newTop);
	DBG(h, "Alloc grew heap by %d and returned %p newTop=%p len=%d\n", growBy, result, newTop, getLen(newTop));
	return result;
}

void uluaHeap_stats(Heap* h, HeapStats* stats) {
	memset(stats, 0, sizeof(HeapStats));
	stats->totalAllocs = h->totalAllocs;
	stats->totalFrees = h->totalFrees;
	for (FreeCell* fc = h->freeList; fc != NULL; fc = getNext(fc)) {
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
	setNext(h->topCell, 0);
	setLen(h->topCell, top - (uintptr)h->topCell);
	h->freeList = h->topCell;

	DBG(h, "Heap reset %p\n", h);
}

#define MEMSTAT_PRINT(fmt, args...) printf(fmt "\n", args)

int memStats_lua(lua_State* L) {
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

void uluaHeap_setLuaState(Heap* h, lua_State* luaState) {
#ifdef DEBUG_LOGGING
	// Reserve some heap so we can use luaState for a stacktrace in OOM
	// void* reserve = uluaHeap_allocFn(h, NULL, 0, 1024);
	// if (reserve) {
	// 	// And use the start of reserve to stash the lua_State while we're there
	// 	h->luaState = (lua_State**)reserve;
	// 	*h->luaState = luaState;
	// }
#endif
}

void uluaHeap_disableDebugPrints(Heap* h) {
	h->luaState = (lua_State**)1;
}
