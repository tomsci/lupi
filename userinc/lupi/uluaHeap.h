#ifndef ULUAHEAP_H
#define ULUAHEAP_H

#include <stddef.h>

typedef struct FreeCell FreeCell;
typedef struct lua_State lua_State;

typedef struct Heap {
	FreeCell* freeList;
	FreeCell* topCell; // Always last in freeList. May be null if we've filled the heap
	uint16 totalAllocs;
	uint16 totalFrees;
	lua_State** luaState; // Bottom bit also doubles as "no debug" flag
} Heap;

typedef struct HeapStats {
	int totalAllocs;
	int totalFrees;

	int numFreeCells;
	int freeSpace;
	int largestFreeCell;

	int alloced;
	int used;

} HeapStats;

void* uluaHeap_init();
void* uluaHeap_allocFn(void *ud, void *ptr, size_t osize, size_t nsize);
void uluaHeap_reset(Heap* h);
void uluaHeap_setLuaState(Heap* h, lua_State* luaState);
void uluaHeap_disableDebugPrints(Heap* h);

void uluaHeap_stats(Heap* heap, HeapStats* stats);

#endif
