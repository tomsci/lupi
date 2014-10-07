#include <k.h>
#include <mmu.h>
#if defined(ARM)
#include <arm.h>
#elif defined(ARMV7_M)
#include <armv7-m.h>
#endif

Thread* findNextReadyThread() {
	Thread* head = TheSuperPage->readyList;
	if (!head) return NULL;
	Thread* t = head;
	//printk("t=%p t->next=%p\n", t, t?t->next:0);
	do {
		if (t->state == EReady) {
			return t;
		}
		t = t->next;
	} while (t != head);
	return NULL;
}

/**
Perform a reschedule, ie causes a different thread to execute. Does not return.
Interrupts may or may not be enabled. Must be in SVC mode.

See also: [reschedule_irq()](#reschedule_irq)
*/
NORETURN NAKED reschedule() {
	asm(".doReschedule:");
	asm("BL findNextReadyThread");
	asm("CMP r0, #0");
	asm("BNE scheduleThread");

	// If we get here, no more threads to run, need to just WFI
	// But in order to do that we need to safely reenable interrupts
	asm("LDR r1, .TheCurrentThreadAddr");
	asm("STR r0, [r1]"); // currentThread = NULL
	DSB(r0);
	kern_enableInterrupts();
	WFI(r0);
	kern_disableInterrupts();
	asm("B .doReschedule");
	LABEL_WORD(.TheCurrentThreadAddr, &TheSuperPage->currentThread);
}

static void dequeueFromReadyList(Thread* t) {
	thread_dequeue(t, &TheSuperPage->readyList);
}

void thread_dequeue(Thread* t, Thread** head) {
	//printk("dequeue t=%p t->next=%p\n", t, t?t->next:0);
	t->prev->next = t->next;
	t->next->prev = t->prev;
	if (*head == t) {
		if (t == t->next) {
			*head = NULL;
		} else {
			*head = t->next;
		}
	}
	// This is important to do when you factor in how we reuse prev and next in
	// Server.blockedClientList (and it's handy for debugging regardless)
	t->prev = NULL;
	t->next = NULL;
}

void thread_enqueueBefore(Thread* t, Thread* before) {
	ASSERT(t->prev == NULL && t->next == NULL, (uint32)t);
	if (before == NULL) {
		// Must be nothing in the list
		t->next = t;
		t->prev = t;
	} else {
		t->next = before;
		t->prev = before->prev;
		before->prev->next = t;
		before->prev = t;
	}
}

/**
Moves a thread to the end of the ready list. Does not reschedule or change its
ready state or timeslice.
*/
void thread_yield(Thread* t) {
	// Move to end of ready list
	SuperPage* s = TheSuperPage;
	dequeueFromReadyList(t);
	thread_enqueueBefore(t, s->readyList ? s->readyList->prev : NULL);
	if (!s->readyList) s->readyList = t;
}

void thread_setState(Thread* t, ThreadState s) {
	//printk("thread_setState thread %d-%d s=%d t->next=%p\n", indexForProcess(processForThread(t)), t->index, s, t->next);
	if (s == EReady) {
		// Move to head of ready list
		thread_enqueueBefore(t, TheSuperPage->readyList);
		TheSuperPage->readyList = t;
	} else if (t->state == EReady) {
		dequeueFromReadyList(t);
	}
	t->state = s;
}

/**
Disables interrupts if they were enabled, otherwise does nothing. Can be called
from any privileged mode; the mode will not be changed by calling this
function. Returns the previous mode which when passed to
[kern_restoreInterrupts()](#kern_restoreInterrupts) will restore the previous
mode and interrupt state.
*/
int kern_disableInterrupts() {
#if defined(ARM)
	int result = getCpsr() & 0xFF;
	int newMode = result | KPsrFiqDisable | KPsrIrqDisable;
	ModeSwitchVar(newMode);
	return result;
#elif defined(ARMV7_M)
	int result;
	asm("MRS %0, PRIMASK" : "=r" (result));
	asm("CPSID i");
	return result;
#endif
}

void kern_enableInterrupts() {
#if defined(ARM)
	ModeSwitch(KPsrModeSvc | KPsrFiqDisable);
#elif defined(ARMV7_M)
	asm("CPSIE i");
#endif
}

/**
Restores the interrupt state which was saved by calling
[kern_disableInterrupts()](#kern_disableInterrupts).
*/
void kern_restoreInterrupts(int mask) {
#if defined(ARM)
	ModeSwitchVar(mask);
#elif defined(ARMV7_M)
	if (mask) {
		asm("CPSIE i");
	}
#endif
}

