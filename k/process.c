#include <k.h>
#include <pageAllocator.h>
#include <mmu.h>
#ifdef ARM
#include <arm.h>
#endif
#include <err.h>
#include <ipc.h>
#include <module.h>

#define KNumPreallocatedUserPages 0

// These will refer to the *user* addresses of these variables, in the BSS.
// Therefore, can only be referenced when process_switch()ed to the
// appropriate Process.
extern uint32 user_ProcessPid;
extern char user_ProcessName[];

#ifndef LUPI_NO_PROCESS
static bool thread_init(Thread* t);
NORETURN do_process_start(uint32 sp);
void do_thread_new(Thread* t, uintptr context);
#endif

int strlen(const char *s);

static int process_init(Process* p, const char* processName) {
	if (strlen(processName) >= MAX_PROCESS_NAME) {
		return KErrBadName;
	}

#ifdef LUPI_NO_PROCESS
	return KErrNotSupported;
#else
	// Do an early check that processName is valid - easier on callers if we fail now rather than
	// once we've actually started executing the process
	const LuaModule* module = getLuaModule(processName);
	if (!module) return KErrNotFound;

	// Assume the Process page itself is already mapped, but nothing else necessarily is
	p->pid = TheSuperPage->nextPid++;

#ifdef HAVE_MMU
	uint32* pde = (uint32*)PDE_FOR_PROCESS(p);
	uint32* kernPtForTheUserPts = (uint32*)KERN_PT_FOR_PROCESS_PTS(p);
	if (!p->pdePhysicalAddress) {
		p->pdePhysicalAddress = mmu_mapPageInSection(Al, (uint32*)KProcessesPdeSection_pt, (uintptr)pde, KPageUserPde);
		uintptr userPtsStart = (uintptr)PT_FOR_PROCESS(p, 0);
		mmu_mapSection(Al, userPtsStart, (uintptr)kernPtForTheUserPts, (uint32*)KKernPtForProcPts_pt, KPageKernPtForProcPts);
		//TODO check return code
	}
	zeroPage(pde);
	mmu_mapPagesInProcess(Al, p, KUserBss, 1 + KNumPreallocatedUserPages);
#endif // HAVE_MMU

	p->heapLimit = KUserHeapBase;

	// Setup initial thread
	p->numThreads = 1;
	bool ok = thread_init(&p->threads[0]);
	if (!ok) return KErrNoMemory;

	char* pname = p->name;
	char ch;
	do {
		ch = *processName++;
		*pname++ = ch;
	} while (ch);

	thread_setState(&p->threads[0], EReady);

	return 0;
#endif // LUPI_NO_PROCESS
}

#ifndef LUPI_NO_PROCESS

static bool thread_init(Thread* t) {
	t->prev = NULL;
	t->next = NULL;
	t->state = EDead;
	t->timeslice = THREAD_TIMESLICE;
	t->completedRequests = 0;
	uintptr stackBase = userStackForThread(t);
#ifdef HAVE_MMU
	Process* p = processForThread(t);
	bool ok = mmu_mapPagesInProcess(Al, p, stackBase, USER_STACK_SIZE >> KPageShift);
	if (!ok) return false;
	ok = mmu_mapSvcStack(Al, p, svcStackBase(t->index));
	if (!ok) {
		mmu_unmapPagesInProcess(Al, p, stackBase, USER_STACK_SIZE >> KPageShift);
		return false;
	}
#endif // HAVE_MMU

	t->savedRegisters[KSavedR13] = stackBase + USER_STACK_SIZE;
	if ((KUserBss & ~0xFFF) == stackBase) {
		// Special case for the case where we stuff the BSS into the top of the
		// stack page
		t->savedRegisters[KSavedR13] = KUserBss;
	}
	return true;
}

NORETURN process_start(Process* p) {
	switch_process(p);
	Thread* t = firstThreadForProcess(p);
	TheSuperPage->currentThread = t;
	// Now we've switched process and mapped the BSS, we first need to zero all initial memory
	if ((KUserBss & 0xFFF) == 0) {
		// If BSS is mapped to a page boundary, assume we need to clear it
		zeroPages((void*)KUserBss, 1 + KNumPreallocatedUserPages);
	}
	zeroPages((void*)userStackForThread(t), USER_STACK_SIZE >> KPageShift);

	// And we can set up the user_* variables
	user_ProcessPid = p->pid;
	char* userpname = user_ProcessName;
	const char* pname = p->name;
	// Poor man's strcpy
	char ch;
	do {
		ch = *pname++;
		*userpname++ = ch;
	} while (ch);
	uint32 sp = t->savedRegisters[KSavedR13];
	do_process_start(sp);
}

