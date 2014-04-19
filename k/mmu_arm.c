#include <k.h>
#include <mmu.h>
#include <arm.h>
#include <pageAllocator.h>

/*
 * Page Directory = "First-level translation table"
 * PDE = "first-level translation descriptor"
 *
 * Page Table     = "Second-level translation table"
 * PTR = "Second-level translation descriptor"
 */

//#define TTBCR_N 2 // Max user address space = 1GB, 1024 PDEs
#define TTBCR_N 3 // Max address space = 256MB which is actually all I allowed in the PT setup, doh. TODO fix that...
#define KMaxUserAddress 0x10000000


//static uint32* getPageDirectoryForProcess(uint processIdx) {
	//TODO
//}



#define SetTTBR(n, val)			asm("MCR p15, 0, %0, c2, c0, " #n : : "r" (val)) // p192
#define SetTTBCR(val)			asm("MCR p15, 0, %0, c2, c0, 2"   : : "r" (val)) // p193

#define KNumPdes 4096 // number of Page Directory Entries is always 4096x1MB entries, on a 32-bit machine (unless we're using truncated PDs, see below)


// See p356
//           19 18 17 16 | 15 14 13 12 | 11 10 9 8 | 7 6 5 4 | 3 2 1 0
//           ------------|-------------|-----------|---------|--------
// Section   NS  0 nG  S |APX ---TEX-- | --AP- P --Domain- XN| C B 1 0
// PageTable .... Page table address ....      P --Domain- 0 |NS 0 0 1

#ifdef NON_SECURE
#define PDE_SECTION_NS_BIT (1<<19)
#define PDE_PAGETABLE_NS_BIT (1<<3)
#else
#define PDE_SECTION_NS_BIT 0
#define PDE_PAGETABLE_NS_BIT 0
#endif

#define KPdeSectionKernelData	(0x00000412 | PDE_SECTION_NS_BIT) // nG=0, S=0, APX=b001, XN=1, P=C=B=0
#define KPdeSectionJfdi			0x00000402 // NS=0, nG=0, S=0, APX=b001, XN=0, P=C=B=0
#define KPdeSectionPeripheral	(0x00000412 | PDE_SECTION_NS_BIT) // nG=0, S=0, APX=b001, XN=1, P=C=B=0
#define KPdePageTable			(0x00000001 | PDE_PAGETABLE_NS_BIT) // NS=0, P=0

// See p357
// 11 10  9  8 | 7 6 5 4 | 3 2 1 0
// ------------|---------|---------
// nG  S APX --TEX-- -AP | C B 1 XN
#define KPteKernelCode			0x0000022A // C=1, B=0, XN=0, APX=b110, S=0, TEX=0, nG=0
#define KPteKernelData			0x00000013 // C=B=0, XN=1, APX=b001, S=0, TEX=0, nG=0
//#define KPteUserData			0x00000833 // C=B=0, XN=1, APX=b011, S=0, TEX=0, nG=1
#define KPteUserData			0x0000083F // C=B=1, XN=1, APX=b011, S=0, TEX=0, nG=1
//#define KPteRoUserData		0x00000823 // C=B=0, XN=1, APX=b010, S=0, TEX=0, nG=1


// Control register bits, see p176
#define CR_XP (1<<23) // Extended page tables
#define CR_I  (1<<12) // Enable Instruction cache
#define CR_C  (1<<2)  // Enable Data cache
#define CR_A  (1<<1)  // Enable strict alignment checks
#define CR_M  (1)     // Enable MMU

uint32 makeCrForMmuEnable() {
	uint32 cr;
	asm("MRC p15, 0, %0, c1, c0, 0" : "=r" (cr));
	//printk("Control register init = 0x%X\n", cr);
	cr = cr & ~CR_I; // unset I
	cr = cr | CR_XP;
	cr = cr | CR_M;
	//printk("Control register going to be 0x%x\n", cr);
	return cr;
}

