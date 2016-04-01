#ifndef LUPI_BUILD_PI3_H
#define LUPI_BUILD_PI3_H

#include <memmap.h>

#define AARCH64 // Architecture, cf ARMV6 and ARMV7-M
#define A64 // Instruction set, cf ARM and THUMB2
#define BCM2837

#define ARCH_HEADER <aarch64.h>

// #define HAVE_SCREEN
// #define HAVE_PITFT
#define HAVE_MMU

// #define NON_SECURE // Ie we do drop to NS mode
// #define ENABLE_DCACHE
// #define ICACHE_IS_STILL_BROKEN

#define KPeripheralPhys		0x20000000ul
// #define KPeripheralSize		0x00300000ul

#define KPhysicalRamBase	0x00000000ul
// // Available RAM is read from ATAGS (it varies depending on the GPU config)

#define KSystemClockFreq	250000000 // 250 MHz

#define KPeripheralBase	KPeripheralPhys
// #define KPeripheralBase		0xF2000000ul
// //#define KTimerBase		0xF2003000

// #if defined(KLUA)
// #define KLuaHeapBase		0x00200000
// #endif

// See BCM-2835-ARM-Peripherals p8
#define AUX_ENABLES		(KPeripheralBase + 0x00215004)
#define AUX_MU_IO_REG	(KPeripheralBase + 0x00215040)
#define AUX_MU_IER_REG	(KPeripheralBase + 0x00215044)
#define AUX_MU_IIR_REG	(KPeripheralBase + 0x00215048)
#define AUX_MU_LCR_REG	(KPeripheralBase + 0x0021504C)
#define AUX_MU_MCR_REG	(KPeripheralBase + 0x00215050)
#define AUX_MU_LSR_REG	(KPeripheralBase + 0x00215054)
#define AUX_MU_MSR_REG	(KPeripheralBase + 0x00215058)
#define AUX_MU_SCRATCH	(KPeripheralBase + 0x0021505C)
#define AUX_MU_CNTL_REG	(KPeripheralBase + 0x00215060)
#define AUX_MU_STAT_REG	(KPeripheralBase + 0x00215064)
#define AUX_MU_BAUD_REG	(KPeripheralBase + 0x00215068)


// See BCM-2835-ARM-Peripherals p112
#define IRQ_BASIC			(KPeripheralBase + 0xB200)
#define IRQ_PEND1			(KPeripheralBase + 0xB204)
#define IRQ_PEND2			(KPeripheralBase + 0xB208)
#define IRQ_FIQ_CONTROL		(KPeripheralBase + 0xB20C)
#define IRQ_ENABLE_1		(KPeripheralBase + 0xB210)
#define IRQ_ENABLE_2		(KPeripheralBase + 0xB214)
#define IRQ_ENABLE_BASIC	(KPeripheralBase + 0xB218)
#define IRQ_DISABLE_1		(KPeripheralBase + 0xB21C)
#define IRQ_DISABLE_2		(KPeripheralBase + 0xB220)
#define IRQ_DISABLE_BASIC	(KPeripheralBase + 0xB224)

#define AUX_INT				29
#define GPIO0_INT			49
#define GPIO1_INT			50
#define GPIO2_INT			51
#define GPIO3_INT			52
#define UART_INT			57

// BCM-2835-ARM-Peripherals p12 says you set bit 1 to enable receive. It's actually bit 0.
#define AUX_MU_EnableReceiveInterrupt	(1 << 0)
#define AUX_MU_ClearReceiveFIFO			(1 << 1)
#define AUX_MU_ClearTransmitFIFO		(1 << 2)
#define AUX_MU_IIR_ReceiveInterrupt		(1 << 2)

#define AUXENB_MiniUartEnable			1
#define AUXENB_Spi1Enable				2
#define AUXENB_Spi2Enable				4

#endif // LUPI_BUILD_PI3_H
