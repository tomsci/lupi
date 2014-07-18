#include <k.h>
#include <exec.h>
#include <kipc.h>
#include <err.h>

void putbyte(byte b);
bool byteReady();
byte getch();
static int getInt(int arg);

NOINLINE NAKED uint64 readUserInt64(uintptr ptr) {
	asm("MOV r2, r0");
	asm("LDRT r0, [r2], #4"); // I think this is the correct way round...
	asm("LDRT r1, [r2]");
	asm("BX lr");
}

int64 handleSvc(int cmd, uintptr arg1, uintptr arg2, uint32 r14_svc) {
	if (cmd & KFastExec) {
		cmd = cmd & ~KFastExec;
	}

	Process* p = TheSuperPage->currentProcess;
	Thread* t = TheSuperPage->currentThread;
	uint64 result = 0;

	switch (cmd) {
		case KExecSbrk:
			if (arg1) {
				uintptr oldLim = p->heapLimit;
				bool ok = process_grow_heap(p, (int)arg1);
				if (ok) result = oldLim;
				else result = -1;
			} else {
				result = p->heapLimit;
			}
			break;
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
				result = getch();
				break;
			}
			thread_setState(t, EBlockedFromSvc);
			saveCurrentRegistersForThread(&r14_svc);
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
				saveCurrentRegistersForThread(&r14_svc);
				t->savedRegisters[0] = p->pid;
				process_start(p); // effectively causes a reschedule
				// We should never get here because when the calling thread gets rescheduled,
				// it goes straight back into user mode (because that's how we roll - no
				// preemption in svc mode except for things that explicitly yield to user mode)
				ASSERT(false);
			} else {
				result = err;
			}
			break;
		}
		case KExecThreadExit:
			thread_exit(t, (int)arg1); // Never returns
			break;
		case KExecAbort:
			if (t) {
				// It's conceivable the thread could be null, if we've aborted
				// during the bootmenu for eg
				saveCurrentRegistersForThread(&r14_svc);
				printk("Abort called by process %s\n", p->name);
			} else {
				printk("Abort called during boot\n");
			}
			kabort1(0xABBADEAD); // doesn't return
			break;
		case KExecReboot:
			reboot(); // doesn't return
			break;
		case KExecThreadYield:
			saveCurrentRegistersForThread(&r14_svc);
			thread_yield(t);
			reschedule();
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
				result = *reqs;
				*reqs = 0;
			} else {
				thread_setState(t, EWaitForRequest);
				saveCurrentRegistersForThread(&r14_svc);
				reschedule();
			}
			break;
		}
#endif
		case KExecGetUptime:
			result = TheSuperPage->uptime;
			break;
		case KExecNewSharedPage:
			result = ipc_mapNewSharedPageInCurrentProcess();
			break;
		case KExecCreateServer:
			result = ipc_createServer(arg1, t);
			break;
		case KExecConnectToServer:
			// Always save registers, because we'll need to block
			saveCurrentRegistersForThread(&r14_svc);
			// This doesn't return, unless there was an error
			result = ipc_connectToServer(arg1, arg2);
			break;
		case KExecRequestServerMsg:
			ipc_requestServerMsg(t, arg1);
			break;
		case KExecCompleteIpcRequest:
			result = ipc_completeRequest(arg1, arg2);
			break;
		case KExecSetTimer: {
			if (TheSuperPage->timerRequest.thread && TheSuperPage->timerRequest.thread != t) {
				printk("Some other thread %p muscling in on the timer racket\n", t);
				result = KErrAlreadyExists;
				break;
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
		case KExecGetInt:
			result = getInt(arg1);
			break;
		default:
			ASSERT(false, cmd);
			break;
	}

	kern_disableInterrupts();
	// Now we're done servicing the SVC, check if rescheduleNeededOnSvcExit
	// was set meaning our thread's timeslice expired during the SVC (but
	// because we don't support preempting SVC threads yet it couldn't
	// reschedule at that point). We disable interrupts here to make sure the
	// timeslice doesn't expire after we've checked rescheduleNeededOnSvcExit
	// but before we return.
	if (atomic_setbool(&TheSuperPage->rescheduleNeededOnSvcExit, false)) {
		saveCurrentRegistersForThread(&r14_svc);
		// Save the result also
		t->savedRegisters[0] = (uint32)result;
		t->savedRegisters[1] = (result >> 32);
		reschedule();
	}
	// The MOVS to return from SVC mode will reenable interrupts for us
	return result;
}

static int getInt(int arg) {
	switch(arg) {
	case EValTotalRam:
		return TheSuperPage->totalRam;
	case EValBootMode:
		return TheSuperPage->bootMode;
	default:
		ASSERT(false, arg);
	}
}
