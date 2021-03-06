#include <k.h>
#include <mmu.h>
#include ARCH_HEADER

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
Sleep for a number of milliseconds. Uses the system timer so may sleep up to 1ms
longer. Can only be called from SVC mode with interrupts enabled (otherwise
timers can't fire).
*/
void kern_sleep(int msec) {
#if defined(ARM)
	ASSERT((getCpsr() & 0xFF) == (KPsrModeSvc | KPsrFiqDisable));
#elif defined(ARMV7_M)
	uint32 primask;
	READ_SPECIAL(PRIMASK, primask);
	ASSERT(primask == 0);
#endif
	// Use volatile to prevent compiler inlining the while check below
	volatile uint64* uptime = &TheSuperPage->uptime;
	uint64 target = *uptime + msec + 1;
	uint32 zero = 0;
	while (*uptime < target) {
		WFI_inline(zero);
	}
}

static void do_request_complete(uintptr arg1, uintptr arg2, uintptr arg3) {
	// printk("do_request_complete t=%X ptr=%X\n", (uint)arg1, (uint)arg2);
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
