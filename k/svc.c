#include <k.h>
#include <exec.h>

void putbyte(byte b);
bool byteReady();
byte getch();

int handleSvc(int cmd, uintptr arg1, uintptr arg2, void* savedRegisters) {
	Process* p = TheSuperPage->currentProcess;
	Thread* t = TheSuperPage->currentThread;

	switch (cmd) {
		case KExecSbrk:
			if (arg1) {
				uintptr oldLim = p->heapLimit;
				bool ok = process_grow_heap(p, (int)arg1);
				if (ok) return oldLim;
				else return -1;
			} else {
				return p->heapLimit;
			}
		case KExecPrintString:
			// TODO sanitise parameters!
			printk("%s", (const char*)arg1);
			break;
		case KExecPutch:
			putbyte(arg1);
			break;
		case KExecGetch: {
			if (byteReady()) {
				return getch();
			}
			t->state = EBlocked;
			saveUserModeRegistersForCurrentThread(savedRegisters);
			TheSuperPage->blockedUartReceiveIrqHandler = t;
			reschedule(t);
			// reschedule never returns - the IRQ handler is responsible for populating
			// t->savedRegisters[0] with the char value and unblocking us, then the IRQ
			// handler will return to after the WFI() in reschedule(), at which point
			// this thread is ready so will get jumped back to user mode with the result
			// of the SVC call all nicely filled in in r0.
		}
		default:
			break;
	}
	return 0;
}
