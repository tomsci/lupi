#include <k.h>
#include <armv7-m.h>
#include <exec.h>

#ifndef LUPI_NO_PROCESS

int newProcessEntryPoint();


static NORETURN NAKED do_first_process_start(uint32 sp, uint32 defaultRegVal) {
	// We only have one process so we know we must be in initial thread state
	// So setup stack then drop to depriviledged mode and go
	// (If we had more processes we'd have to do an actual reschedule call here
	// or something)
	asm("MSR PSP, r0");
	asm("MOV r2, %0" : : "i" (CONTROL_PSP | CONTROL_NPRIV));
	asm("MSR CONTROL, r2");
	asm("ISB");
	asm("LDR r0, =newProcessEntryPoint");
	// Clear all other registers to make stack dumps clearer
	asm("MOV r2, r1");
	asm("MOV r3, r1");
	asm("MOV r4, r1");
	asm("MOV r5, r1");
	asm("MOV r6, r1");
	asm("MOV r7, r1");
	asm("MOV r8, r1");
	asm("MOV r9, r1");
	asm("MOV r10, r1");
	asm("MOV r11, r1");
	asm("MOV r12, r1");

	asm("BLX r0");
	// Will only return here if thread returns
	asm("B exec_threadExit");
}

static NAKED NORETURN do_replaced_process_start(uint32 sp, uint32 defaultRegVal) {
	asm("MSR PSP, r0");
	asm("MOV r0, %0" : : "i" (KExcReturnThreadProcess));
	asm("MOV r4, r1");
	asm("MOV r5, r1");
	asm("MOV r6, r1");
	asm("MOV r7, r1");
	asm("MOV r8, r1");
	asm("MOV r9, r1");
	asm("MOV r10, r1");
	asm("MOV r11, r1");
	asm("BX r0");
}

void exec_threadExit(int reason);

#define KRegisterDefaultValue 0xA11FADED

NORETURN do_process_start(uint32 sp) {
	if (TheSuperPage->currentProcess->pid == 1) {
		do_first_process_start(sp, KRegisterDefaultValue);
	} else {
		// We must be doing a process replace from an SVC, but the original
		// stack frame will have been nuked when the Process was reset
		ExceptionStackFrame* esf = pushDummyExceptionStackFrame((uint32*)sp, (uintptr)newProcessEntryPoint);
		esf->r0 = KRegisterDefaultValue;
		esf->r1 = KRegisterDefaultValue;
		esf->r2 = KRegisterDefaultValue;
		esf->r3 = KRegisterDefaultValue;
		esf->r12 = KRegisterDefaultValue;
		esf->lr = (uintptr)exec_threadExit;
		do_replaced_process_start((uintptr)esf, KRegisterDefaultValue);
	}
}

void do_thread_new(Thread* t, uintptr context) {
	uintptr entryPoint;
	asm("LDR %0, =newThreadEntryPoint" : "=r" (entryPoint));
	ExceptionStackFrame* esf = pushDummyExceptionStackFrame((uint32*)t->savedRegisters[KSavedSp], entryPoint);
	esf->r0 = context;
	t->savedRegisters[KSavedSp] = (uintptr)esf;
}

#endif // LUPI_NO_PROCESS

/**
The behaviour of this function is different depending on whether the current
thread is in an SVC call, and what exception handler is currently running.

* If we are being called from the SVCall handler to do an explicit block,
  `savedRegisters` should be NULL. The exception return is not unwound, ie to
  resume the thread you need to trigger an EXC_RETURN from a handler.
* If we are being called from another handler (ie PendSV) for a
  preempt then `savedRegisters` should point to the {r4-r11} saved on the
  handler stack.
*/
void saveCurrentRegistersForThread(void* savedRegisters) {
	Thread* t = TheSuperPage->currentThread;
	const ExceptionStackFrame* esf = getThreadExceptionStackFrame();
	t->savedRegisters[KSavedSp] = (uintptr)esf;
	if (SVCallCurrent()) {
		ASSERT(savedRegisters == NULL, (uintptr)savedRegisters);
	} else {
		uint32* regs = (uint32*)savedRegisters;
		memcpy(t->savedRegisters, regs, 8*sizeof(uint32));
	}
}

static NORETURN NAKED doScheduleThread(uint32* savedRegisters) {
	asm("LDM r0!, {r4-r11}"); // restore r4-r11
	// r0 is now pointing to saved SP
	// Set the PSP to this thread's saved SP
	asm("LDR r0, [r0]");
	asm("MSR PSP, r0");
	// Remember to clear exclusive
	asm("CLREX");

	// Reset the handler stack pointer, it'll be a bit of a mess by now
	// This is ok as we are only ever called in svc or pendsv which are the
	// lowest priority handler so there can never be any other handler active
	asm("MOV r1, %0" : : "i" (KKernelCodeBase));
	asm("LDR sp, [r1]");

	// And perform an exception return
	asm("LDR r2, .excReturnThreadProcess");
	asm("BX r2");

	LABEL_WORD(.excReturnThreadProcess, KExcReturnThreadProcess);
}