// Does not return; jumps to virtual address returnAddr
void NAKED mmu_setControlRegister(uint32 controlRegister, uintptr returnAddr) {
	asm("MOV r2, #0"); // r2 = 0

	DSB(r2);
	asm("MCR p15, 0, r0, c1, c0, 0"); // Boom!

	asm("BX r1");
	ISB(r2); // Prevent prefetch from when MMU was disabled from going beyond this point. Probably.
	//TODO returnAddr will need to reenable instruction cache (and maybe data cache)
}

// Macro for when we're too early to call zeroPage()
#define init_zeroPages(ptr, n) \
	for (uint32 *p = ptr, *end = (uint32*)(((uint8*)ptr) + (n << KPageShift)); p != end; p++) {\
		*p = 0; \
	}

void NAKED mmu_setCache(bool icache, bool dcache) {
	asm("MRC p15, 0, r2, c1, c0, 0");
	asm("CMP r0, #0");
	asm("BICEQ r2, %0" : : "i" (CR_I));
	asm("ORRNE r2, %0" : : "i" (CR_I));
	asm("CMP r1, #0");
	asm("BICEQ r2, %0" : : "i" (CR_C));
	asm("ORRNE r2, %0" : : "i" (CR_C));
	asm("MCR p15, 0, r2, c1, c0, 0");
	asm("BX lr");
}

/*
Enter and exit with MMU disabled
Sets up the minimal set of page tables at KPhysicalPdeBase

Note: Anything within the brackets of pde[xyz] or pte[xyz] should NOT contain the word 'physical'.
If it does, I've confused the fact that the PTE offsets refer to the VIRTUAL addresses.
*/
void mmu_init() {
	uint32* pde = (uint32*)KPhysicalPdeBase;
	// Default no access
	init_zeroPages(pde, 4);

	// Map peripheral memory as a couple of sections
	int peripheralMemIdx = KPeripheralBase >> KAddrToPdeIndexShift;
	for (int i = 0; i < KPeripheralSize >> KSectionShift; i++) {
		pde[peripheralMemIdx + i] = (KPeripheralPhys + (i << KSectionShift)) | KPdeSectionPeripheral;
	}

	// Map section zero
	pde[KSectionZero >> KAddrToPdeIndexShift] = KPhysicalSect0Pt | KPdePageTable;
	uint32* sectPte = (uint32*)KPhysicalSect0Pt;
	init_zeroPages(sectPte, 1);
	// Don't strictly need to set up these next two yet, but might as well.
	sectPte[PTE_IDX(KAbortStackBase)] = KPhysicalAbortStackBase | KPteKernelData;
	sectPte[PTE_IDX(KIrqStackBase)] = KPhysicalIrqStackBase | KPteKernelData;
	sectPte[PTE_IDX(KKernelStackBase)] = KPhysicalStackBase | KPteKernelData;
	sectPte[PTE_IDX(KKernelStackBase) + 1] = (KPhysicalStackBase + KPageSize) | KPteKernelData;

	// Code!
	for (int i = 0; i < KKernelCodesize >> KPageShift; i++) {
		uint32 phys = KPhysicalCodeBase + (i << KPageShift);
		sectPte[PTE_IDX(KKernelCodeBase) + i] = phys | KPteKernelCode;
	}

	// Map the kern PDEs themselves
	for (int i = 0; i < 4; i++) {
		uint32 phys = KPhysicalPdeBase + (i << KPageShift);
		sectPte[PTE_IDX(KKernelPdeBase) + i] = phys | KPteKernelData;
	}

	// And make sure we can the section zero pte ourselves once the MMU is on
	sectPte[PTE_IDX(KSectionZeroPt)] = KPhysicalSect0Pt | KPteKernelData;

	SetTTBR(0, KPhysicalPdeBase);
	SetTTBR(1, KPhysicalPdeBase);
	SetTTBCR(TTBCR_N);
	// Set DACR to get the hell out of the way and don't screw with our access permissions
	uint32 clientMeUp = 0x1;
	asm("MCR p15, 0, %0, c3, c0, 0" : : "r" (clientMeUp));
}

