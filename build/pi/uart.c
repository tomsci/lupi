#include <std.h>

extern void dummy ( unsigned int );

uint32 GET32(uint32 addr);
void PUT32(uint32 addr, uint32 val);

/*
See BCM-2835-ARM-Peripherals p11 2.2.1 Mini UART implementation details

baud = system_clock_freq / (8 * (reg + 1)
reg  = ((system_clock_freq / 8) / baud) - 1
*/
#define baudReg(baud) (((KSystemClockFreq / 8) / baud) - 1)
#define k115200baud baudReg(115200)

//-------------------------------------------------------------------------
//
// Copyright (c) 2012 David Welch dwelch@dwelch.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------

void uart_init() {
	PUT32(AUX_ENABLES,1);
	PUT32(AUX_MU_CNTL_REG,0);
	PUT32(AUX_MU_LCR_REG,3);
	PUT32(AUX_MU_MCR_REG,0);
	PUT32(AUX_MU_IER_REG, AUX_MU_EnableReceiveInterrupt);
	PUT32(AUX_MU_IIR_REG, AUX_MU_ClearReceiveFIFO | AUX_MU_ClearTransmitFIFO);
	PUT32(AUX_MU_BAUD_REG, k115200baud);

	unsigned int ra;
	ra = GET32(GPFSEL1);
	// Alt5 for GPIO 14 is TXD1
	SetGpioFunctionForPin(ra, 14, KGpioAlt5);
	// Alt5 for GPIO 15 is RXD1
	SetGpioFunctionForPin(ra, 15, KGpioAlt5);
	PUT32(GPFSEL1, ra);

	PUT32(GPPUD,0);
	for(ra=0;ra<150;ra++) dummy(ra);
	PUT32(GPPUDCLK0,(1<<14)|(1<<15));
	for(ra=0;ra<150;ra++) dummy(ra);
	PUT32(GPPUDCLK0,0);

	PUT32(AUX_MU_CNTL_REG,3);

	// The mini uart uses the AUX interrupt not the UART one.
	PUT32(IRQ_ENABLE_1, 1 << AUX_INT);
}

void putbyte(byte c) {
    while (1) {
        if (GET32(AUX_MU_LSR_REG) & 0x20) break;
    }
    PUT32(AUX_MU_IO_REG, c);
}

bool byteReady() {
	return (GET32(AUX_MU_LSR_REG) & 0x01);
}

byte getch() {
	while (!byteReady()) {
		// Spin
	}
	return (byte)GET32(AUX_MU_IO_REG);
}
