extern void dummy ( unsigned int );

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

//GPIO14  TXD0 and TXD1
//GPIO15  RXD0 and RXD1
//alt function 5 for uart1
//alt function 0 for uart0

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
    PUT32(AUX_MU_IER_REG,0);
    PUT32(AUX_MU_CNTL_REG,0);
    PUT32(AUX_MU_LCR_REG,3);
    PUT32(AUX_MU_MCR_REG,0);
    PUT32(AUX_MU_IER_REG,0);
    PUT32(AUX_MU_IIR_REG,0xC6);
    PUT32(AUX_MU_BAUD_REG, k115200baud);

    unsigned int ra;
    ra=GET32(GPFSEL1);
    ra&=~(7<<12); //gpio14
    ra|=2<<12;    //alt5
    ra&=~(7<<15); //gpio15
    ra|=2<<15;    //alt5
    PUT32(GPFSEL1,ra);

    PUT32(GPPUD,0);
    for(ra=0;ra<150;ra++) dummy(ra);
    PUT32(GPPUDCLK0,(1<<14)|(1<<15));
    for(ra=0;ra<150;ra++) dummy(ra);
    PUT32(GPPUDCLK0,0);

    PUT32(AUX_MU_CNTL_REG,3);
}

void putbyte(byte c) {
    while (1) {
        if (GET32(AUX_MU_LSR_REG) & 0x20) break;
    }
    PUT32(AUX_MU_IO_REG, c);
}

byte getch() {
	while (1)
	{
		if (GET32(AUX_MU_LSR_REG) & 0x01) break;
	}
	return (byte)GET32(AUX_MU_IO_REG);
}
