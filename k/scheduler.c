#include <k.h>
#include <mmu.h>
#include <arm.h>

// Assumes we were in IRQ mode with interrupts off to start with
static NAKED NOINLINE void irq_saveSvcSpLr(uint32* splr) {
	ModeSwitch(KPsrModeSvc | KPsrIrqDisable | KPsrFiqDisable);
	asm("STM r0, {r13-r14}");
	ModeSwitch(KPsrModeIrq | KPsrIrqDisable | KPsrFiqDisable);
	asm("BX lr");
}

/**
The behaviour of this function is different depending on the mode it is called
in, and the mode the current thread is in (as indicated by the current value of
spsr).

* If mode is IRQ, `savedRegisters` should point to registers {r0-r12, pc} for
  the mode specified by spsr_irq. r13 and r14 are retrieved via `STM ^` if
  spsr_irq is USR (or System), and by a mode switch to SVC mode otherwise.
* If mode is SVC, `savedRegisters` should point to r14\_svc only (ie usr PC) and
  spsr_svc really better be Usr or System. r13 and r14 are retreived via `STM ^`.
*/
void saveCurrentRegistersForThread(void* savedRegisters) {
	Thread* t = TheSuperPage->currentThread;
	uint32 cpsr = getCpsr();
	uint32 spsr = getSpsr();
	t->savedRegisters[16] = spsr;
	bool irq = (cpsr & KPsrModeMask) == KPsrModeIrq;
	uint32* splr = &t->savedRegisters[13];
	if (irq) {
		memcpy(&t->savedRegisters[0], savedRegisters, 13 * sizeof(uint32));
		// LR_irq in savedRegisters[13] is 4 more than what PC_usr needs to be restored to
		t->savedRegisters[15] = ((uint32*)savedRegisters)[13] - 4;
		if ((spsr & KPsrModeMask) == KPsrModeSvc) {
			// Have to mode switch to retrieve r13 and r14
			ASSERT(false, 0x5C4D); // We don't currently ever save SVC registers
			irq_saveSvcSpLr(splr);
		} else {
			ASM_JFDI("STM %0, {r13-r14}^" : : "r" (splr)); // Saves the user (banked) r13 and r14
		}
	} else {
		t->savedRegisters[15] = ((uint32*)savedRegisters)[0];
		ASM_JFDI("STM %0, {r13-r14}^" : : "r" (splr)); // Saves the user (banked) r13 and r14
	}
	//printk("Saved registers thread %d-%d ts=%d\n", indexForProcess(processForThread(t)), t->index, t->timeslice);
	//worddump((char*)&t->savedRegisters[0], sizeof(t->savedRegisters));
}

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

static NORETURN NAKED doScheduleUserThread(uint32* savedRegisters, uint32 spsr) {
	asm("MSR spsr, r1"); // make sure the 'S' returns us to the correct mode
	asm("LDR r14, [r0, #60]"); // r14 = savedRegisters[15]

	ASM_JFDI("LDM r0, {r0-r14}^");
	// Make sure any atomic ops know they've been interrupted
	asm("CLREX");
	asm("MOVS pc, r14");
	// And we're done
}

// IMPORTANT NOTE: This is an incomplete implementation, which does not support
// cleanly resuming an arbitrary kernel thread. It requires that
// savedRegisters[14] is set to the address to branch to, NOT savedRegisters[15]
// Therefore it can only be used to start execution, not resume it.
static NORETURN NAKED doScheduleKernThread(uint32* savedRegisters, uint32 spsr) {
	asm("MSR spsr, r1"); // make sure the 'S' returns us to the correct mode
	ASM_JFDI("LDM r0, {r0-r14}");
	// Make sure any atomic ops know they've been interrupted
	asm("CLREX");
	asm("MOVS pc, r14");
	// And we're done
}

