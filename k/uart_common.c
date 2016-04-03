#include <k.h>

#define UART_BUF_SIZE (sizeof(TheSuperPage->uartBuf) - 2)

bool uart_byteReady();
byte uart_getch();

void uart_got_char(byte b) {
	SuperPage* s = TheSuperPage;
	Thread* t = atomic_set_thread(&s->blockedUartReceiveIrqHandler, NULL);
	if (t) {
		thread_writeSvcResult(t, b);
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
	return !ring_empty(s->uartBuf, UART_BUF_SIZE) || uart_byteReady();
}

byte getch() {
#ifndef INTERRUPTS_OFF
	SuperPage* s = TheSuperPage;
	if (!s->quiet) {
		printk("Calling atomic_set8\n");
		uint8 droppedChars = atomic_set8(&s->uartDroppedChars, 0);
		printk("atomic_set8 returned %d\n", droppedChars);
		if (droppedChars) {
			printk("|Warning: %d dropped chars|", droppedChars);
		}
	}
	if (!ring_empty(s->uartBuf, UART_BUF_SIZE)) {
		return ring_pop(s->uartBuf, UART_BUF_SIZE);
	}
#endif
	while (!byteReady()) {
		// Spin
	}
	return uart_getch();
}
