#ifndef MEMMAP_ARMV7M_H
#define MEMMAP_ARMV7M_H

// I don't think we'll be worrying about huge pages
#define KPageSize 4096
#define KPageShift 12

/**
Kernel memory map
-----------------

<pre>
Code (flash)	00080000-000C0000	(256k)

SuperPage		20070000-20071000	(4k)
Handler stack	20071000-20072000	(4k)
ProcessPage		20072000-20073000	(4k)

</pre>
*/

#define KHandlerStackBase		0x20071000u
#define KKernelCodeBase			0x00080000u
#define KKernelCodesize			0x00004000u

// Note this is an alias for 20070000 - we use it because
// 20000000 can be loaded into a register in one instruction
#define KSuperPageAddress		0x20000000u

#define KProcessesSection		0x20072000u

#define KDfcThreadStack			0xF8092000u // TODO!

#define KEndOfKernelMemory		0x20073000u


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

#define KUserBss				KEndOfKernelMemory
// Heap assumed to be immediately following BSS in process_init()
#define KUserHeapBase			(KEndOfKernelMemory + 0x1000)

//TODO fix these!
#define KUserStacksBase			0x0FE00000u
#define KUserMemLimit			0x10000000u

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
