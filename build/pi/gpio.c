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
