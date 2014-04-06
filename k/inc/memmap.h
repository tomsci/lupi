#ifndef MEMMAP_H
#define MEMMAP_H

// I don't think we'll be worrying about huge pages
#define KPageSize 4096
#define KPageShift 12

/*

Section Zero:
Guard		-----------------	F8000000-F8001000	(4k)
Abort stack	00004000-00005000	F8001000-F8002000	(4k)
Guard		-----------------	F8002000-F8003000	(4k)
IRQ stack	00005000-00006000	F8003000-F8004000	(4k)
Guard		-----------------	F8004000-F8005000	(4k)
Kern stack	00006000-00008000	F8005000-F8007000	(8k)
Guard		-----------------	F8007000-F8008000	(4k)
Code		00008000-00028000	F8008000-F8028000	(128k)
Kern PDEs	00028000-0002C000	F8028000-F802C000	(16k)
Sect0 PT	00003000-00004000	F802C000-F802D000	(4k)
Proc PTEs						F802D000-F802E000	(4k)
SuperPage						F802E000-F802F000	(4k)
Process PDEs section PT			F802F000-F8030000	(4k)
KKernPtForProcPts_pt			F8030000-F8040000	(4k)
Unused		-----------------	F8040000-F8080000
PageAlloctr	0002C000-dontcare	F8080000-F8100000	(512k)
-------------------------------------------------
Processes						F8100000-F8200000	(1 MB)
Kern PTs for user proc PTs		F8200000-F8300000	(1 MB)
User PDEs						F8300000-F8400000	(1 MB)
-------------------------------------------------
User PTs						90000000-A0000000	(256MB)

*/


#define KSectionZero			0xF8000000u

#define KPhysicalSect0Pt		0x00003000u
#define KSectionZeroPt			0xF802C000u

#define KPhysicalPdeBase		0x00028000u
#define KKernelPdeBase			0xF8028000u

#define KPhysicalStackBase		0x00006000u
#define KKernelStackBase		0xF8005000u
#define KKernelStackSize		0x00002000u // 8kB

#define KPhysicalCodeBase		0x00008000u
#define KKernelCodeBase			0xF8008000u
#define KKernelCodesize			0x00020000

#define KPhysicalAbortStackBase	0x00004000u
#define KPhysicalIrqStackBase	0x00005000u

#define KAbortStackBase			0xF8001000u
#define KIrqStackBase			0xF8003000u

#define KPhysPageAllocator		0x0002C000u
#define KPageAllocatorAddr		0xF8080000u

#define KSuperPageAddress		0xF802E000u

// Other sections and their page tables

#define KProcessesSection		0xF8100000u
#define KProcessesSection_pt	0xF802D000u

#define KProcessesPdeSection	0xF8300000u
#define KProcessesPdeSection_pt	0xF802F000u


#define KKernPtForProcPts		0xF8200000u
#define KKernPtForProcPts_pt	0xF8030000u

#define KProcessPtBase			0x90000000u

/*
User memory map:

Unmapped						00000000-00007000
BSS								00007000-00008000
Heap							00008000-heapLimit
Thread stacks					03E00000-40000000
*/

#define KUserBss				0x00007000u
// Heap assumed to be immediately following BSS in process_init()
#define KUserHeapBase			0x00008000u
#define KUserStacksBase			0x03E00000u

#endif
