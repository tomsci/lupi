#ifndef MMU_H
#define MMU_H

#include <k.h>

/*
			Physical			Virtual

RW
InitVectors	00000000-00000040						(preset by pi to all jump to 0x8000)
PageAlloctr	00020000-00041000	F0020000-F0041000	(132kB)
User PDEs	00100000-00200000	F0100000-F0200000	(1MB)
Processes	00200000-00300000	F0200000-F0300000	(1MB)
Kern PDEs	00300000-00400000	F0300000-F0400000	(1MB)

Lua heap	00400000-...?							TEMPORARY!


Boring		00000000-00008000	- 					(ATAGS, earlyboot stack, 32k)


Section Zero:

Guard		-----------------	F8000000-F8001000	(4k)
Abort stack	00004000-00005000	F8001000-F8002000	(4k)
Guard		-----------------	F8002000-F8003000	(4k)
IRQ stack	00005000-00006000	F8003000-F8004000	(4k)
Guard		-----------------	F8004000-F8005000	(4k)
Kern stack	00006000-00008000	F8005000-F8007000	(8k)
Guard		-----------------	F8007000-F8008000	(4k)
Code		00008000-00028000	F8008000-F8028000	(128k)
Kern PDE	00028000-0002C000	F8028000-F802C000	(16k)
Sect 0 PTE	00003000-00004000	F802C000-F802D000	(4k)
Unused		-----------------	F802D000-F8100000

*/

#define KPageSize 4096
#define KPageShift 12
#define KOneMegShift 20
#define MB *1024*1024
#define KAddrToPdeIndexShift KOneMegShift
#define KAddrToPdeAddrShift (KAddrToPdeIndexShift - 2)
#define KPageTableSize		4096
#define KSectionMask		0x000FFFFFu

#define PTE_IDX(virtAddr)	(((virtAddr) & KSectionMask) >> KPageShift)

#define KSectionZero		0xF8000000u

#define KPhysicalSect0Pte	0x00003000u
#define KSectionZeroPte		0xF802C000u

#define KPhysicalPdeBase	0x00028000u
#define KKernelPdeBase		0xF8028000u

#define KPhysicalStackBase	0x00006000u
#define KKernelStackBase	0xF8005000u
#define KKernelStackSize	0x00002000u // 8kB

#define KPhysicalCodeBase	0x00008000u
#define KKernelCodeBase		0xF8008000u
#define KKernelCodesize		0x00020000

#define KPhysicalAbortStackBase	0x00004000u
#define KPhysicalIrqStackBase	0x00005000u

#define KAbortStackBase		0xF8001000u
#define KIrqStackBase		0xF8003000u

//////

#define KNumPhysicalRamPages (KPhysicalRamSize >> KPageShift)


void mmu_init();
void mmu_enable(uintptr returnAddr);

#endif
