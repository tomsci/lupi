#ifndef LUPI_BUILD_PI_H
#define LUPI_BUILD_PI_H

#include <memmap.h>

#define ARM
#define ARMV6
#define ARM1176
#define BCM2835

#define NON_SECURE // Ie we do drop to NS mode


#define KPeripheralPhys		0x20000000
#define KPeripheralSize		0x00300000
//#define KSuperPagePhys	0x00004000

#define KPhysicalRamBase	0x00000000
#define KPhysicalRamSize	0x20000000 // 512MB - we're assuming model B atm

#define KSystemClockFreq	250000000 // 250 MHz

//#define KPeripheralBase	KPeripheralPhys
#define KPeripheralBase		0xF2000000
//#define KTimerBase		0xF2003000

#ifdef KLUA
#define KLuaHeapBase		0x00200000
#endif

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

// See BCM-2835-ARM-Peripherals p90
#define GPFSEL1			(KPeripheralBase + 0x00200004)
#define GPSET0			(KPeripheralBase + 0x0020001C)
#define GPCLR0			(KPeripheralBase + 0x00200028)
#define GPPUD			(KPeripheralBase + 0x00200094)
#define GPPUDCLK0		(KPeripheralBase + 0x00200098)
#define KGpioFunctionSelectPinMask (7)
#define KGpioFunctionSelectOutput (1)
#define PIN_SHIFT(n) (((n) < 10 ? (n) : (n)-10)*3)
#define SetGpioFunctionForPin(reg, pin, val) reg = (((reg) & ~(KGpioFunctionSelectPinMask << PIN_SHIFT(pin))) | (((val) & KGpioFunctionSelectPinMask) << PIN_SHIFT(pin)))

#define KGpioModeInput		0
#define KGpioModeOutput		1
#define KGpioAlt0			4
#define KGpioAlt1			5
#define KGpioAlt2			6
#define KGpioAlt3			7
#define KGpioAlt4			3
#define KGpioAlt5			2

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
#define UART_INT			57

// BCM-2835-ARM-Peripherals p12 says you set bit 1 to enable receive. It's actually bit 0.
#define AUX_MU_EnableReceiveInterrupt	(1 << 0)
#define AUX_MU_ClearReceiveFIFO			(1 << 1)
#define AUX_MU_ClearTransmitFIFO		(1 << 2)
#define AUX_MU_IIR_ReceiveInterrupt		(1 << 2)


#endif // LUPI_BUILD_PI_H