#endif // LUPI_NO_PROCESS

bool process_grow_heap(Process* p, int incr) {
	//printk("+process_grow_heap %d heapLimit=%p\n", incr, (void*)p->heapLimit);
	bool dec = (incr < 0);
	int amount;
	if (dec) {
		amount = (-incr) & ~(KPageSize - 1); // Be sure not to free more than was asked to
	} else {
#ifndef HAVE_MMU
		// There isn't any real restriction we need apply, and uluaHeap benefits
		// from being able to ask for less than a page
		amount = incr;
#else
		amount = PAGE_ROUND(incr);
#endif
	}

	if (dec) {
		if (p->heapLimit - amount < KUserHeapBase) {
			amount = p->heapLimit - KUserHeapBase;
		}
		p->heapLimit = p->heapLimit - amount;
#ifdef HAVE_MMU
		mmu_unmapPagesInProcess(Al, p, p->heapLimit, amount >> KPageShift);
		mmu_finishedUpdatingPageTables();
#endif
		return true;
	} else {
		const int npages = amount >> KPageShift;
#ifdef HAVE_MMU
		bool ok = mmu_mapPagesInProcess(Al, p, p->heapLimit, npages);
		if (!ok) {
			return false;
		}
		mmu_finishedUpdatingPageTables();
#else
		// With no MMU heap grows until it hits the stacks
		const uint32 heapLim = userStackForThread(&p->threads[p->numThreads-1]);
		if (p->heapLimit + amount > heapLim) {
			printk("OOM @ heapLimit = %X + %d > %X!\n", (uint)p->heapLimit, incr, heapLim);
			return false;
		}
#endif
		zeroPages((void*)p->heapLimit, npages);
		p->heapLimit += amount;
		//printk("-process_grow_heap heapLimit=%p\n", (void*)p->heapLimit);
		return true;
	}
}

int process_new(const char* name, Process** resultProcess) {
	*resultProcess = NULL;
	// First see if there is a spare Process* we can use
	SuperPage* s = TheSuperPage;
	Process* p = 0;
	int err = KErrNoMemory;
	for (int i = 0; i < s->numValidProcessPages; i++) {
		Process* candidate = GetProcess(i);
		if (candidate->pid == 0) {
			p = candidate;
			break;
		}
	}
#if MAX_PROCESSES > 1
	if (!p && s->numValidProcessPages < MAX_PROCESSES) {
		// Map ourselves a new one
		Process* newp = GetProcess(s->numValidProcessPages);
		uintptr phys = mmu_mapPageInSection(Al, (uint32*)KProcessesSection_pt, (uintptr)newp, KPageProcess);
		if (phys) {
			mmu_finishedUpdatingPageTables();
			p = newp;
			p->pdePhysicalAddress = 0;
			s->numValidProcessPages++;
		}
	}
#endif

	if (!p) {
		return KErrResourceLimit;
	}

	if (p) {
		err = process_init(p, name);
		if (err) p = NULL;
	}
	if (p) {
		err = 0;
		*resultProcess = p;
	}
	return err;
}

int thread_new(Process* p, uintptr context, Thread** resultThread) {
#ifdef LUPI_NO_PROCESS
	return KErrNotSupported;
#else

	// See if there are any dead threads we can reuse
	*resultThread = NULL;
	Thread* t = NULL;

	for (int i = 0; i < p->numThreads; i++) {
		if (p->threads[i].state == EDead) {
			t = &p->threads[i];
			break;
		}
	}
	if (!t && p->numThreads < MAX_THREADS) {
		t = &p->threads[p->numThreads];
		t->index = p->numThreads;
		if (p->heapLimit > userStackForThread(t)) {
			// Uh oh the heap has already encroached on where we were going to
			// put the thread stack, so we can't allow this.
			return KErrResourceLimit;
		}
		p->numThreads++;
	}

	if (t) {
		bool ok = thread_init(t);
		if (!ok) return KErrNoMemory;
		do_thread_new(t, context);
		thread_setState(t, EReady);
		*resultThread = t;
		return 0;
	} else {
		return KErrResourceLimit;
	}
#endif // LUPI_NO_PROCESS
}