/**
Sleep for a number of milliseconds. Uses the system timer so may sleep up to 1ms
longer. Can only be called from SVC mode with interrupts enabled (otherwise
timers can't fire).
*/
void kern_sleep(int msec) {
#ifdef ARM
	ASSERT((getCpsr() & 0xFF) == (KPsrModeSvc | KPsrFiqDisable));
#endif
	// Use volatile to prevent compiler inlining the while check below
	volatile uint64* uptime = &TheSuperPage->uptime;
	uint64 target = *uptime + msec + 1;
	uint32 zero = 0;
	while (*uptime < target) {
		WFI_inline(zero);
	}
}

// Stack alignment must be 16 bytes and we push one of these directly onto the
// stack
ASSERT_COMPILE((sizeof(TheSuperPage->dfcs) & 0xF) == 0);

static void dfcs_run(int numDfcsPending, Dfc* dfcs);

/**
Call from end of IRQ handler to check for any pending DFCs to run. Runs in IRQ
mode, interrupts disabled. Returns true if a reschedule is required due to there
being any pending DFCs (and if so, will mark the dfcThread as ready to run)
*/
bool irq_checkDfcs() {
	Thread* dfcThread = &TheSuperPage->dfcThread;

	if (dfcThread->state == EReady) {
		// Then DFC thread is already scheduled or running, nothing we can do
		return false;
	}
	uint32 numDfcsPending = atomic_set(&TheSuperPage->numDfcsPending, 0);
	if (numDfcsPending == 0) return false;
	// If we have some DFCs, ready the DFC thread
	thread_setState(dfcThread, EReady);
	// Copy DFCs into dfcThread's stack so that we don't have to access the
	// SuperPage's copy with interrupts enabled.
	uintptr sp = KDfcThreadStack + KPageSize - sizeof(TheSuperPage->dfcs);
	memcpy((void*)sp, TheSuperPage->dfcs, sizeof(TheSuperPage->dfcs));
	// And massage the register set so that scheduleThread() will do the right thing
	dfcThread->savedRegisters[0] = numDfcsPending;
	dfcThread->savedRegisters[1] = sp;
	dfcThread->savedRegisters[13] = sp;
	// See doScheduleKernThread for why we set reg[14] not reg[15]
	dfcThread->savedRegisters[14] = (uintptr)&dfcs_run;
#ifdef ARM
	uint32 psr;
	GetCpsr(psr);
	psr = (psr & ~0xFF) | KPsrModeSvc | KPsrFiqDisable;
	dfcThread->savedRegisters[16] = psr;
#endif
	return true;
}

void dfc_queue(DfcFn fn, uintptr arg1, uintptr arg2, uintptr arg3) {
	uint32 n = atomic_inc(&TheSuperPage->numDfcsPending);
	ASSERT(n <= MAX_DFCS);
	Dfc* dfc = &TheSuperPage->dfcs[n-1];
	dfc->fn = fn;
	dfc->args[0] = arg1;
	dfc->args[1] = arg2;
	dfc->args[2] = arg3;

	/*
	if (n == 1 && (getCpsr() & KPsrModeMask) == KPsrModeSvc) {
		// TODO ready the DFC thread
	}
	*/
}

static void do_request_complete(uintptr arg1, uintptr arg2, uintptr arg3) {
	KAsyncRequest req = {
		.thread = (Thread*)arg1,
		.userPtr = arg2,
	};
	ASSERT(req.userPtr, (uintptr)req.thread);
	thread_requestComplete(&req, (int)arg3);
}

void dfc_requestComplete(KAsyncRequest* request, int result) {
	// We want the DFC to take ownership of the request, so null its userPtr
	// as if it had been completed. Might as well use an atomic.
	uint32 userPtr = atomic_set_uptr(&request->userPtr, 0);
	dfc_queue(do_request_complete, (uintptr)request->thread, userPtr, result);
}

// Run as if it were a normal (albeit kernel-only) thread.
static void dfcs_run(int numDfcsPending, Dfc* dfcs) {
	for (int i = 0; i < numDfcsPending; i++) {
		Dfc* dfc = &dfcs[i];
		dfc->fn(dfc->args[0], dfc->args[1], dfc->args[2]);
	}
	Thread* me = TheSuperPage->currentThread;
	ASSERT(me == &TheSuperPage->dfcThread);
	// Have to do disable before messing with the current thread's state
	kern_disableInterrupts();
	thread_setState(me, EBlockedFromSvc);
	thread_setBlockedReason(me, EBlockedWaitingForDfcs);
	reschedule();
}
