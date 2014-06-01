#include <k.h>
#include <exec.h>
#include <kipc.h>
#include <err.h>

void putbyte(byte b);
bool byteReady();
byte getch();

NOINLINE NAKED uint64 readUserInt64(uintptr ptr) {
	asm("MOV r2, r0");
	asm("LDRT r0, [r2], #4"); // I think this is the correct way round...
	asm("LDRT r1, [r2]");
	asm("BX lr");
}

int64 handleSvc(int cmd, uintptr arg1, uintptr arg2, uintptr r14_svc) {
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
			//printk("[%d] %s", indexForProcess(p), (const char*)arg1);
			break;
		case KExecPutch:
			putbyte(arg1);
			break;
		case KExecGetch: {
			if (byteReady()) {
				return getch();
			}
			thread_setState(t, EBlockedFromSvc);
			saveUserModeRegistersForCurrentThread(&r14_svc, true);
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
				saveUserModeRegistersForCurrentThread(&r14_svc, true);
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
			thread_exit(t, (int)arg1); // Never returns
			break;
		case KExecAbort:
			saveUserModeRegistersForCurrentThread(&r14_svc, true);
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
				saveUserModeRegistersForCurrentThread(&r14_svc, true);
				reschedule();
			}
			break;
		}
#endif
		case KExecGetUptime:
			return TheSuperPage->uptime;
		case KExecNewSharedPage:
			return ipc_mapNewSharedPageInCurrentProcess();
		case KExecCreateServer:
			return ipc_createServer(arg1, t);
		case KExecConnectToServer:
			// Always save registers, because we'll need to block
			saveUserModeRegistersForCurrentThread(&r14_svc, true);
			// This doesn't return, unless there was an error
			return ipc_connectToServer(arg1, arg2);
		case KExecRequestServerMsg:
			ipc_requestServerMsg(t, arg1);
			break;
		case KExecCompleteIpcRequest:
			return ipc_completeRequest(arg1, arg2);
			break;
		case KExecSetTimer: {
			if (TheSuperPage->timerRequest.thread && TheSuperPage->timerRequest.thread != t) {
				printk("Some other thread %p muscling in on the timer racket\n", t);
				return KErrAlreadyExists;
			}
			TheSuperPage->timerRequest.thread = t;
			TheSuperPage->timerRequest.userPtr = arg1;
			uint64 time = readUserInt64(arg2);
			if (time <= TheSuperPage->uptime) {
				// Already ready, don't wait for tick()
				TheSuperPage->timerCompletionTime = UINT64_MAX;
				thread_requestComplete(&TheSuperPage->timerRequest, 0);
			} else {
				TheSuperPage->timerCompletionTime = time;
			}
			break;
		}
		default:
			break;
	}
	return 0;
}
