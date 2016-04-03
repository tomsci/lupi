#ifndef MEMMAP_H
#define MEMMAP_H

/**
## Kernel memory map

<pre>
Section Zero:
SuperPage						F8000000-F8001000	(4k)
Sect0 PT	00003000-00004000	F8001000-F8002000	(4k)
KProcessesSection_pt			F8002000-F8003000	(4k)
KProcessesPdeSection_pt			F8003000-F8004000	(4k)
KKernelPdeB	00088000-0008C000	F8004000-F8008000	(16k)
Code		00008000-00088000	F8008000-F8088000	(512k)
Guard		-----------------	F8088000-F8089000	(4k)
Abort stack	00004000-00005000	F8089000-F808A000	(4k)
Guard		-----------------	F808A000-F808B000	(4k)
IRQ stack	00005000-00006000	F808B000-F808C000	(4k)
Guard		-----------------	F808C000-F808D000	(4k)
Kern stack	00006000-00007000	F808D000-F808E000	(8k)
Guard		-----------------	F808E000-F808F000	(4k)
Unused		-----------------	F808F000-F8090000	(4k)
KKernPtForProcPts_pt			F8090000-F8091000	(4k)
atags		00000000-00001000	F8091000-F8092000	(12k)
KDfcThreadStack					F8092000-F8093000	(4k)
Unused		-----------------	F8093000-F80C0000
PageAlloctr	0008C000-dontcare	F80C0000-F8100000	(256k)
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


#define KSectionZero			0xF8000000ul

#define KPhysicalSect0Pt		0x00003000ul
#define KSectionZeroPt			0xF8001000ul

#define KPhysicalPdeBase		0x00088000ul
#define KKernelPdeBase			0xF8004000ul

#define KPhysicalStackBase		0x00006000ul
#define KKernelStackBase		0xF808D000ul
#define KKernelStackSize		0x00001000ul // 4kB

#define KPhysicalCodeBase		0x00008000ul
#define KKernelCodeBase			0xF8008000ul
#define KKernelCodesize			0x00080000ul

#define KPhysicalAbortStackBase	0x00004000ul
#define KPhysicalIrqStackBase	0x00005000ul
#define KTemporaryIdMappingPt	KPhysicalIrqStackBase

#define KAbortStackBase			0xF8089000ul
#define KIrqStackBase			0xF808B000ul

#define KPhysPageAllocator		0x0008C000ul
#define KPageAllocatorAddr		0xF80C0000ul
#define KPageAllocatorMaxSize	0x00040000ul

#define KKernelAtagsBase		0xF8091000ul
#define KDfcThreadStack			0xF8092000ul

#ifndef KSuperPageAddress
#define KSuperPageAddress		0xF8000000ul
#endif

// Other sections and their page tables

#define KProcessesSection		0xF8100000ul
#define KProcessesSection_pt	0xF8002000ul

#define KProcessesPdeSection	0xF8300000ul
#define KProcessesPdeSection_pt	0xF8003000ul

#define KKernPtForProcPts		0xF8200000ul
#define KKernPtForProcPts_pt	0xF8090000ul

#define KProcessPtBase			0x90000000ul

#define KLuaDebuggerSection		0x42000000ul
#define KLuaDebuggerStackBase	0x42000000ul
#define KLuaDebuggerStackSize	0x00004000ul
#define KLuaDebuggerSvcStackBase 0x42004000ul
#define KLuaDebuggerSvcStackSize 0x00001000ul
#define KLuaDebuggerSectionHeap	0x42005000ul

/**
## User memory map

<pre>
Unmapped						00000000-00007000
BSS								00007000-00008000
Heap							00008000-heapLimit
Shared pages					0F000000-0F100000
Thread stacks					0FE00000-10000000
</pre>
*/

#define KUserBss				0x00007000ul
// Heap assumed to be immediately following BSS in process_init()
#define KUserHeapBase			0x00008000ul
// Note these next two are also defined in usersrc/ipc.c
#define KSharedPagesBase		0x0F000000ul
#define KSharedPagesSize		0x00100000ul
#define KUserStacksBase			0x0FE00000ul
#define KUserMemLimit			0x10000000ul

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

#define LoadSuperPageAddress(reg) asm("MOV " #reg ", %0" : : "i" (KSuperPageAddress))
#endif
