#include <k.h>
#include <mmu.h>
#include ARCH_HEADER

int kern_disableInterrupts() {
	uint64 result;
	READ_SPECIAL(DAIF, result);
	WRITE_SPECIAL(DAIF, (uint64)(SPSR_D | SPSR_A | SPSR_I | SPSR_F));
	return (int)result;
}

void kern_enableInterrupts() {
	WRITE_SPECIAL(DAIF, (uint64)(SPSR_F));
}

void kern_restoreInterrupts(int mask) {
	WRITE_SPECIAL(DAIF, (uint64)mask);
}

void exec_threadExit(int reason);

bool do_thread_init(Thread* t, uintptr entryPoint, uintptr context) {
	t->savedRegisters[0] = context;
	t->savedRegisters[14] = (uintptr)exec_threadExit;
	t->savedRegisters[KSavedSpsr] = 0;
	t->savedRegisters[KSavedPc] = entryPoint;
	return true;
}

NORETURN NAKED doScheduleThread(uintptr* savedRegisters, uintptr sp, uintptr spsr, uintptr pc) {
	asm("MOV x31, x0"); // x31 = savedRegisters (that is, sp is being used as a temporary)
	asm("MSR SP_EL0, x1");
	asm("MSR SPSR_EL1, x2");
	asm("MSR ELR_EL1, x3");
	LOAD_ALL_REGISTERS();
	RESET_SP_EL1_AND_ERET();
}

NORETURN scheduleThread(Thread* t) {
	Process* p = processForThread(t);
	switch_process(p);
	t->timeslice = THREAD_TIMESLICE;
	TheSuperPage->currentThread = t;
	doScheduleThread(t->savedRegisters, t->savedRegisters[KSavedSp], t->savedRegisters[KSavedSpsr], t->savedRegisters[KSavedPc]);
}

NORETURN do_process_start(uintptr sp) {
	scheduleThread(TheSuperPage->currentThread);
}

void thread_writeSvcResult(Thread* t, uintptr result) {
	t->savedRegisters[0] = result;
}

void dfc_queue(DfcFn fn, uintptr arg1, uintptr arg2, uintptr arg3) {
	uint32 n = atomic_inc8(&TheSuperPage->numDfcsPending);
	ASSERT(n <= MAX_DFCS);
	Dfc* dfc = &TheSuperPage->dfcs[n-1];
	dfc->fn = fn;
	dfc->args[0] = arg1;
	dfc->args[1] = arg2;
	dfc->args[2] = arg3;
}

/**
This is almost a no-op on aarch64 as we always save thread general registers on
entry to svc().
*/
void saveCurrentRegistersForThread(void* savedRegisters) {
	Thread* t = TheSuperPage->currentThread;
	READ_SPECIAL(ELR_EL1, t->savedRegisters[KSavedPc]);
	READ_SPECIAL(SPSR_EL1, t->savedRegisters[KSavedSpsr]);
	//printk("Saved registers thread %d-%d ts=%d\n", indexForProcess(processForThread(t)), t->index, t->timeslice);
	//worddump((char*)&t->savedRegisters[0], sizeof(t->savedRegisters));
}

NORETURN reschedule() {
	//  TODO
	ASSERT(false);
}

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
			// TODO
			// bool threadWasInSvc = (getSpsr() & KPsrModeMask) == KPsrModeSvc;
			// ASSERT(threadWasInSvc);
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
