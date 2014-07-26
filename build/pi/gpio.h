#ifndef GPIO_H

#ifndef LUPI_STD_H
#include <std.h>
#endif

// See BCM-2835-ARM-Peripherals p90
#define GPFSEL0			(KPeripheralBase + 0x00200000) // GPIOs 0-9
#define GPFSEL1			(KPeripheralBase + 0x00200004) // GPIOs 10-19
#define GPFSEL2			(KPeripheralBase + 0x00200008) // GPIOs 20-29
#define GPFSEL3			(KPeripheralBase + 0x0020000C) // GPIOs 30-39
#define GPFSEL4			(KPeripheralBase + 0x00200010) // GPIOs 40-49
#define GPFSEL5			(KPeripheralBase + 0x00200014) // GPIOs 50-53
#define GPSET0			(KPeripheralBase + 0x0020001C)
#define GPSET1			(KPeripheralBase + 0x00200020)
#define GPCLR0			(KPeripheralBase + 0x00200028)
#define GPCLR1			(KPeripheralBase + 0x0020002C)
#define GPPUD			(KPeripheralBase + 0x00200094)
#define GPPUDCLK0		(KPeripheralBase + 0x00200098)
#define KGpioFunctionSelectPinMask (7)
#define PIN_SHIFT(n)	(((n) % 10) * 3)
#define SetGpioFunctionForPin(reg, pin, val) \
	reg = (((reg) & ~(KGpioFunctionSelectPinMask << PIN_SHIFT(pin))) | \
	(((val) & KGpioFunctionSelectPinMask) << PIN_SHIFT(pin)))

// See GPPUD in BCM-2835-ARM-Peripherals p100
#define KGpioDisablePullUpDown 0
#define KGpioEnablePullDown	1
#define KGpioEnablePullUp	2

#define KGpioModeInput		0
#define KGpioModeOutput		1
#define KGpioAlt0			4
#define KGpioAlt1			5
#define KGpioAlt2			6
#define KGpioAlt3			7
#define KGpioAlt4			3
#define KGpioAlt5			2

void gpio_setPull(int upDownStatus, uintptr clkReg, uint32 bitmask);

void gpio_set(int pin, bool value);

#endif
