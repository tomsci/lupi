#ifndef ULUAHEAP_H
#define ULUAHEAP_H

#include <stddef.h>

typedef struct FreeCell FreeCell;

typedef struct Heap {
	FreeCell* topCell; // Always last in freeList. May be null if we've filled the heap
	FreeCell* freeList;
	int totalAllocs;
	int totalFrees;
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

void uluaHeap_stats(Heap* heap, HeapStats* stats);

#endif
