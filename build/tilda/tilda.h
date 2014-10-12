#ifndef LUPI_BUILD_TILDA_H
#define LUPI_BUILD_TILDA_H

#include <memmap_armv7m.h>

// See p37 for overall SAM3X/A memory mappings

#define THUMB
#define THUMB2
#define ARMV7_M
#define CORTEX_M3

// SRAM bank0 base is actually 0x20000000 but
// this addr gives us both banks contiguously
#define KPhysicalRamBase	0x20070000
#define KRamBase			KPhysicalRamBase
#define KRamSize			(96 * 1024)

// According to doc11057 SAM3x.pdf p199
#define NUM_MPU_REGIONS		8

#define NUM_EXCEPTIONS		45 // p83

#define KLuaHeapBase		(KUserHeapBase)

#define LUPI_NO_SECTION0 // Implies packed superpage and no MMU translation

#define LUPI_NO_IPC // This is a temporary macro

#define TILDA_USE_USART0

// #define HAVE_SCREEN

#define LUPI_USE_MALLOC_FOR_KLUA

//#define MCLK		84000000

#define PIO_PER		0x00 // PIO enable
#define PIO_PDR		0x04 // PIO disable
#define PIO_PSR		0x08 // PIO status
#define PIO_OER		0x10 // Output enable
#define PIO_ODR		0x14 // Output disable
#define PIO_SODR	0x30 // Set output (1)
#define PIO_CODR	0x34 // Clear output (0)
#define PIO_IDR		0x44 // Interrupt disable
#define PIO_PUDR	0x60 // Pull-up disable
#define PIO_PUER	0x64 // Pull-up enable
#define PIO_ABSR	0x70 // Peripheral A/B select
#define PIO_OWER	0xA0 // Output write enable

// p656
#define PIOA		0x400E0E00
#define PIOB		0x400E1000
#define PIOC		0x400E1200
#define PIOD		0x400E1400

// p 557
#define PMC_PCER0	0x400E0610 // PMC Peripheral Clock Enable Register 0
#define PMC_PCDR0	0x400E0614 // PMC Peripheral Clock Disable Register 0
#define PMC_PCSR0	0x400E0618 // PMC Peripheral Clock Status Register 0
#define PMC_MCKR	0x400E0630 // Master clock register
#define PMC_SR		0x400E0668

#define PERIPHERAL_ID_UART		8
#define PERIPHERAL_ID_USART0	17
#define PERIPHERAL_ID_SPI0		24

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
#define KPrioritySysTick	(0x4 << 4)
#define KPriorityPeripheral	(0x8 << 4)
#define KPriorityWfiPendsv	(0x9 << 4)
#define KPrioritySvc		(0xA << 4)

#endif
