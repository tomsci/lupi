#include <k.h>
#include "gpio.h"

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
	PUT32(AUX_ENABLES, AUXENB_MiniUartEnable);
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

	gpio_setPull(KGpioDisablePullUpDown, GPPUDCLK0, (1<<14)|(1<<15));

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

#define UART_BUF_SIZE (sizeof(TheSuperPage->uartBuf) - 2)

void uart_got_char(byte b) {
	SuperPage* s = TheSuperPage;
	Thread* t = atomic_set_thread(&s->blockedUartReceiveIrqHandler, NULL);
	if (t) {
		t->savedRegisters[0] = b;
		thread_setState(t, EReady);
		// The returning WFI in reschedule() should take care of the rest
	} else if (s->uartRequest.userPtr) {
		dfc_requestComplete(&s->uartRequest, b);
	} else if (!ring_full(s->uartBuf, UART_BUF_SIZE)) {
		ring_push(s->uartBuf, UART_BUF_SIZE, b);
	} else {
		//printk("Dropping char %c on the floor\n", b);
		atomic_inc8(&s->uartDroppedChars);
	}
}

bool byteReady() {
	SuperPage* s = TheSuperPage;
	return !ring_empty(s->uartBuf, UART_BUF_SIZE) || (GET32(AUX_MU_LSR_REG) & 0x01);
}

byte getch() {
	SuperPage* s = TheSuperPage;
	uint8 droppedChars = atomic_set8(&s->uartDroppedChars, 0);
	if (droppedChars) {
		printk("|Warning: %d dropped chars|", droppedChars);
	}
	if (!ring_empty(s->uartBuf, UART_BUF_SIZE)) {
		return ring_pop(s->uartBuf, UART_BUF_SIZE);
	}

	while (!byteReady()) {
		// Spin
	}
	return (byte)GET32(AUX_MU_IO_REG);
}