// Enter in SVC
NORETURN scheduleThread(Thread* t) {
	Process* p = processForThread(t);
	switch_process(p);
	t->timeslice = THREAD_TIMESLICE;
	TheSuperPage->currentThread = t;
	//printk("Scheduling thread %d-%d ts=%d\n", indexForProcess(p), t->index, t->timeslice);
	//worddump(t->savedRegisters, sizeof(t->savedRegisters));
	uint32 spsr = t->savedRegisters[16];
	uint32 mode = spsr & KPsrModeMask;
	if (mode == KPsrModeUsr) {
		doScheduleUserThread(t->savedRegisters, spsr);
	} else if (mode == KPsrModeSvc) {
		//printk("Scheduling kern thread %p\n", t);
		//worddump(t->savedRegisters, sizeof(t->savedRegisters));
		doScheduleKernThread(t->savedRegisters, spsr);
	} else {
		ASSERT(false, spsr);
	}
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

/**
Call this instead of reschedule() to reschedule when in IRQ mode. Performs
the appropriate cleanup and mode switching. Does not return, so should only be
called at the very end of the IRQ handler.
*/
NORETURN NAKED reschedule_irq() {
	// Reset IRQ stack and call reschedule in SVC mode (so nothing else messes stack up again)
	asm("LDR r13, .irqStack");
	asm("MOV r1, #0");
	DSB(r1);
	ModeSwitch(KPsrModeSvc | KPsrFiqDisable | KPsrIrqDisable);
	GetKernelStackTop(AL, r13);
	asm("B reschedule");
	LABEL_WORD(.irqStack, KIrqStackBase + KPageSize);
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

// This runs in IRQ context remember
bool tick(void* savedRegs) {
	SuperPage* const s = TheSuperPage;
	s->uptime++;
	if (s->uptime == s->timerCompletionTime) {
		s->timerCompletionTime = UINT64_MAX;
		dfc_requestComplete(&s->timerRequest, 0);
	}
	Thread* t = s->currentThread;
	if (t && t->state == EReady) {
		if (t->timeslice > 0) {
			t->timeslice--;
		} else {
			// This is only allowed if the thread is (still!) in an SVC,
			// which probably shouldn't ever happen
			uint32 spsr;
			GetSpsr(spsr);
			bool threadWasInSvc = (spsr & KPsrModeMask) == KPsrModeSvc;
			ASSERT(threadWasInSvc);
		}
		if (t->timeslice == 0) {
			thread_yield(t);
			// Thread is out of time!
			// Check if we can preempt directly or whether to wait for svc exit
			uint32 spsr = getSpsr();
			bool threadWasInSvc = (spsr & KPsrModeMask) == KPsrModeSvc;
			if (threadWasInSvc) {
				atomic_setbool(&s->rescheduleNeededOnSvcExit, true);
				return false; // Don't reschedule immediately
			} else {
				saveCurrentRegistersForThread(savedRegs);
				return true; // Reschedule right now
			}
		}
	}
	return false;
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
	int result = getCpsr() & 0xFF;
	int newMode = result | KPsrFiqDisable | KPsrIrqDisable;
	ModeSwitchVar(newMode);
	return result;
}

void kern_enableInterrupts() {
	ModeSwitch(KPsrModeSvc | KPsrFiqDisable);
}

/**
Restores the interrupt state which was saved by calling
[kern_disableInterrupts()](#kern_disableInterrupts).
*/
void kern_restoreInterrupts(int mask) {
	ModeSwitchVar(mask);
}

// Stack alignment must be 16 bytes and we push one of these directly onto the
// stack
ASSERT_COMPILE((sizeof(TheSuperPage->dfcs) & 0xF) == 0);

static void dfcs_run(int numDfcsPending, Dfc* dfcs);

/**
Call from end of IRQ handler to check for any pending DFCs to run. Runs in IRQ
mode, interrupts disabled. Will not return if there are pending DFCs, therefore
should only be called as the last thing the IRQ handler does (except for
calling reschedule_irq if tick() returned true).

`savedRegisters` can be NULL if the IRQ handler has called tick and it returned
true.
*/
void irq_checkDfcs(void* savedRegisters) {
	uint32 numDfcsPending = atomic_set(&TheSuperPage->numDfcsPending, 0);
	if (numDfcsPending == 0) return;
	// If we have some DFCs, ready the DFC thread
	Thread* dfcThread = &TheSuperPage->dfcThread;
	Thread* currentThread = TheSuperPage->currentThread;
	if (currentThread == dfcThread) {
		// We interrupted the DFC thread, aargh
		ASSERT(false);
		return;
	}
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
	uint32 psr;
	GetCpsr(psr);
	psr = (psr & ~0xFF) | KPsrModeSvc | KPsrFiqDisable;
	dfcThread->savedRegisters[16] = psr;

	// Preempt the current thread
	if (currentThread && savedRegisters) {
		saveCurrentRegistersForThread(savedRegisters);
	}

	reschedule_irq();
	// Doesn't return, and also will not enable interrupts until it has set
	// currentThread (which is important in ensuring this code works reenterantly)
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