#if 0
void mmu_identity_init() {
	uint32* pde = (uint32*)KPhysicalPdeBase;

	uint32 phys = 0;
	for (uint32 i = 0; i < KNumPdes; i++) {
		uint32 entry = phys | KPdeSectionJfdi;
		//		printk("PDE for %X = %X\n", i * (1024*1024), entry);
		pde[i] = entry;
		phys += 1 MB;
	}

	pde[0x4800000 >> KAddrToPdeIndexShift] = 0; // No access here

	SetTTBR(0, KPhysicalPdeBase);
	SetTTBR(1, KPhysicalPdeBase);
	SetTTBCR(0);
	// Set DACR to get the hell out of the way
	uint32 everythingIsPermitted = 0xFFFFFFFF;
	asm("MCR p15, 0, %0, c3, c0, 0" : : "r" (everythingIsPermitted));
}
#endif

static void mmu_doMapPagesInSection(uintptr virtualAddress, uint32* sectPte, uintptr physicalAddress, int numPages) {
	for (int i = 0; i < numPages; i++) {
		uint32 phys = physicalAddress + (i << KPageShift);
		sectPte[PTE_IDX(virtualAddress) + i] = phys | KPteKernelData;
	}
}

void mmu_mapSect0Data(uintptr virtualAddress, uintptr physicalAddress, int npages) {
	uint32* sectPt = (uint32*)KSectionZeroPt;
	mmu_doMapPagesInSection(virtualAddress, sectPt, physicalAddress, npages);
}

uintptr mmu_mapSectionContiguous(PageAllocator* pa, uintptr virtualAddress, uint8 type) {
	// To map a whole section we need 256 contiguous pages *aligned on a section boundary*
	uintptr phys = pageAllocator_allocAligned(pa, type, KPagesInSection, 1<<KSectionShift);
	if (phys) {
		uint32* pde = (uint32*)KKernelPdeBase;
		pde[virtualAddress >> KAddrToPdeIndexShift] = phys | KPdeSectionKernelData;
	}
	return phys;
}

bool mmu_mapSection(PageAllocator* pa, uintptr sectionAddress, uintptr ptAddress, uint32* ptsPt) {
	// Map a page for the section pt into the ptsPt
	uint32 pageTablePhysical = mmu_mapPageInSection(pa, ptsPt, ptAddress, KPageSect0);
	if (!pageTablePhysical) return false;

	// Now update so we can write to the new PTE page
	mmu_finishedUpdatingPageTables();
	zeroPage((uint32*)ptAddress); // The section starts out with all pages unmapped

	// And update the kern PDE with this PT
	uint32* pde = (uint32*)KKernelPdeBase;
	pde[sectionAddress >> KAddrToPdeIndexShift] = pageTablePhysical | KPdePageTable;

	return true;
}

uintptr mmu_mapPageInSection(PageAllocator* pa, uint32* pt, uintptr virtualAddress, uint8 type) {
	uint32 newPagePhysical = pageAllocator_alloc(pa, type, 1);
	if (newPagePhysical) {
		pt[PTE_IDX(virtualAddress)] = newPagePhysical | KPteKernelData;
	}
	return newPagePhysical;
}

void NAKED mmu_finishedUpdatingPageTables() {
	asm("MOV r0, #0");
	DSB(r0);
	asm("BX lr");
}

/*
void mmu_freePagesForProcess(Process* p) {
	// First, indicate to our allocator that all the pages mapped to user mem are now available

	// Then, free up the pages used by the page tables
}
*/

bool mmu_createUserSection(PageAllocator* pa, Process* p, int sectionIdx) {
	// We need a new page for the user PT for this section.
	uint32* newPt = PT_FOR_PROCESS(p, sectionIdx);
	//printk("+mmu_createUserSection %d newPt=%p\n", sectionIdx, newPt);
	uint32* kernPt = KERN_PT_FOR_PROCESS_PTS(p);
	uint32 ptPhysical = mmu_mapPageInSection(pa, kernPt, (uintptr)newPt, KPageUserPt);
	if (!ptPhysical) return false;
	mmu_finishedUpdatingPageTables();
	zeroPage(newPt); // Default no access to anything
	// Now add this page to the user PDE
	uint32* pde = (uint32*)PDE_FOR_PROCESS(p);
	pde[sectionIdx] = ptPhysical | KPdePageTable;
	//printk("-mmu_createUserSection %d\n", sectionIdx);
	return true;
}

