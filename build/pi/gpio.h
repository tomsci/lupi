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
#define GPLEV0			(KPeripheralBase + 0x00200034)
#define GPLEV1			(KPeripheralBase + 0x00200038)
#define GPEDS0			(KPeripheralBase + 0x00200040)
#define GPEDS1			(KPeripheralBase + 0x00200044)
#define GPFEN0			(KPeripheralBase + 0x00200058)
#define GPFEN1			(KPeripheralBase + 0x0020005C)
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


/// SPI stuff


#define SPI_CS			(KPeripheralBase + 0x00204000)
#define SPI_FIFO		(KPeripheralBase + 0x00204004)
#define SPI_CLK			(KPeripheralBase + 0x00204008)

#define SPI_CS_TXD		(1<<18) // TX FIFO can accept data
#define SPI_CS_RXD		(1<<17) // RX FIFO contains data
#define SPI_CS_DONE		(1<<16) // Everything quiet
#define SPI_CS_INTR		(1<<10)
#define SPI_CS_INTD		(1<<9)
#define SPI_CS_TA		(1<<7)	// Transfer active
#define SPI_CS_CSPOL	(1<<6)
#define SPI_CS_CPOL		(1<<3)
#define SPI_CS_CPHA		(1<<2)

#define SPI_CS_CLEAR_TX	(1<<4)
#define SPI_CS_CLEAR_RX	(1<<5)

void spi_beginTransaction(uint32 cs, uint32 cdiv);
void spi_endTransaction();
void spi_readwrite_poll(uint8* buf, int length, bool writeBack);
#define spi_write_poll(buf, length) spi_readwrite_poll(buf, length, false)

#endif
