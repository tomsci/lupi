#ifndef LUPI_BUILD_TILDA_H
#define LUPI_BUILD_TILDA_H

#define THUMB
#define THUMB2
#define ARMV7_M
#define CORTEX_M3

#define ARCH_HEADER <armv7-m.h>

#define LUPI_NO_SECTION0 // Implies packed superpage and no MMU translation

#define LUPI_NO_IPC // This is a temporary macro

#define TILDA_USE_USART0

#define HAVE_SCREEN
#define HAVE_MPU
#define HAVE_AUDIO

#ifdef MALLOC_AVAILABLE
#define LUPI_USE_MALLOC_FOR_KLUA
#endif

//#define MCLK		84000000

#define PIO_PER		0x00 // PIO enable
#define PIO_PDR		0x04 // PIO disable
#define PIO_PSR		0x08 // PIO status
#define PIO_OER		0x10 // Output enable
#define PIO_ODR		0x14 // Output disable
#define PIO_IFER	0x20 // Input filter enable
#define PIO_IFDR	0x24 // Input filter disable
#define PIO_SODR	0x30 // Set output (1)
#define PIO_CODR	0x34 // Clear output (0)
#define PIO_PDSR	0x3C // Pin data status
#define PIO_IER		0x40 // Interrupt enable
#define PIO_IDR		0x44 // Interrupt disable
#define PIO_IMR		0x48 // Interrupt mask
#define PIO_ISR		0x4C // Interrupt status
#define PIO_PUDR	0x60 // Pull-up disable
#define PIO_PUER	0x64 // Pull-up enable
#define PIO_ABSR	0x70 // Peripheral A/B select
#define PIO_DIFSR	0x84 // Debouncing Input Filter Select
#define PIO_SCDR	0x8C // Slow Clock Divider Debouncing Reg
#define PIO_OWER	0xA0 // Output write enable
#define PIO_ESR		0xC0 // Edge select register
#define PIO_LSR		0xCC // Level select register

// Aliases because I keep screwing up PER/PDR
#define PIO_ENABLE			PIO_PER
#define PERIPHERAL_ENABLE	PIO_PDR

// p656
#define PIOA		0x400E0E00
#define PIOB		0x400E1000
#define PIOC		0x400E1200
#define PIOD		0x400E1400

// p557
#define PMC_PCER0	0x400E0610 // PMC Peripheral Clock Enable Register 0
#define PMC_PCDR0	0x400E0614 // PMC Peripheral Clock Disable Register 0
#define PMC_PCSR0	0x400E0618 // PMC Peripheral Clock Status Register 0
#define PMC_MCKR	0x400E0630 // Master clock register
#define PMC_SR		0x400E0668
#define PMC_PCER1	0x400E0700 // PMC Peripheral Clock Enable Register 1
#define PMC_PCDR1	0x400E0704 // PMC Peripheral Clock Disable Register 1
#define PMC_PCSR1	0x400E0708 // PMC Peripheral Clock Status Register 1

// p47
#define PERIPHERAL_ID_UART		8
#define PERIPHERAL_ID_SMC_SDRAMC 9
#define PERIPHERAL_ID_PIOA		11
#define PERIPHERAL_ID_PIOB		12
#define PERIPHERAL_ID_PIOC		13
#define PERIPHERAL_ID_PIOD		14
#define PERIPHERAL_ID_USART0	17
#define PERIPHERAL_ID_SPI0		24
#define PERIPHERAL_ID_TC0		27
#define PERIPHERAL_ID_PWM		36
#define PERIPHERAL_ID_DACC		38

// Peripheral DMA Controller (PDC) p524
#define PERIPH_PTCR		0x120
#define PERIPH_RXTDIS	(1 << 1)
#define PERIPH_TXTDIS	(1 << 9)

#define LED_TX (1 << 21) // PA21, Output
#define LED_RX (1 << 30) // PC30, Output

#define GPIO_LED_TX (PIOA | 21)
#define GPIO_LED_RX (PIOC | 30)

#define KCrashedPrioritySvc	(0x1 << 4)
#define KCrashedBasePri		(0x2 << 4)
#define KPriorityAudio		(0x3 << 4)
#define KPrioritySysTick	(0x4 << 4)
#define KPriorityPeripheral	(0x8 << 4)
#define KPriorityWfiPendsv	(0x9 << 4)
#define KPrioritySvc		(0xA << 4)

// See p37 for overall SAM3X/A memory mappings

/**
## Kernel memory map

<pre>
Code (flash)	00080000-000C0000	(256k)

Handler stack	20086000-20087000	(4k)
SuperPage		20087000-20088000	(4k)

</pre>
*/

// SRAM bank0 base is actually 0x20000000 but
// this addr gives us both banks contiguously
#define KPhysicalRamBase		0x20070000
#define KRamBase				KPhysicalRamBase
#define KRamSize				(96 * 1024)

#define KKernelCodeBase			0x00080000
#define KKernelCodesize			0x00080000
#define KLogKernelCodesize		(19)

#define KSuperPageAddress		0x20087000
#define KHandlerStackBase		0x20086000

// NAND Flash Controller has 4KB of RAM we can steal
#define KNfcRamBase				0x20100000

/**
## User memory map

<pre>
------------					00000000-20070000
Heap							20070000-heapLimit
Thread stacks					????????-20086000
------------					20086000-20087DE0
User BSS						20087DE0-20088000 (640 B)
------------					20088000-22E00000
Bit-banded user mem				22E00000-230C0000
------------					230C0000-FFFFFFFF
</pre>
*/

#define KUserBssSize			0x220
#define KSuperPageUsrRegionSize 0x280
#define KUserBss				0x20087DE0

#define KUserHeapBase			0x20070000
#define KLuaHeapBase			(KUserHeapBase)

#define KUserMemLimit			KHandlerStackBase

#define USER_STACK_SIZE			(4096)
#define USER_STACK_AREA_SHIFT	(12)

#define LoadSuperPageAddress(reg) \
	asm("MOV " #reg ", %0" : : "i" (0x20000000)); \
	asm("ADD " #reg ", %0" : : "i" (0x87000))

// #define TIMER_DEBUG

#endif // LUPI_BUILD_TILDA_H
