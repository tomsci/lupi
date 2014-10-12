#include <k.h>
#include <armv7-m.h>
#include <exec.h>

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
	asm("LDR r0, =newProcessEntryPoint");
	// Clear all other registers to make stack dumps clearer
	asm("LDR r1, .clearReg");
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

	LABEL_WORD(.clearReg, 0xA11FADED);
}
#endif // LUPI_NO_PROCESS

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
	// There's actually no need to save any of these, since they're saved in the
	// ESF already. However in various places we set t->savedRegisters[0] as
	// part of SVC return so we will save and restore that too.
	t->savedRegisters[0] = esf->r0;
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

static NORETURN NAKED doScheduleThread(uint32* savedRegisters) {
	asm("MOV r12, r0"); // R12 = &savedRegisters[0]
	asm("ADD r3, r12, %0" : : "i" (4 * 4)); // r3 = &savedRegisters[4]
	asm("LDM r3, {r4-r11}"); // restore r4-r11
	// Set the PSP to this thread's saved SP
	asm("LDR r3, [r12, %0]" : : "i" (13 * 4)); // r3 = savedRegisters[13]
	asm("MSR PSP, r3");
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
	// Restore r0 in the exception frame from the saved registers. This is
	// because we set savedRegisters[0] in various places to the svc return
	// value, because the ARM model would always restore it as a matter of
	// course. Under ARMv7-M this isn't the case so we will restore it
	// explicitly in lieu of making a tidier generic mechanism that makes sense
	// on both ARM and ARMv7-M.
	getThreadExceptionStackFrame()->r0 = t->savedRegisters[0];
	// printk("Scheduling thread\n");
	doScheduleThread(t->savedRegisters);
}

Thread* findNextReadyThread();
void PUT8(uint32 addr, byte val);

NORETURN reschedule() {
	Thread* t = findNextReadyThread();
	if (t) {
		scheduleThread(t);
		// Doesn't return
	}

	TheSuperPage->currentThread = NULL;
	// Bump pendsv priority so it can run during the WFI
	PUT8(SHPR_PENDSV, KPriorityWfiPendsv);

	while (t == NULL) {
		WFI();
		kern_disableInterrupts(); // Could use BASEPRI instead here
		t = findNextReadyThread();
		kern_enableInterrupts();
	}

	// Stop any further pendSVs during SVC
	PUT8(SHPR_PENDSV, KPrioritySvc);
	scheduleThread(t);
}

void pendSV() {
	// printk("pendSV\n");
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

	// Last thing the pendSV handler does is to check for thread timeslice
	// expired during SVC
	if (atomic_setbool(&TheSuperPage->rescheduleNeededOnSvcExit, false)) {
		saveCurrentRegistersForThread(NULL);
		// Save the result also
		reschedule();
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
	// printk("+Tick!\n");
	SuperPage* const s = TheSuperPage;
	s->uptime++;
	if (s->uptime == s->timerCompletionTime) {
		s->timerCompletionTime = UINT64_MAX;
		dfc_requestComplete(&s->timerRequest, 0);
	}
	/*TODO
	Thread* t = s->currentThread;
	if (t && t->state == EReady) {
		if (t->timeslice > 0) {
			t->timeslice--;
		} else {
			// This is only allowed if the thread is (still!) in an SVC,
			bool threadWasInSvc = SvOrPendSvActive();
			ASSERT(threadWasInSvc);
			atomic_setbool(&TheSuperPage->rescheduleNeededOnSvcExit, true);
			// And make sure PendSV is set
			printk("Setting pendsv due to tick thread timeslice 0 in svc\n");
			PUT32(SCB_ICSR, ICSR_PENDSVSET);
		}
		if (t->timeslice == 0) {
			// Thread timeslice expired
			printk("timeslice expired\n");
			thread_yield(t);
			// TODO dfc_queue(reschedule_dfc, 0, 0, 0);
		}
	}
	*/
	// printk("-Tick!\n");
}

void dfc_queue(DfcFn fn, uintptr arg1, uintptr arg2, uintptr arg3) {
	// printk("dfc_queue\n");
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
