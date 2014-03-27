#ifndef PAGEALLOCATOR_H
#define PAGEALLOCATOR_H

#include <mmu.h>

#define KPageFree 0 // must be zero
#define KPageUsed 1 // TODO more info needed here I think!

typedef struct PageAllocator {
	uint8 pageInfo[KNumPhysicalRamPages];
	int firstFreePage;
	uint8 spare[4092];
} PageAllocator;


#ifdef BCM2835
ASSERT_COMPILE((sizeof(PageAllocator) & 0xFFF) == 0);
ASSERT_COMPILE(sizeof(PageAllocator) == 132*1024); // Just for my own sanity, obviously dependant on phys mem size
#endif

int pageAllocator_doAlloc(PageAllocator* allocator, uint8 type, int num);
void pageAllocator_doFree(PageAllocator* allocator, int idx, int num);

#endif