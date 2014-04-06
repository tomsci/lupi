#include <k.h>
#include <pageAllocator.h>
#include <memmap.h>
#include <mmu.h>
#include <arm.h>

#define KNumPreallocatedUserPages 0
#define userStackAddress(t) (KUserStacksBase + t->index * (USER_STACK_SIZE + KPageSize))

bool thread_init(Process* p, int index);

bool process_init(Process* p) {
	// Assume the Process page itself is already mapped, but nothing else necessarily is
	p->pid = TheSuperPage->nextPid++;
	uint32* pde = (uint32*)PDE_FOR_PROCESS(p);
	uint32* kernPtForTheUserPts = KERN_PT_FOR_PROCESS_PTS(p);
	if (!p->pdePhysicalAddress) {
		p->pdePhysicalAddress = mmu_mapPageInSection(Al, (uint32*)KProcessesPdeSection_pt, (uintptr)pde, KPageUserPde);
		uintptr userPtsStart = PT_FOR_PROCESS(p, 0);
		mmu_mapSection(Al, userPtsStart, (uintptr)kernPtForTheUserPts, (uint32*)KKernPtForProcPts_pt);
		//TODO check return code
	}
	zeroPage(pde);
	switch_process(p); // Have to do this before calling mmu_mapPagesInProcess

	mmu_mapPagesInProcess(Al, p, KUserBss, 1 + KNumPreallocatedUserPages);
	p->heapLimit = KUserHeapBase + (KNumPreallocatedUserPages << KPageShift);

	// Setup initial thread
	p->numThreads = 1;
	thread_init(p, 0);
	return true;
}

bool thread_init(Process* p, int index) {
	Thread* t = &p->threads[index];
	t->index = index;
	uintptr stackBase = userStackAddress(t);
	bool ok = mmu_mapPagesInProcess(Al, p, stackBase, USER_STACK_SIZE >> KPageShift);
	if (!ok) return false;

	t->savedRegisters[13] = stackBase + USER_STACK_SIZE;
	return true;
}

void NAKED process_start(const char* moduleName, const char* module, int moduleSize, uint32 sp) {
	ModeSwitch(KPsrModeUsr|KPsrFiqDisable);
	// We are in user mode now! So no calling printk(), or doing priviledged stuff
	asm("MOV sp, r3");
	asm("LDR pc, =newProcessEntryPoint");
	// And we're off. Shouldn't ever return
	hang(); //TODO
}

uintptr process_grow_heap(Process* p, int incr) {
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
		return p->heapLimit;
	} else {
		bool ok = mmu_mapPagesInProcess(Al, p, p->heapLimit, amount >> KPageShift);
		if (ok) {
			p->heapLimit += amount;
			return p->heapLimit;
		} else {
			return -1;
		}
	}
}