static void freeThreadStacks(Thread* t) {
#ifdef HAVE_MMU
	uintptr stackBase = userStackForThread(t);
	Process* p = processForThread(t);
	mmu_unmapPagesInProcess(Al, p, stackBase, USER_STACK_SIZE >> KPageShift);
	mmu_unmapPagesInProcess(Al, p, svcStackBase(t->index), 1);
#endif
}

static void process_exit(Process* p, int reason) {
#ifndef LUPI_NO_IPC
	ipc_processExited(Al, p);
#endif

#ifdef HAVE_MMU
	// Now reclaim the heap
	mmu_unmapPagesInProcess(Al, p, KUserBss, 1 + ((p->heapLimit - KUserHeapBase) >> KPageShift));
#endif

	// Currently there's no way for the process to die unless all its threads
	// are already dead and cleaned up, but do this anyway in case we ever add
	// an explicit process exit API.
	for (int i = 0; i < p->numThreads; i++) {
		Thread* t = &p->threads[i];
		if (t->state != EDead) {
			thread_setState(t, EDead);
			freeThreadStacks(t);
		}
	}

#ifdef HAVE_MMU
	// Cleans up caches and page tables etc
	mmu_processExited(Al, p);
#endif

	if (TheSuperPage->currentProcess == p) {
		TheSuperPage->currentProcess = NULL;
	}
	p->pid = 0;
	//printk("Process %s exited with %d", p->name, reason);
}

static void threadExit_dfc(uintptr arg1, uintptr arg2, uintptr arg3) {
	Thread* t = (Thread*)arg1;
	switch_process(processForThread(t));
	freeThreadStacks(t);
	thread_setState(t, EDead);
	Process* p = processForThread(t);

	// See if we can shrink numThreads - important for reclaiming stack memory
	// in non-MMU mem model.
	if (t->index == p->numThreads-1) {
		while (p->numThreads && p->threads[p->numThreads-1].state == EDead) {
			p->numThreads--;
		}
	}

	// Check if the process still has any alive threads - if not call process_exit()
	bool dead = true;
	for (int i = 0; i < p->numThreads; i++) {
		if (p->threads[i].state != EDead) {
			dead = false;
			break;
		}
	}
	// printk("Thread %d exited, process dead=%d\n", t->index, (int)dead);
	if (dead) {
		process_exit(p, t->exitReason);
	}
}

/**
Call to exit the current thread (which must equal `t`). Does not return.
*/
NORETURN thread_exit(Thread* t, int reason) {
	ASSERT(t == TheSuperPage->currentThread, (uintptr)t);
	t->exitReason = reason;
	thread_setState(t, EDying);
	// We can't free the thread stacks directly because we're using the svc one.
	// So queue a DFC to do it for us.
	dfc_queue(threadExit_dfc, (uint32)t, 0, 0);
	reschedule();
}

void thread_setBlockedReason(Thread* t, ThreadBlockedReason reason) {
	ASSERT(t->state == EBlockedFromSvc, (uintptr)t);
	t->exitReason = reason;
}

NOINLINE NAKED uint32 do_user_read(uintptr ptr) {
	asm("LDRT r1, [r0]");
	asm("MOV r0, r1");
	asm("BX lr");
}

static NOINLINE NAKED void do_user_write(uintptr ptr, uint32 data) {
	asm("STRT r1, [r0]");
	asm("BX lr");
}

void thread_requestComplete(KAsyncRequest* request, int result) {
	Process* requestProcess = processForThread(request->thread);
	Process* oldP = switch_process(requestProcess);
	do_user_write(request->userPtr, result); // AsyncRequest->result = result
	do_user_write(request->userPtr + 4, KAsyncFlagPending | KAsyncFlagCompleted | KAsyncFlagIntResult);
	switch_process(oldP);
	thread_requestSignal(request);
}

void thread_requestSignal(KAsyncRequest* request) {
	Thread* t = request->thread;
	t->completedRequests++;
	//printk("Thread %s signalled nreq=%d state=%d\n", processForThread(t)->name, t->completedRequests, t->state);
	request->userPtr = 0;
	if (t->state == EWaitForRequest) {
		thread_writeSvcResult(t, t->completedRequests);
		t->completedRequests = 0;
		thread_setState(t, EReady);
		// Next reschedule will run it
	}
}
