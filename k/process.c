#include <k.h>
#include <pageAllocator.h>
#include <memmap.h>
#include <mmu.h>
#include <arm.h>

#define KNumPreallocatedUserPages 0

// These will refer to the *user* addresses of these variables, in the BSS.
// Therefore, can only be referenced when process_switch()ed to the
// appropriate Process.
extern uint32 user_ProcessPid;
extern char user_ProcessName[];

bool thread_init(Process* p, int index);

static bool process_init(Process* p, const char* processName) {
	// Assume the Process page itself is already mapped, but nothing else necessarily is
	p->pid = TheSuperPage->nextPid++;
	uint32* pde = (uint32*)PDE_FOR_PROCESS(p);
	uint32* kernPtForTheUserPts = (uint32*)KERN_PT_FOR_PROCESS_PTS(p);
	bool ok;
	if (!p->pdePhysicalAddress) {
		p->pdePhysicalAddress = mmu_mapPageInSection(Al, (uint32*)KProcessesPdeSection_pt, (uintptr)pde, KPageUserPde);
		uintptr userPtsStart = (uintptr)PT_FOR_PROCESS(p, 0);
		mmu_mapSection(Al, userPtsStart, (uintptr)kernPtForTheUserPts, (uint32*)KKernPtForProcPts_pt);
		//TODO check return code
	}
	zeroPage(pde);

	mmu_mapPagesInProcess(Al, p, KUserBss, 1 + KNumPreallocatedUserPages);
	p->heapLimit = KUserHeapBase;

	// Setup initial thread
	p->numThreads = 1;
	ok = thread_init(p, 0);
	if (!ok) return false;

	char* pname = p->name;
	char ch;
	do {
		ch = *processName++;
		*pname++ = ch;
	} while (ch);

	return ok;
}

bool thread_init(Process* p, int index) {
	Thread* t = &p->threads[index];
	t->prev = NULL;
	t->next = NULL;
	t->index = index;
	t->timeslice = THREAD_TIMESLICE;
	uintptr stackBase = userStackForThread(t);
	bool ok = mmu_mapPagesInProcess(Al, p, stackBase, USER_STACK_SIZE >> KPageShift);
	if (!ok) return false;

	t->savedRegisters[13] = stackBase + USER_STACK_SIZE;
	thread_setState(t, EReady);
	return true;
}

#ifndef KLUA
static void NAKED do_process_start(uint32 sp) {
	ModeSwitch(KPsrModeUsr|KPsrFiqDisable);
	// We are in user mode now! So no calling printk(), or doing priviledged stuff
	asm("MOV sp, r0");
	asm("LDR pc, =newProcessEntryPoint");
	// And we're off. Shouldn't ever return
	hang(); //TODO
}

void process_start(Process* p) {
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

Process* process_new(const char* name) {
	// First see if there is a spare Process* we can use
	SuperPage* s = TheSuperPage;
	Process* p = 0;
	for (int i = 0; i < s->numValidProcessPages; i++) {
		Process* candidate = GetProcess(i);
		if (candidate->pid == 0) {
			p = candidate;
			break;
		}
	}
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

	if (p) {
		bool ok = process_init(p, name);
		if (!ok) p = NULL;
	}
	return p;
}