bool mmu_mapPagesInProcess(PageAllocator* pa, Process* p, uintptr virtualAddress, int numPages) {
	//printk("mmu_mapPagesInProcess va=%p n=%d\n", (void*)virtualAddress, numPages);
	ASSERT(numPages > 0);
	// User processes can only map up to 1GB due to how we've configured TTBCR
	ASSERT(virtualAddress <= KMaxUserAddress - numPages * KPageSize);
	int sectionIdx = virtualAddress >> KSectionShift;
	const uintptr endAddr = virtualAddress + (numPages << KPageShift);
	int numSections = ((endAddr-1) >> KSectionShift) - sectionIdx + 1;
	uint32* pde = (uint32*)PDE_FOR_PROCESS(p);
	// Make sure all required sections are created
	for (int i = sectionIdx; i < sectionIdx + numSections; i++) {
		if (!pde[i]) {
			// PT hasn't been created yet
			bool ok = mmu_createUserSection(pa, p, i);
			if (!ok) return false;
		}
	}
	uint32* const pt = PT_FOR_PROCESS(p, sectionIdx);
	// The PTs are contiguous so we don't need to worry about whether we cross sections - just
	// keep incrementing pte until we're done
	uint32* pte = pt + PTE_IDX(virtualAddress);
	uint32* endPte = pte + numPages;
	while (pte != endPte) {
		uint32 newPagePhysical = pageAllocator_alloc(pa, KPageUser, 1);
		if (!newPagePhysical) {
			// Erk, better cleanup
			mmu_unmapPagesInProcess(pa, p, virtualAddress, pte - pt);
			return false;
		}
		*pte = newPagePhysical | KPteUserData;
		pte++;
	}
	return true;
}

#if 0
bool mmu_mapKernelPageInProcess(Process* p, uintptr physicalAddress, uintptr virtualAddress, bool readWrite) {
	int sectionIdx = virtualAddress >> KSectionShift;
	uint32* const pt = PT_FOR_PROCESS(p, sectionIdx);
	uint32* pte = pt + PTE_IDX(virtualAddress);

	uint32* pde = (uint32*)PDE_FOR_PROCESS(p);
	ASSERT(pde[sectionIdx]); // Section must already be mapped
	/*
	// Make sure section exists
	if (!pde[sectionIdx]) {
		// PT hasn't been created yet
		bool ok = mmu_createUserSection(pa, p, i);
		if (!ok) return false;
	}
	*/

	*pte = physicalAddress | (readWrite ? KPteUserData : KPteRoUserData);
	return true;
}
#endif

void mmu_unmapPagesInProcess(PageAllocator* pa, Process* p, uintptr virtualAddress, int numPages) {
	ASSERT(numPages >= 0);
	ASSERT(virtualAddress <= KMaxUserAddress - numPages * KPageSize);
	int sectionIdx = virtualAddress >> KSectionShift;
	uint32* pde = (uint32*)PDE_FOR_PROCESS(p);
	ASSERT(pde[sectionIdx]); // PT must be created
	uint32* pt = PT_FOR_PROCESS(p, sectionIdx);
	uint32* pte = pt + PTE_IDX(virtualAddress);
	uint32* endPte = pte + numPages;
	while (pte != endPte) {
		uintptr physicalAddress = *pte & ~(KPageSize - 1);
		pageAllocator_free(pa, physicalAddress);
		pte++;
	}
	//TODO invalidate caches, TLB
}

void switch_process(Process* p) {
	uint32 asid = indexForProcess(p);

	SetTTBR(0, p->pdePhysicalAddress);

	// Set context ID register
	uint32 zero = 0;
	DSB_inline(zero);
	asm("MCR p15, 0, %0, c13, c0, 1" : : "r" (asid));
	ISB_inline(zero);

	TheSuperPage->currentProcess = p;
	// I think we're done - setting context ID does all the flushing required
}
