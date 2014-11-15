#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>


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

Currently no effort is made to coelesce adjacent free cells. This would be
difficult in the current implementation because the free list is not ordered by
address but by the order in which the memory was freed, meaning it would be
O(N^2) to coelesce the whole free list.
*/


// #define DEBUG_LOGGING
// #define VERBOSE_LOGGING

#ifdef DEBUG_LOGGING
void printk(const char* fmt, ...) ATTRIBUTE_PRINTF(1, 2);
#define DBG(args...) printk(args)
#define ASSERT_DBG(cond, args...) do { if (!(cond)) { printk(args); abort(); } } while(0)
#else
#define DBG(args...)
#define DBGV(args...)
#define ASSERT_DBG(cond, args...)
#endif // DEBUG_LOGGING

#ifdef VERBOSE_LOGGING
#define DBGV(args...) printk(args)
#else
#define DBGV(args...)
#endif

void* sbrk(ptrdiff_t inc);
int memStats_lua(lua_State* L);


// Free cells are always aligned to an 8-byte boundary and are always a min of
// 8 bytes long

typedef struct FreeCell FreeCell;
struct FreeCell {
	FreeCell* next;
	int len;
};

typedef struct Heap {
	FreeCell* topCell; // Always last in freeList. May be null if we've filled the heap
	FreeCell* freeList;
	int totalAllocs;
	int totalFrees;
} Heap;


#define align(ptr) ((((uintptr)(ptr)) + 0x7) & ~0x7)

void* uluaHeap_init() {
	Heap* h = (Heap*)sbrk(4096);
	h->totalAllocs = 0;
	h->totalFrees = 0;
	h->topCell = (FreeCell*)align(h+1);
	h->topCell->next = 0;
	h->topCell->len = (uintptr)h + 4096 - (uintptr)h->topCell;
	h->freeList = h->topCell;

	DBG("Heap reset %p\n", h);
	return h;
}

// Returns the new cell for the region no longer covered by cell
// cellNewSize must be 8-byte aligned
static FreeCell* shrinkCell(FreeCell* cell, int cellNewSize) {
	int remainder = cell->len - cellNewSize;
	FreeCell* cellNext = cell->next;
	FreeCell* new = (FreeCell*)((uintptr)cell + cellNewSize);
	cell->next = new;
	new->next = cellNext;
	cell->len = cellNewSize;
	new->len = remainder;
	return new;
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
			cell->len = osize;
			cell->next = h->freeList;
			h->freeList = cell;
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
			newCell->next = h->freeList;
			int newLen = osize - nsize;
			newCell->len = newLen;
			h->freeList = newCell;
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
	// go with worst-fit.
	FreeCell* found = NULL;
	FreeCell* foundPrev = (FreeCell*)&h->freeList;
	FreeCell* prev = foundPrev;
	for (FreeCell* fc = h->freeList; fc != NULL; fc = fc->next) {
		if (fc->len == nsize) {
			found = fc;
			foundPrev = prev;
			break;
		} else if (fc->len >= nsize && (!found || fc->len > found->len)) {
			found = fc;
			foundPrev = prev;
		}
		if (!found && !fc->next) {
			ASSERT_DBG(h->topCell == NULL || fc == h->topCell, "Last cell %p not topCell %p\n", fc, h->topCell);
			break;
		}
		prev = fc;
	}

	if (found) {
		int remainder = found->len - nsize;
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
			DBG("topCell now %p, last freeCell prior %p\n", h->topCell, foundPrev);
		}
		h->totalAllocs++;
		DBGV("Alloc found cell %p len=%d remainder=%d\n", found, found->len, remainder);
		return found;
	}

	// Need to expand
	int growBy = max(4096, nsize - (h->topCell ? h->topCell->len : 0));
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
#endif
		return NULL;
	}
	if (!h->topCell) {
		DBG("Reinstating topCell %p\n", topPtr);
		h->topCell = (FreeCell*)topPtr;
		h->topCell->next = 0;
		h->topCell->len = 0;
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
	h->topCell->len += growBy;
	void* result = h->topCell;
	FreeCell* newTop = shrinkCell(h->topCell, nsize);
	// If we reach here, prev->next should be pointing to the topCell, unless the topCell was null
	h->topCell = newTop;
	prev->next = newTop;
	DBG("Alloc grew heap by %d and returned %p newTop=%p len=%d\n", growBy, result, newTop, newTop->len);
	return result;
}

#ifdef DEBUG_LOGGING
// Then use more direct mechanism that doesn't need to allocate
#define MEMSTAT_PRINT(fmt, args...) printk(fmt "\n", args)
#else
#define MEMSTAT_PRINT(fmt, args...) PRINTL(fmt, args)
#endif

int memStats_lua(lua_State* L) {
	// lua_gc(L, LUA_GCCOLLECT, 0);
	Heap* h = (Heap*)0x20070000;

	int freeSpace = 0;
	int nfreeCells = 0;
	int largestFreeCell = 0;
	for (FreeCell* fc = h->freeList; fc != NULL; fc = fc->next) {
		freeSpace += fc->len;
		nfreeCells++;
		if (fc->len > largestFreeCell) largestFreeCell = fc->len;
	}
	uintptr top = (uintptr)sbrk(0);
	int used = top - (uintptr)h;
	int alloced = used - freeSpace - sizeof(Heap);

	MEMSTAT_PRINT("total counts: allocs = %d frees = %d", h->totalAllocs, h->totalFrees);
	MEMSTAT_PRINT("free: cells = %d space = %d largestCell = %d", nfreeCells, freeSpace, largestFreeCell);
	MEMSTAT_PRINT("used: alloced = %d total = %d", alloced, used);
	if (L) {
		int luaAlloced = lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);
		MEMSTAT_PRINT("Lua: alloced = %d", luaAlloced);
	}
	return 0;
}
