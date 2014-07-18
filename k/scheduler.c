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
* If mode is SVC, `savedRegisters` should point to r14_svc only (ie usr PC) and
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

static NORETURN NAKED doScheduleThread(uint32* savedRegisters, uint32 spsr) {
	asm("MSR spsr, r1"); // make sure the 'S' returns us to the correct mode
	asm("LDR r14, [r0, #60]"); // r14 = savedRegisters[15]

	ASM_JFDI("LDM r0, {r0-r14}^");
	asm("MOVS pc, r14");
	// And we're done
}

// For now assume we're in SVC
NORETURN scheduleThread(Thread* t) {
	Process* p = processForThread(t);
	switch_process(p);
	t->timeslice = THREAD_TIMESLICE;
	TheSuperPage->currentThread = t;
	//printk("Scheduling thread %d-%d ts=%d\n", indexForProcess(p), t->index, t->timeslice);
	//worddump(t->savedRegisters, sizeof(t->savedRegisters));
	doScheduleThread(t->savedRegisters, t->savedRegisters[16]);
}

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

static void dequeue(Thread* t) {
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
	dequeue(t);
	thread_enqueueBefore(t, s->readyList ? s->readyList->prev : NULL);
	if (!s->readyList) s->readyList = t;
}

// This runs in IRQ context remember
bool tick(void* savedRegs) {
	SuperPage* const s = TheSuperPage;
	s->uptime++;
	if (s->uptime == s->timerCompletionTime) {
		s->timerCompletionTime = UINT64_MAX;
		thread_requestComplete(&s->timerRequest, 0);
	}
	Thread* t = s->currentThread;
	if (t && t->state == EReady) {
		ASSERT(t->timeslice > 0); // Otherwise it shouldn't have been running
		t->timeslice--;
		if (t->timeslice == 0) {
			// Thread is out of time!
			saveCurrentRegistersForThread(savedRegs);
			// Move to end of ready list
			dequeue(t);
			thread_enqueueBefore(t, s->readyList ? s->readyList->prev : NULL);
			if (!s->readyList) s->readyList = t;
			return true;
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
		dequeue(t);
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
