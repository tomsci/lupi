#include <k.h>
#include <armv7-m.h>

#ifndef LUPI_NO_PROCESS
NORETURN NAKED do_process_start(uint32 sp) {
	// We only have one process so we know we must be in initial thread state
	// So setup stack then drop to depriviledged mode and go
	// (If we had more processes we'd have to do an actual reschedule call here
	// or something)
	asm("MSR PSP, r0");
	asm("MOV r2, %0" : : "i" (CONTROL_PSP | CONTROL_NPRIV));
	asm("MSR CONTROL, r2");
	asm("ISB");
	asm("LDR r1, =newProcessEntryPoint");
	asm("BLX r1");
	// Will only return here if thread returns
	asm("B exec_threadExit");
}
#endif

/**
The behaviour of this function is different depending on whether the current
thread is in an SVC call, and what exception handler is currently running.

* If we are being called from the SVCall handler to do an explicit block,
  `savedRegisters` should be NULL. The exception return is not unwound, ie to
  resume the thread you need to trigger an EXC_RETURN from a handler.
* If we are being called from another handler (presumably SysTick) for a
  preempt then `savedRegisters` should point to the {r4-r11} saved on the
  handler stack.
*/
void saveCurrentRegistersForThread(void* savedRegisters) {
	Thread* t = TheSuperPage->currentThread;
	const ExceptionStackFrame* esf = getThreadExceptionStackFrame();
	// There's actually no need to save any of these, since they'e saved in the
	// ESF already
	// t->savedRegisters[0] = esf->r0;
	// t->savedRegisters[1] = esf->r1;
	// t->savedRegisters[2] = esf->r2;
	// t->savedRegisters[3] = esf->r3;
	// t->savedRegisters[12] = esf->r12;
	t->savedRegisters[13] = (uintptr)esf;
	// t->savedRegisters[14] = esf->lr;
	// t->savedRegisters[15] = esf->returnAddress;
	// t->savedRegisters[16] = esf->psr & (~(1 << 9)); // Clear stack frame padding bit
	if (SVCallCurrent()) {
		ASSERT(savedRegisters == NULL, (uintptr)savedRegisters);
	} else {
		uint32* regs = (uint32*)savedRegisters;
		t->savedRegisters[4] = regs[0];
		t->savedRegisters[5] = regs[1];
		t->savedRegisters[6] = regs[2];
		t->savedRegisters[7] = regs[3];
		t->savedRegisters[8] = regs[4];
		t->savedRegisters[9] = regs[5];
		t->savedRegisters[10] = regs[6];
		t->savedRegisters[11] = regs[7];
	}
}

// Must be in handler mode
static NORETURN NAKED doScheduleThread(uint32* savedRegistersR4, uintptr sp, uint32 excReturn) {
	// Restore R4-R11
	asm("LDM r0, {r4-r11}");
	// Set the PSP to this thread's saved SP
	asm("MSR PSP, r1");
	// Remember to clear exclusive 
	asm("CLREX");
	// And perform an exception return. Is this going to work?
	asm("BX r2");
}

NORETURN scheduleThread(Thread* t) {
	t->timeslice = THREAD_TIMESLICE;
	TheSuperPage->currentThread = t;
	doScheduleThread(&t->savedRegisters[4], t->savedRegisters[13], KExcReturnThreadProcess);
}

void pendSV() {
	printk("pendSV\n");
	Dfc dfcs[MAX_DFCS];
	// Interrupts will always be enabled on entry to a configurable exception handler
	kern_disableInterrupts();
	const int n = TheSuperPage->numDfcsPending;
	memcpy(dfcs, TheSuperPage->dfcs, n * sizeof(Dfc));
	TheSuperPage->numDfcsPending = 0;
	kern_enableInterrupts();

	for (int i = 0; i < n; i++) {
		Dfc* dfc = &dfcs[i];
		dfc->fn(dfc->args[0], dfc->args[1], dfc->args[2]);
	}
}

// static void reschedule_dfc(uintptr arg1, uintptr arg2, uintptr arg3) {
// 	//TODO
// }

void NAKED sysTick() {
	asm("PUSH {r4-r12, lr}");
	asm("MOV r0, sp");
	asm("MOV r1, lr");
	asm("BL doTick");
	asm("POP {r4-r12, pc}");
}

static void USED doTick(uint32* regs, uintptr excReturn) {
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
			bool threadWasInSvc = SVCallActive();
			ASSERT(threadWasInSvc);
			atomic_setbool(&TheSuperPage->rescheduleNeededOnSvcExit, true);
		}
		if (t->timeslice == 0) {
			// Thread timeslice expired
			thread_yield(t);
			// TODO dfc_queue(reschedule_dfc, 0, 0, 0);
		}
	}
}

void dfc_queue(DfcFn fn, uintptr arg1, uintptr arg2, uintptr arg3) {
	printk("dfc_queue\n");
	int mask = kern_disableInterrupts();
	uint32 n = ++(TheSuperPage->numDfcsPending);
	ASSERT(n <= MAX_DFCS);
	Dfc* dfc = &TheSuperPage->dfcs[n-1];
	dfc->fn = fn;
	dfc->args[0] = arg1;
	dfc->args[1] = arg2;
	dfc->args[2] = arg3;
	kern_restoreInterrupts(mask);
	PUT32(SCB_ICSR, ICSR_PENDSVSET);
}
