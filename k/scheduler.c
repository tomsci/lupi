#include <k.h>
#include <mmu.h>
#include <arm.h>

void saveUserModeRegistersForCurrentThread(uint32* savedRegisters, bool svc) {
	Thread* t = TheSuperPage->currentThread;
	if (svc) {
		// savedRegisters[0] has LR_svc and that's it
		// (LR_svc is what PC_usr needs to be restored to)
		t->savedRegisters[15] = ((uint32*)savedRegisters)[0];
	} else {
		// savedRegisters[0..12] contains r0-r12 and savedRegisters[13] has LR_irq
		memcpy(&t->savedRegisters[0], savedRegisters, 13 * sizeof(uint32));
		// LR_irq in savedRegisters[13] is 4 more than what PC_usr needs to be restored to
		t->savedRegisters[15] = savedRegisters[13] - 4;
	}
	uint32* splr = &t->savedRegisters[13];
	ASM_JFDI("STM %0, {r13-r14}^" : : "r" (splr)); // Saves the user (banked) r13 and r14
	uint32 userPsr;
	asm("MRS %0, spsr" : "=r" (userPsr));
	t->savedRegisters[16] = userPsr;

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
	ModeSwitch(KPsrModeSvc | KPsrFiqDisable); // Reenable interrupts
	WFI(r0);
	ModeSwitch(KPsrModeSvc | KPsrFiqDisable | KPsrIrqDisable); // Disable interrupts again, just to be safe
	asm("B .doReschedule");
	LABEL_WORD(.TheCurrentThreadAddr, &TheSuperPage->currentThread);
}

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
			saveUserModeRegistersForCurrentThread(savedRegs, false);
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
	} else {
		dequeue(t);
	}
	t->state = s;
}
