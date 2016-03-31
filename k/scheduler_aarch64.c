#include <k.h>
#include <mmu.h>
#include ARCH_HEADER

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
