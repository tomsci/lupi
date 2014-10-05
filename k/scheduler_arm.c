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
  the mode specified by spsr\_irq. r13 and r14 are retrieved via `STM ^` if
  spsr\_irq is USR (or System), and by a mode switch to SVC mode otherwise.
* If mode is SVC, `savedRegisters` should point to r14\_svc only (ie usr PC) and
  spsr\_svc really better be Usr or System. r13 and r14 are retreived via `STM ^`.
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

// This runs in IRQ context remember
bool tick() {
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
			bool threadWasInSvc = (getSpsr() & KPsrModeMask) == KPsrModeSvc;
			ASSERT(threadWasInSvc);
			return true;
		}
		if (t->timeslice == 0) {
			// Thread timeslice expired
			thread_yield(t);
			return true;
		}
	}
	return false;
}
