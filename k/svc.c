#include <k.h>
#include <exec.h>
#include <kipc.h>
#include <err.h>

void putbyte(byte b);
bool byteReady();
byte getch();
static int getInt(int arg);
static const char* getString(int arg);

NOINLINE NAKED uint64 readUserInt64(uintptr ptr) {
	// ARM supports a post-increment which we use on the first instruction,
	// but THUMB2 only supports a pre-increment which we use on the second!
#ifdef ARM
	asm("MOV r2, r0");
	asm("LDRT r0, [r2], #4");
	asm("LDRT r1, [r2]");
	asm("BX lr");
#elif defined(THUMB2)
	asm("MOV r2, r0");
	asm("LDRT r0, [r2]");
	asm("LDRT r1, [r2, #4]");
	asm("BX lr");
#elif defined(AARCH64)
	asm("LDTR x0, [x0]");
	asm("RET");
#else
	#error "wtf?"
#endif
}

int64 handleSvc(int cmd, uintptr arg1, uintptr arg2, void* savedRegisters) {
	// printk("+handleSvc %x\n", cmd);
#ifdef TIMER_DEBUG
	if (!TheSuperPage->marvin) {
		TheSuperPage->lastSvcTime = (TheSuperPage->uptime << 16) + GET32(SYSTICK_VAL);
		TheSuperPage->lastSvc = cmd;
	}
#endif
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
				// printk("sbrk oldLim=%X incr=%d\n", (uint)oldLim, (int)arg1);
				bool ok = process_grow_heap(p, (int)arg1);
				if (ok) result = oldLim;
				else result = -1;
			} else {
				result = p->heapLimit;
			}
			// printk("sbrk p=%p returning %X\n", p, (uint)result);
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
			saveCurrentRegistersForThread(savedRegisters);
			thread_setState(t, EBlockedFromSvc);
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
#ifndef LUPI_NO_PROCESS
		case KExecCreateProcess: {
			// TODO sanitise again!
			const char* name = (const char*)arg1;
			Process* p = NULL;
			int err = process_new(name, &p);
			if (err == 0) {
				saveCurrentRegistersForThread(savedRegisters);
				thread_writeSvcResult(t, p->pid);
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
		case KExecThreadCreate: {
			Thread* result = NULL;
			return thread_new(p, arg1, &result);
		}
		case KExecThreadExit:
			thread_exit(t, (int)arg1); // Never returns
			break;
		case KExecAbort:
			if (t) {
				// It's conceivable the thread could be null, if we've aborted
				// during the bootmenu for eg
				saveCurrentRegistersForThread(savedRegisters);
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
			saveCurrentRegistersForThread(savedRegisters);
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
				saveCurrentRegistersForThread(savedRegisters);
				thread_setState(t, EWaitForRequest);
				reschedule();
			}
			break;
		}
#endif
		case KExecGetUptime:
			result = TheSuperPage->uptime;
			break;
#ifndef LUPI_NO_IPC
		case KExecNewSharedPage:
			result = ipc_mapNewSharedPageInCurrentProcess();
			break;
		case KExecCreateServer:
			result = ipc_createServer(arg1, t);
			break;
		case KExecConnectToServer:
			// Always save registers, because we'll need to block
			saveCurrentRegistersForThread(savedRegisters);
			// This doesn't return, unless there was an error
			result = ipc_connectToServer(arg1, arg2);
			break;
		case KExecRequestServerMsg:
			ipc_requestServerMsg(t, arg1);
			break;
		case KExecCompleteIpcRequest:
			result = ipc_completeRequest(arg1, arg2);
			break;
#endif
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
		case KExecGetString:
			result = (uintptr)getString(arg1);
			break;
		case KExecDriverConnect: {
			uint32 id = arg1;
			// Find the driver
			int i;
			for (i = 0; i < MAX_DRIVERS; i++) {
				if (TheSuperPage->drivers[i].id == id) {
					break;
				}
			}
			if (i == MAX_DRIVERS) {
				result = KErrNotFound;
			} else {
				result = i | KDriverHandle;
			}
			break;
		}
		case KExecStfu:
			TheSuperPage->quiet = (bool)arg1;
			break;
#ifndef LUPI_NO_PROCESS
		case KExecReplaceProcess: {
			const char* newName = (const char*)arg1;
			ASSERT_USER_PTR8(newName);
			result = process_reset(t, newName);
			// printk("process_reset to '%s' completed with %d\n", newName, (int)result);
			if (result) break;
			process_start(p);
			// Doesn't return
			break;
		}
#endif
		default: {
			ASSERT(cmd & KDriverHandle, cmd);
			int driverIdx = cmd & 0xFF;
			ASSERT(driverIdx < MAX_DRIVERS, cmd);
			Driver* d = &TheSuperPage->drivers[driverIdx];
			result = d->execFn(d, arg1, arg2);
			break;
		}
	}

#ifdef ARM
	// This is handled by pendSV on ARMv7-M
	kern_disableInterrupts();
	// Now we're done servicing the SVC, check if rescheduleNeededOnSvcExit
	// was set meaning our thread's timeslice expired during the SVC (but
	// because we don't support preempting SVC threads yet it couldn't
	// reschedule at that point). We disable interrupts here to make sure the
	// timeslice doesn't expire after we've checked rescheduleNeededOnSvcExit
	// but before we return.
	if (atomic_setbool(&TheSuperPage->rescheduleNeededOnSvcExit, false)) {
		saveCurrentRegistersForThread(savedRegisters);
		// Save the result also
		t->savedRegisters[0] = (uint32)result;
		t->savedRegisters[1] = (result >> 32);
		reschedule();
	}
	// The MOVS to return from SVC mode will reenable interrupts for us
#endif
	// printk("-handleSvc %x\n", cmd);
	return result;
}

static int getInt(int arg) {
	switch(arg) {
	case EValTotalRam:
		return TheSuperPage->totalRam;
	case EValBootMode:
		return TheSuperPage->bootMode;
	case EValScreenWidth:
		return TheSuperPage->screenWidth;
	case EValScreenHeight:
		return TheSuperPage->screenHeight;
	case EValScreenFormat:
		return TheSuperPage->screenFormat;
	default:
		ASSERT(false, arg);
	}
}

static const char* getString(int arg) {
	switch(arg) {
	case EValVersion:
		return LUPI_VERSION_STRING;
	default:
		return NULL;
	}
}

void kern_registerDriver(uint32 id, DriverExecFn fn) {
	Driver* driver = NULL;
	for (int i = 0; i < MAX_DRIVERS; i++) {
		if (TheSuperPage->drivers[i].id == 0) {
			driver = &TheSuperPage->drivers[i];
			break;
		}
	}
	ASSERT(driver); // Otherwise no room at the inn
	driver->id = id;
	driver->execFn = fn;
}
