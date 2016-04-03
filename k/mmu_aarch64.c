#include <k.h>
#include <mmu.h>
#include ARCH_HEADER

#define SetTTBR(n, val)			WRITE_SPECIAL(TTBR ## n ## _EL1, (uintptr)(val))

#define TCR_TG1_4KB		(2ULL << 30)
#define TCR_PS_4GB		(0ULL << 32) // Physical size

#define TCR_T0SZ_SHIFT	(0)
#define TCR_T1SZ_SHIFT	(16)


void mmu_init() {

	// uintptr* pde = (uintptr*)KPhysicalPdeBase;
	// TODO actually setup PDEs

	SetTTBR(0, KPhysicalPdeBase);
	SetTTBR(1, KPhysicalPdeBase);

	// Configure user/kern split - we only have 1GB RAM so go with 1GB max for user
	// T0SZ = 34, ie TTBR0 size is 2^(64-34) = 2^30 = 1GB
	uintptr tcr = TCR_TG1_4KB | TCR_PS_4GB | (34 << TCR_T0SZ_SHIFT);
	WRITE_SPECIAL(TCR_EL1, tcr);
}

Process* switch_process(Process* p) {
	if (!p) return NULL;
	Process* oldp = TheSuperPage->currentProcess;
	if (p == oldp) return NULL;

#ifdef HAVE_MMU // todo
	uint64 asid = indexForProcess(p);
	SetTTBR(0, p->pdePhysicalAddress | (asid << 48));
#endif

	TheSuperPage->currentProcess = p;
	return oldp;
}
