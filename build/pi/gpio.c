#include <k.h>
#include "gpio.h"

void dummy();
uint32 GET32(uint32 addr);
void PUT32(uint32 addr, uint32 val);

/**
Sets the pull up or pull down status for the given pins according to the
BCM-2835-ARM-Peripherals.pdf spec p101. `clkReg` must be `GPPUDCLK0` or
`GPPUDCLK1`, depending on whether the desired pins are 0-31 or 32-53. The pins
should be specified as a bitmask relative to the appropriate `GPPUDCLKn`
register. For example:

	// Disable pullups for pins 24 and 25
	gpio_setPull(KGpioDisablePullUpDown, GPPUDCLK0, (1<<24)|(1<<25));

	// Enable pull down on pin 34 (pin 34 = bit 2 in GPPUDCLK1)
	gpio_setPull(KGpioEnablePullDown, GPPUDCLK1, 1<<2);
*/
void gpio_setPull(int upDownStatus, uintptr clkReg, uint32 bitmask) {
	PUT32(GPPUD, upDownStatus);
	for (int i = 0; i < 150; i++) {
		// Spec says you really do have to spin for 150 cycles
		dummy();
	}
	PUT32(clkReg, bitmask);
	for (int i = 0; i < 150; i++) {
		dummy();
	}
	PUT32(clkReg, 0);
}

/**
Sets the value of a GPIO output pin. No checks are performed as to whether the
pin is actually configured correctly for output.
*/
void gpio_set(int pin, bool value) {
 	uintptr reg = (value ? GPSET0 : GPCLR0);
 	if (pin >= 32) {
 		pin -= 32;
 		reg += 4; // Ie GPSET1 or GPCLR1
 	}
 	uint32 mask = 1 << pin;
 	PUT32(reg, mask);
}

//////

/**
Call at the beginning of communication with a SPI device. This sets chip_select
and starts the SPI clock running. `cs` must be the appropriate BCM2835 CS
register configuration for the device you'll be talking to.
*/
void spi_beginTransaction(int cs) {
	cs |= SPI_CS_TA; // This is what kicks the bus into motion
	PUT32(SPI_CS, cs);
}

void spi_endTransaction() {
	PUT32(SPI_CS, 0); // Clears SPI_CS_TA
}

#define WaitForBit(b) while ((GET32(SPI_CS) & (b)) == 0) { dummy(); }

/**
Writes data to the SPI bus, polling for completion. Interrupts are not used and
need not be enabled.
*/
void spi_write_poll(uint8* buf, int length) {
	//printk("Starting SPI write cmd=%X len=%d ", (uint)buf[0], length);
	for (int i = 0; i < length; i++) {
		// Wait for fifo to be ready
		//printk("T");
		WaitForBit(SPI_CS_TXD);
		PUT32(SPI_FIFO, buf[i]);
		// Wait for the corresponding read byte (which we ignore for now)
		//printk("R");
		WaitForBit(SPI_CS_RXD);
		GET32(SPI_FIFO); // Have to do this to keep the SPI FIFO happy
	}
	//printk("D");
	WaitForBit(SPI_CS_DONE);
	//printk("one\n");
}