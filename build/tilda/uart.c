#include <k.h>
#include "pio.h"

#define UART_CR		0x400E0800
#define UART_MR		0x400E0804
#define UART_SR		0x400E0814
#define UART_BRGR	0x400E0820

#define UART_RHR	0x400E0818
#define UART_THR	0x400E081C

// see p835
#define USART0_BASE	0x40098000
#define USART0_CR	0x40098000
#define USART0_MR	0x40098004
#define USART0_CSR	0x40098014
#define USART0_RHR	0x40098018
#define USART0_THR	0x4009801C
#define USART0_BRGR	0x40098020

#define USART0_RXD	(1 << 10) // USART0 RXD0 PA10 perif A
#define USART0_TXD	(1 << 11) // USART0 TXD0 PA11 perif A

// UART_CR, USARTx_CR (p836)
#define RSTRX		(1 << 2)
#define RSTTX		(1 << 3)
#define RXEN		(1 << 4)
#define RXDIS		(1 << 5)
#define TXEN		(1 << 6)
#define TXDIS		(1 << 7)

// UART_MR (p765), USARTx_MR (p839)
#define PARITY_NONE	(4 << 9)
#define MODE_NORMAL	0

// USARTx_MR only (p839)
#define CHRL_8BIT	(3 << 6)
#define OVER		(1 << 19)

// UART_SR, USARTx_CSR (p849)
#define RXRDY		(1 << 0)
#define TXRDY		(1 << 1)

// Fortunately, the basic read and write is the same between the UART and the
// USARTs, so we can reuse the same code and just change which registers are read.
#ifdef TILDA_USE_USART0
#define RHR	USART0_RHR
#define THR	USART0_THR
#define SR	USART0_CSR
#else
#define RHR UART_RHR
#define THR UART_THR
#define SR	UART_SR
#endif

void uart_init() {

	// TODO configure NVIC

#ifdef TILDA_USE_USART0

    PUT32(PIOA + PIO_PDR, USART0_RXD | USART0_TXD); // PIO disable (peripheral enable)
	PUT32(PIOA + PIO_IDR, USART0_RXD | USART0_TXD); // Disable interrupts

    // Must use pull-up on TXD (p780)
	PUT32(PIOA + PIO_PUDR, USART0_RXD); // Disable pull-up on RXD
	PUT32(PIOA + PIO_PUER, USART0_TXD); // Enable pull-up on TXD
	PUT32(PMC_PCER0, 1 << PERIPHERAL_ID_USART0); // Enable clock

    PUT32(USART0_CR, RSTTX | RSTRX | RXDIS | TXDIS); // Start off with a reset

	PUT32(USART0_MR, OVER | CHRL_8BIT | PARITY_NONE | MODE_NORMAL);
	PUT32(USART0_BRGR, 91 | (1 << 16)); // CS=91 FP=1 with OVER=1 should give about 115226 baud
	PUT32(USART0_CR, RXEN | TXEN);

	// Disable PDC just in case
	//PUT32(USART0_BASE + PERIPH_PTCR, PERIPH_TXTDIS | PERIPH_RXTDIS);
#else

	PUT32(UART_BRGR, 46); // Probably gives about 115200 baud
	PUT32(UART_MR, PARITY_NONE | MODE_NORMAL);
	PUT32(UART_CR, RXEN | TXEN);

#endif
}

#define UART_BUF_SIZE (sizeof(TheSuperPage->uartBuf) - 2)

bool byteReady() {
	SuperPage* s = TheSuperPage;
	return !ring_empty(s->uartBuf, UART_BUF_SIZE) || (GET32(SR) & RXRDY);
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
	return (byte)GET32(RHR);
}

// See p761
void putbyte(byte c) {
	while(1) {
		if (GET32(SR) & TXRDY) break;
	}
	PUT32(THR, c);
}
