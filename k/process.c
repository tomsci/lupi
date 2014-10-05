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
static bool thread_init(Process* p, int index);
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
	uint32* pde = (uint32*)PDE_FOR_PROCESS(p);
	uint32* kernPtForTheUserPts = (uint32*)KERN_PT_FOR_PROCESS_PTS(p);
	bool ok;
	if (!p->pdePhysicalAddress) {
		p->pdePhysicalAddress = mmu_mapPageInSection(Al, (uint32*)KProcessesPdeSection_pt, (uintptr)pde, KPageUserPde);
		uintptr userPtsStart = (uintptr)PT_FOR_PROCESS(p, 0);
		mmu_mapSection(Al, userPtsStart, (uintptr)kernPtForTheUserPts, (uint32*)KKernPtForProcPts_pt, KPageKernPtForProcPts);
		//TODO check return code
	}
	zeroPage(pde);

	mmu_mapPagesInProcess(Al, p, KUserBss, 1 + KNumPreallocatedUserPages);
	p->heapLimit = KUserHeapBase;

	// Setup initial thread
	p->numThreads = 1;
	ok = thread_init(p, 0);
	if (!ok) return KErrNoMemory;

	char* pname = p->name;
	char ch;
	do {
		ch = *processName++;
		*pname++ = ch;
	} while (ch);

	return 0;
#endif // LUPI_NO_PROCESS
}

#ifndef LUPI_NO_PROCESS

static bool thread_init(Process* p, int index) {
	Thread* t = &p->threads[index];
	t->prev = NULL;
	t->next = NULL;
	t->index = index;
	t->timeslice = THREAD_TIMESLICE;
	t->completedRequests = 0;
	uintptr stackBase = userStackForThread(t);
	bool ok = mmu_mapPagesInProcess(Al, p, stackBase, USER_STACK_SIZE >> KPageShift);
	if (!ok) return false;
	ok = mmu_mapSvcStack(Al, p, svcStackBase(index));
	if (!ok) {
		mmu_unmapPagesInProcess(Al, p, stackBase, USER_STACK_SIZE >> KPageShift);
		return false;
	}

	t->savedRegisters[13] = stackBase + USER_STACK_SIZE;
	thread_setState(t, EReady);
	return true;
}

#ifdef ARM

static NORETURN NAKED do_process_start(uint32 sp) {
	ModeSwitch(KPsrModeUsr|KPsrFiqDisable);
	// We are in user mode now! So no calling printk(), or doing privileged stuff
	asm("MOV sp, r0");
	asm("LDR r1, =newProcessEntryPoint");
	asm("BLX r1");
	// And we're off. We might return here if the module's main returns (with return code in r0)
	asm("B exec_threadExit");
	// Definitely don't return from here
}

#else

static NORETURN NAKED do_process_start(uint32 sp) {
	ASSERT(false);
}

#endif

NORETURN process_start(Process* p) {
	switch_process(p);
	Thread* t = firstThreadForProcess(p);
	TheSuperPage->currentThread = t;
	// Now we've switched process and mapped the BSS, we first need to zero all initial memory
	zeroPages((void*)KUserBss, 1 + KNumPreallocatedUserPages);
	zeroPages((void*)userStackForThread(t), USER_STACK_SIZE >> KPageShift);

	// And we can set up the user_* variables
	user_ProcessPid = p->pid;
	char* userpname = user_ProcessName;
	const char* pname = p->name;
	// Poor man's memcpy
	char ch;
	do {
		ch = *pname++;
		*userpname++ = ch;
	} while (ch);
	uint32 sp = t->savedRegisters[13];
	do_process_start(sp);
}
#endif

bool process_grow_heap(Process* p, int incr) {
	//printk("+process_grow_heap %d heapLimit=%p\n", incr, (void*)p->heapLimit);
	bool dec = (incr < 0);
	int amount;
	if (dec) amount = (-incr) & ~(KPageSize - 1); // Be sure not to free more than was asked to
	else amount = PAGE_ROUND(incr);

	if (dec) {
		if (p->heapLimit - amount < KUserHeapBase) {
			amount = p->heapLimit - KUserHeapBase;
		}
		p->heapLimit = p->heapLimit - amount;
		mmu_unmapPagesInProcess(Al, p, p->heapLimit, amount >> KPageShift);
		mmu_finishedUpdatingPageTables();
		return true;
	} else {
		const int npages = amount >> KPageShift;
		bool ok = mmu_mapPagesInProcess(Al, p, p->heapLimit, npages);
		if (ok) {
			mmu_finishedUpdatingPageTables();
			zeroPages((void*)p->heapLimit, npages);
			p->heapLimit += amount;
			//printk("-process_grow_heap heapLimit=%p\n", (void*)p->heapLimit);
		}
		return ok;
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
		return KErrProcessLimit;
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

static void freeThreadStacks(Thread* t) {
	uintptr stackBase = userStackForThread(t);
	Process* p = processForThread(t);
	mmu_unmapPagesInProcess(Al, p, stackBase, USER_STACK_SIZE >> KPageShift);
	mmu_unmapPagesInProcess(Al, p, svcStackBase(t->index), 1);
}

static void process_exit(Process* p, int reason) {
#ifndef LUPI_NO_IPC
	ipc_processExited(Al, p);
#endif

	// Now reclaim the heap
	mmu_unmapPagesInProcess(Al, p, KUserBss, 1 + ((p->heapLimit - KUserHeapBase) >> KPageShift));

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

	// Cleans up caches and page tables etc
	mmu_processExited(Al, p);

	if (TheSuperPage->currentProcess == p) {
		TheSuperPage->currentProcess = NULL;
	}
	p->pid = 0;
	//printk("Process %s exited with %d", p->name, reason);
}

static void dfc_threadExit(uintptr arg1, uintptr arg2, uintptr arg3) {
	Thread* t = (Thread*)arg1;
	switch_process(processForThread(t));
	freeThreadStacks(t);
	thread_setState(t, EDead);
	Process* p = processForThread(t);
	// Check if the process still has any alive threads - if not call process_exit()
	bool dead = true;
	for (int i = 0; i < p->numThreads; i++) {
		if (p->threads[i].state != EDead) {
			dead = false;
			break;
		}
	}
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
	dfc_queue(dfc_threadExit, (uint32)t, 0, 0);
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
		t->savedRegisters[0] = t->completedRequests;
		t->completedRequests = 0;
		thread_setState(t, EReady);
		// Next reschedule will run it
	}
}
