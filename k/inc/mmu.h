#ifndef MMU_H
#define MMU_H

#include <k.h>

/*
Physical			Virtual

RW
Vectors		00000000-00000040						(preset by pi to all jump to 0x8000)
PageAlloctr	00020000-00041000	F0020000-F0041000	(132kB)
User PDEs	00100000-00200000	F0100000-F0200000	(1MB)
Processes	00200000-00300000	F0200000-F0300000	(1MB)
Kern PDEs	00300000-00400000	F0300000-F0400000	(1MB)

Lua heap	00400000-...?							TEMPORARY!


Boring		00000000-00008000	- 					(ATAGS, earlyboot stack, 32k)


CodeAndStuffSection:
SelfPte		00005000-00006000	F8000000-F8001000	(4k)
Unused							F8001000-F8004000	(12k)
Guard		-----------------	F8004000-F8005000	(4k)
Kern stack	00006000-00008000	F8005000-F8007000	(8k)
Guard		-----------------	F8007000-F8008000	(4k)
Code		00008000-00020000	F8008000-F8020000	(96k)

*/

#define KPageShift 12
#define KOneMegShift 20
#define MB *1024*1024
#define KAddrToPdeIndexShift KOneMegShift
#define KAddrToPdeAddrShift (KAddrToPdeIndexShift - 2)
#define KPageTableSize		4096
#define KSectionMask		0x000FFFFFu

#define KPhysicalPdeBase	0x00300000u
#define KKernelPdeBase		0xF0300000u

#define KCodeAnStuffSection 0xF8000000u
#define KPhysicalStuffPte	0x00005000u

#define KPhysicalStackBase	0x00006000u
#define KKernelStackBase	0xF8005000u
#define KKernelStackSize	0x00002000u // 8kB

#define KPhysicalCodeBase	0x00008000u
#define KKernelCodeBase		0xF8008000u
#define KKernelCodesize		0x00018000

//////

#define KNumPhysicalRamPages (KPhysicalRamSize >> KPageShift)


#endif
