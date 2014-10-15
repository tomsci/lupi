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

</pre>
*/

#define KHandlerStackBase		0x20071000
#define KKernelCodeBase			0x00080000
#define KKernelCodesize			0x00040000
#define KLogKernelCodesize		(18)

// Note this is an alias for 20070000 - we use it because
// 20000000 can be loaded into a register in one instruction
#define KSuperPageAddress		0x20000000u

#define KEndOfKernelMemory		0x20072000u


/**
User memory map
---------------

<pre>
Inaccessible kernel stuff		00000000-20072000
Heap							20072000-heapLimit
Thread stacks					Downwards from KUserMemLimit
User BSS						20087DE0-20088000

Inaccessible					20088000-FFFFFFFF
</pre>
*/

#define KUserBss				0x20087DE0
#define KUserHeapBase			0x20072000

#define KUserMemLimit			(KRamBase + KRamSize)

#define USER_STACK_SIZE			(KPageSize)
#define USER_STACK_AREA_SHIFT	(KPageShift)

#endif
