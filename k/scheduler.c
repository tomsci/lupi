#include <k.h>
#include <mmu.h>
#include <arm.h>

void saveUserModeRegistersForCurrentThread(void* savedRegisters) {
	Thread* t = TheSuperPage->currentThread;
	// savedRegisters[0..8] contains r4-r12
	memcpy(&t->savedRegisters[4], savedRegisters, 9 * sizeof(uint32));
	// savedRegisters[9] has LR_svc, which is what PC_usr needs to be restored to
	t->savedRegisters[15] = ((uint32*)savedRegisters)[9];
	uint32* dest = &t->savedRegisters[13];
	ASM_JFDI("STM %0, {r13-r14}^" : : "r" (dest)); // Saves the user (banked) r13 and r14
	uint32 userPsr;
	asm("MRS %0, spsr" : "=r" (userPsr));
	t->savedRegisters[16] = userPsr;

	//worddump((char*)&t->savedRegisters[0], 16*4);
}

Thread* findNextReadyThread(Thread* current) {
	// Find next ready thread
	Thread* t = current == NULL ? firstThreadForProcess(GetProcess(0)) : current->nextSchedulable;
	for (; t != current; t = t->nextSchedulable) {
		if (t->state == EReady) {
			return t;
		}
	}
	if (current->state == EReady) {
		// Given all the other threads a chance, current can run again
		return current;
	}
	return NULL;
}

static NORETURN NAKED doScheduleThread(uint32* savedRegisters, uint32 spsr) {
	// Important to reset r13_svc because it might be pointed up at whatever called reschedule()
	asm("LDR r13, .svcStack");
	asm("MSR spsr_c, r1"); // make sure the 'S' returns us to the correct mode
	asm("LDR r14, [r0, #60]"); // r14 = savedRegisters[15]

	ASM_JFDI("LDM r0, {r0-r14}^");
	asm("MOVS pc, r14");
	// And we're done
	LABEL_WORD(.svcStack, KKernelStackBase + KKernelStackSize);
}

// For now assume we're in SVC
NORETURN scheduleThread(Thread* t) {
	Process* p = processForThread(t);
	if (p != TheSuperPage->currentProcess) {
		switch_process(p);
	}
	TheSuperPage->currentThread = t;
	doScheduleThread(t->savedRegisters, t->spsr);
}

NORETURN NAKED reschedule(Thread* current) {
	asm(".doReschedule:");
	asm("BL findNextReadyThread");
	asm("CMP r0, #0");
	asm("BNE scheduleThread");

	// If we get here, no more threads to run, need to just WFI
	// But in order to do that we need to safely reenable interrupts
	DSB(r0);
	ModeSwitch(KPsrModeSvc | KPsrFiqDisable); // Reenable interrupts
	WFI(r0);
	asm("B .doReschedule");
}

// This runs in IRQ context remember
void tick() {
	TheSuperPage->uptime++;
}