static NORETURN USED scheduleThread(Thread* t) {
	t->timeslice = THREAD_TIMESLICE;
	TheSuperPage->currentThread = t;
	// printk("Scheduling thread\n");
	doScheduleThread(t->savedRegisters);
}

void thread_writeSvcResult(Thread* t, uintptr result) {
	ExceptionStackFrame* esf = (ExceptionStackFrame*)(t->savedRegisters[KSavedSp]);
	esf->r0 = result;
}

static void setPendSvPriority(uint8 priority) {
	PUT8(SHPR_PENDSV, priority);
	asm("ISB");
	asm("DSB");
}

Thread* findNextReadyThread();

NORETURN reschedule() {
	Thread* t = findNextReadyThread();
	if (t) {
		scheduleThread(t);
		// Doesn't return
	}

	TheSuperPage->currentThread = NULL;
	// Bump pendsv priority so it can run during the WFI
	// in case of ISRs that need a DFC to make a thread ready
	setPendSvPriority(KPriorityWfiPendsv);

	for (;;) {
		WFI();
		kern_disableInterrupts(); // Could use BASEPRI instead here
		t = findNextReadyThread();
		if (t) {
			break;
		}
		kern_enableInterrupts();
	}

	// Stop any further pendSVs during SVC
	setPendSvPriority(KPrioritySvc);
	kern_enableInterrupts();
	scheduleThread(t);
}

NAKED void pendSV() {
	asm("PUSH {r4-r12, lr}");
	asm("MOV r0, sp");
	asm("BL doPendSv");
	asm("POP {r4-r12, pc}");
}

static void USED doPendSv(void* savedRegisters) {
#ifdef TIMER_DEBUG
	TheSuperPage->lastPendSvTime = (TheSuperPage->uptime << 16) + GET32(SYSTICK_VAL);
#endif
	// printk("+pendSV\n");
	Dfc dfcs[MAX_DFCS];
	// Interrupts will always be enabled on entry to a configurable exception handler
	kern_disableInterrupts();
	const int n = TheSuperPage->numDfcsPending;
	if (n) {
		memcpy(dfcs, TheSuperPage->dfcs, n * sizeof(Dfc));
		TheSuperPage->numDfcsPending = 0;
		kern_enableInterrupts();

		for (int i = 0; i < n; i++) {
			Dfc* dfc = &dfcs[i];
			dfc->fn(dfc->args[0], dfc->args[1], dfc->args[2]);
		}
	} else {
		kern_enableInterrupts();
	}

	// Last thing the pendSV handler does is to check for thread timeslice expired
	if (atomic_setbool(&TheSuperPage->rescheduleNeededOnPendSvExit, false)) {
#ifdef TIMER_DEBUG
		TheSuperPage->lastRescheduleTime = (TheSuperPage->uptime << 16) + GET32(SYSTICK_VAL);
#endif
		// printk("Rescheduling\n");
		saveCurrentRegistersForThread(savedRegisters);
		reschedule();
	}
	// printk("-pendSV\n");
}

void sysTick() {
	// printk("+Tick!\n");
	SuperPage* const s = TheSuperPage;
	s->uptime++;
	if (s->uptime == s->timerCompletionTime) {
		s->timerCompletionTime = UINT64_MAX;
		// printk("Queueing timer completion\n");
		dfc_requestComplete(&s->timerRequest, 0);
		// printk("Done\n");
	}
	Thread* t = s->currentThread;
	if (t && t->state == EReady) {
		if (t->timeslice > 0) {
			t->timeslice--;
		} else {
			// This is only allowed if the PendSv handler has not yet run to
			// reschedule the thread
			ASSERT(SvOrPendSvActive() || TheSuperPage->rescheduleNeededOnPendSvExit);
		}
		if (t->timeslice == 0) {
			// Thread timeslice expired
			// printk("timeslice expired\n");
			thread_yield(t);
			atomic_setbool(&TheSuperPage->rescheduleNeededOnPendSvExit, true);
			PUT32(SCB_ICSR, ICSR_PENDSVSET);
		}
	}
	// printk("-Tick!\n");
}

void dfc_queue(DfcFn fn, uintptr arg1, uintptr arg2, uintptr arg3) {
	int mask = kern_disableInterrupts();
	// printk("+dfc_queue\n");
	uint8 n = ++(TheSuperPage->numDfcsPending);
	ASSERT(n <= MAX_DFCS);
	Dfc* dfc = &TheSuperPage->dfcs[n-1];
	dfc->fn = fn;
	dfc->args[0] = arg1;
	dfc->args[1] = arg2;
	dfc->args[2] = arg3;
	// printk("-dfc_queue\n");
	kern_restoreInterrupts(mask);
	PUT32(SCB_ICSR, ICSR_PENDSVSET);
}
