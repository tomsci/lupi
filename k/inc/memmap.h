#ifndef MEMMAP_H
#define MEMMAP_H

// I don't think we'll be worrying about huge pages
#define KPageSize 4096
#define KPageShift 12

/**
Kernel memory map
-----------------

<pre>
Section Zero:
SuperPage						F8000000-F8001000	(4k)
Sect0 PT	00003000-00004000	F8001000-F8002000	(4k)
KProcessesSection_pt			F8002000-F8003000	(4k)
KProcessesPdeSection_pt			F8003000-F8004000	(4k)
KKernelPdeB	00048000-0004C000	F8004000-F8008000	(16k)
Code		00008000-00048000	F8008000-F8048000	(256k)
Guard		-----------------	F8048000-F8049000	(4k)
Abort stack	00004000-00005000	F8049000-F804A000	(4k)
Guard		-----------------	F804A000-F804B000	(4k)
IRQ stack	00005000-00006000	F804B000-F804C000	(4k)
Guard		-----------------	F804C000-F804D000	(4k)
Kern stack	00006000-00007000	F804D000-F804E000	(8k)
Guard		-----------------	F804E000-F804F000	(4k)
Unused		-----------------	F804F000-F8050000	(4k)
KKernPtForProcPts_pt			F8050000-F8051000	(4k)
atags		00000000-00001000	F8051000-F8052000	(12k)
KDfcThreadStack					F8052000-F8053000	(4k)
Unused		-----------------	F8053000-F80C0000
PageAlloctr	0004C000-dontcare	F80C0000-F8100000	(256k)
-------------------------------------------------
Processes						F8100000-F8200000	(1 MB)
Kern PTs for user proc PTs		F8200000-F8300000	(1 MB)
User PDEs						F8300000-F8400000	(1 MB)
-------------------------------------------------
User PTs						90000000-A0000000	(256MB)
-------------------------------------------------
Pi specific:
Peripherals	20000000-20300000	F2000000-F2300000	(3 MB)
</pre>
*/


#define KSectionZero			0xF8000000u

#define KPhysicalSect0Pt		0x00003000u
#define KSectionZeroPt			0xF8001000u

#define KPhysicalPdeBase		0x00048000u
#define KKernelPdeBase			0xF8004000u

#define KPhysicalStackBase		0x00006000u
#define KKernelStackBase		0xF804D000u
#define KKernelStackSize		0x00001000u // 4kB

#define KPhysicalCodeBase		0x00008000u
#define KKernelCodeBase			0xF8008000u
#define KKernelCodesize			0x00040000u

#define KPhysicalAbortStackBase	0x00004000u
#define KPhysicalIrqStackBase	0x00005000u
#define KTemporaryIdMappingPt	KPhysicalIrqStackBase

#define KAbortStackBase			0xF8049000u
#define KIrqStackBase			0xF804B000u

#define KPhysPageAllocator		0x0004C000u
#define KPageAllocatorAddr		0xF80C0000u
#define KPageAllocatorMaxSize	0x00040000u

#define KKernelAtagsBase		0xF8051000u
#define KDfcThreadStack			0xF8052000u

#define KSuperPageAddress		0xF8000000u

// Other sections and their page tables

#define KProcessesSection		0xF8100000u
#define KProcessesSection_pt	0xF8002000u

#define KProcessesPdeSection	0xF8300000u
#define KProcessesPdeSection_pt	0xF8003000u

#define KKernPtForProcPts		0xF8200000u
#define KKernPtForProcPts_pt	0xF8050000u

#define KProcessPtBase			0x90000000u

#define KLuaDebuggerSection		0x42000000u
#define KLuaDebuggerStackBase	0x42000000u
#define KLuaDebuggerSvcStackBase 0x42001000u
#define KLuaDebuggerHeap		0x42002000u

/**
User memory map
---------------

<pre>
Unmapped						00000000-00007000
BSS								00007000-00008000
Heap							00008000-heapLimit
Shared pages					0F000000-0F100000
Thread stacks					0FE00000-10000000
</pre>
*/

#define KUserBss				0x00007000u
// Heap assumed to be immediately following BSS in process_init()
#define KUserHeapBase			0x00008000u
// Note these next two are also defined in usersrc/ipc.c
#define KSharedPagesBase		0x0F000000u
#define KSharedPagesSize		0x00100000u
#define KUserStacksBase			0x0FE00000u

/**
I'm feeling generous.
*/
#define USER_STACK_SIZE (16*1024)

#define USER_STACK_AREA_SHIFT 15 // 32kB

/**
The format of each user stack area is as follows. Note the svc stack for a
thread is always 4kB, and that the area is rounded up to a power of 2 to make
calculating the svc stack address simpler.

	svc stack			1 page
	guard page			---------------
	user stack			USER_STACK_SIZE (16kB)
	guard page			---------------
	padding				--------------- (4kB)
*/

#endif
