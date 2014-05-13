#include <k.h>
#include <exec.h>

void putbyte(byte b);
bool byteReady();
byte getch();

int64 handleSvc(int cmd, uintptr arg1, uintptr* arg2, void* savedRegisters) {
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
			thread_setState(t, EBlockedFromSvc);
			saveUserModeRegistersForCurrentThread(savedRegisters, true);
			thread_setBlockedReason(t, EBlockedOnGetch);
			TheSuperPage->blockedUartReceiveIrqHandler = t;
			reschedule();
			// reschedule never returns - the IRQ handler is responsible for populating
			// t->savedRegisters[0] with the char value and unblocking us, then the IRQ
			// handler will return to after the WFI() in reschedule(), at which point
			// this thread is ready so will get jumped back to user mode with the result
			// of the SVC call all nicely filled in in r0.
			kabort();
		}
#ifndef KLUA
		case KExecCreateProcess: {
			// TODO sanitise again!
			const char* name = (const char*)arg1;
			Process* p = NULL;
			int err = process_new(name, &p);
			if (err == 0) {
				saveUserModeRegistersForCurrentThread(savedRegisters, true);
				t->savedRegisters[0] = p->pid;
				process_start(p); // effectively causes a reschedule
				// We should never get here because when the calling thread gets rescheduled,
				// it goes straight back into user mode (because that's how we roll - no
				// preemption in svc mode except for things that explicitly yield to user mode)
				ASSERT(false);
			} else {
				return err;
			}
			break;
		}
		case KExecThreadExit:
			thread_exit(t, (int)arg1);
			reschedule(); // Never returns
			break;
		case KExecAbort:
			saveUserModeRegistersForCurrentThread(savedRegisters, true);
			kabort1(0xABBADEAD); // doesn't return
			break;
		case KExecGetch_Async: {
			if (byteReady()) {
				KAsyncRequest req = { .thread = t, .userPtr = arg1 };
				thread_requestComplete(&req, getch());
			} else {
				SuperPage* s = TheSuperPage;
				s->uartRequest.thread = t;
				s->uartRequest.userPtr = arg1;
			}
			break;
		}
		case KExecWaitForAnyRequest: {
			uint8* reqs = &t->completedRequests;
			if (*reqs) {
				// There are some completed requests, return immediately
				int result = *reqs;
				*reqs = 0;
				return result;
			} else {
				thread_setState(t, EWaitForRequest);
				saveUserModeRegistersForCurrentThread(savedRegisters, true);
				reschedule();
			}
			break;
		}
#endif
		case KExecGetUptime:
			return TheSuperPage->uptime;
		default:
			break;
	}
	return 0;
}
