#include <k.h>
#include <mmu.h>
#include <arm.h>
#include <pageAllocator.h>

#define MB *1024*1024
#define KSectionShift 20
#define KAddrToPdeIndexShift KSectionShift
#define KAddrToPdeAddrShift (KAddrToPdeIndexShift - 2)
#define KPageTableSize		4096
#define KSectionMask		0x000FFFFFu
#define KPagesInSection		(1 << (KSectionShift-KPageShift)) // ie 256

#define PTE_IDX(virtAddr)	(((virtAddr) & KSectionMask) >> KPageShift)

#define PDE_FOR_PROCESS(p) (((uintptr)(p)) | 0x00200000) // Whee for crazy hacks
#define PT_FOR_PROCESS(p, sectionNum) ((uint32*)(KProcessPtBase | (indexForProcess(p) << KSectionShift) | (sectionNum << KPageShift)))

// masks off all bits except those that distinguish one Process from another
#define MASKED_PROC_PTR(p) (((uintptr)(p)) & 0x000FF000)

#define KERN_PT_FOR_PROCESS_PTS(p) ((uint32*)(KKernPtForProcPts | MASKED_PROC_PTR(p)))

/**
Creates a new section map for the given index (where `sectionIdx` is the virtual address right-
shifted by `KPageShift`) in the given Process.
*/
static bool mmu_createUserSection(PageAllocator* pa, Process* p, int sectionIdx);


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
#ifdef ENABLE_DCACHE
#define KPteKernelCode			0x0000022A // C=1, B=0, XN=0, APX=b110, S=0, TEX=0, nG=0
#define KPteKernelData			0x00000013 // C=B=0, XN=1, APX=b001, S=0, TEX=0, nG=0
#define KPteUserData			0x0000083F // C=B=1, XN=1, APX=b011, S=0, TEX=0, nG=1
#define KPteProcessKernelData	0x0000081F // C=B=1, XN=1, APX=b001, S=0, TEX=0, nG=1
#else
#define KPteKernelCode			0x00000222 // C=B=0, XN=0, APX=b110, S=0, TEX=0, nG=0
#define KPteKernelData			0x00000013 // C=B=0, XN=1, APX=b001, S=0, TEX=0, nG=0
#define KPteUserData			0x00000833 // C=B=0, XN=1, APX=b011, S=0, TEX=0, nG=1
#define KPteProcessKernelData	0x00000813 // C=B=0, XN=1, APX=b001, S=0, TEX=0, nG=1
#endif

// Control register bits, see p176
#define CR_XP (1<<23) // Extended page tables
#define CR_I  (1<<12) // Enable Instruction cache
#define CR_C  (1<<2)  // Enable Data cache
#define CR_A  (1<<1)  // Enable strict alignment checks
#define CR_M  (1)     // Enable MMU

static void invalidateTLBEntry(uintptr virtualAddress, Process* p);

#ifdef ICACHE_IS_STILL_BROKEN
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
	FlushTLB(r2);
	asm("MCR p15, 0, r0, c1, c0, 0"); // Boom!

	asm("BX r1");
	ISB(r2); // Prevent prefetch from when MMU was disabled from going beyond this point. Probably.
}
#endif

// Macro for when we're too early to call zeroPage()
#define init_zeroPages(ptr, n) \
	for (uint32 *p = ptr, *end = (uint32*)(((uint8*)ptr) + (n << KPageShift)); p != end; p++) {\
		*p = 0; \
	}

#ifdef ICACHE_IS_STILL_BROKEN
void NAKED mmu_setCache(bool icache, bool dcache) {
	/* Doesn't work...
	if (icache) {
		asm("MOV r3, #0");
		InvalidateTLB(r3);
		FlushBTAC(r3);
	}
	*/

	asm("MRC p15, 0, r2, c1, c0, 0");
	asm("CMP r0, #0");
	asm("BICEQ r2, %0" : : "i" (CR_I));
	asm("ORRNE r2, %0" : : "i" (CR_I));
	asm("CMP r1, #0");
	asm("BICEQ r2, %0" : : "i" (CR_C));
	asm("ORRNE r2, %0" : : "i" (CR_C));
	asm("MCR p15, 0, r2, c1, c0, 0");

	/* Also doesn't work...
	// Finally, invalidate all caches. How much of this is needed, I have no idea
	if (icache) {
		InvalidateTLB(r3);
		FlushBTAC(r3);
		ISB(r3);
		// Why doesn't this work??
		InvalidateIcache(r3);
	}
	*/
	asm("BX lr");
}
#endif

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

#ifndef ICACHE_IS_STILL_BROKEN
	// Also set up a temporary identity mapping for the first page of code, this
	// is needed when enabling the MMU. We steal the IRQ stack briefly for this
	// purpose.
	pde[KPhysicalCodeBase >> KAddrToPdeIndexShift] = KTemporaryIdMappingPt | KPdePageTable;
	uint32* idPte = (uint32*)KTemporaryIdMappingPt;
	init_zeroPages(idPte, 1);
	idPte[PTE_IDX(KPhysicalCodeBase)] = KPhysicalCodeBase | KPteKernelCode;
#endif

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

#ifndef ICACHE_IS_STILL_BROKEN
// r14 contains virtual address to return to.
// We are free to nuke anything except r5 (and r13)
// KTemporaryIdMappingPt must be set up
// Enter with MMU and interrupts disabled
void NAKED mmu_enable() {
	asm("MOV r0, #0");

	asm("MRC p15, 0, r1, c1, c0, 0"); // r1 = CR
	asm("BIC r1, %0" : : "i" (CR_I)); // Make sure icache disabled
	asm("BIC r1, %0" : : "i" (CR_C)); // Make sure dcache disabled
	asm("ORR r1, %0" : : "i" (CR_XP));

	asm("ORR r2, r1, %0" : : "i" (CR_M)); // r2 = CR + enableMMU no caching
	asm("ORR r3, r2, %0" : : "i" (CR_C));
	asm("ORR r3, r3, %0" : : "i" (CR_I)); // r3 = CR + enableMMU with caching

	// Heh the way we've arranged the memmap, the offset from phys->virt can
	// be loaded in a single MOV instruction
	asm("MOV r4, %0" : : "i" (KKernelCodeBase - KPhysicalCodeBase));

	asm("MCR p15, 0, r1, c1, c0, 0"); // Disable icache (seems superfluous...)

	FlushIcache(r0);
	FlushDcache(r0);
	DSB(r0);

	DSB(r0);
	asm("MCR p15, 0, r2, c1, c0, 0"); // Enable MMU with no caching
	ISB(r0);
	asm("ADD pc, pc, r4"); // PC is two ahead so this'll jump an instruction
	asm("NOP"); // ... which might as well be a nop

	//TODO remove temp mapping

	FlushTLB(r0);
	ISB(r0);

	asm("MCR p15, 0, r3, c1, c0, 0"); // Now enable caching
	DSB(r0);
	ISB(r0);
	asm("BX lr");
}
#endif

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

void mmu_unmapSection(PageAllocator* pa, uintptr virtualAddress) {
	uint32* pde = (uint32*)KKernelPdeBase;
	uint32 sectionAddr = pde[virtualAddress >> KAddrToPdeIndexShift];
	// This code only works with mmu_mapSectionContiguous
	ASSERT((sectionAddr & 0xFFFFF) == KPdeSectionKernelData, virtualAddress, sectionAddr);
	uintptr physAddr = sectionAddr & ~0xFFFFF;
	pde[virtualAddress >> KAddrToPdeIndexShift] = 0;
	mmu_finishedUpdatingPageTables();
	pageAllocator_freePages(pa, physAddr, KPagesInSection);
}

bool mmu_mapSection(PageAllocator* pa, uintptr sectionAddress, uintptr ptAddress, uint32* ptsPt, uint8 ptPageType) {
	// Map a page for the section pt into the ptsPt
	uint32 pageTablePhysical = mmu_mapPageInSection(pa, ptsPt, ptAddress, ptPageType);
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

void mmu_unmapPageInSection(PageAllocator* pa, uint32* pt, uintptr virtualAddress) {
	uint32 phys = pt[PTE_IDX(virtualAddress)] & ~(KPageSize-1);
	pageAllocator_free(pa, phys);
	pt[PTE_IDX(virtualAddress)] = 0;
}

void NAKED mmu_finishedUpdatingPageTables() {
	asm("MOV r0, #0");
	DSB(r0);
	asm("BX lr");
}

static bool mmu_createUserSection(PageAllocator* pa, Process* p, int sectionIdx) {
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

// Note doesn't invalidate the user process TLB
void mmu_freeUserSection(PageAllocator* pa, Process* p, int sectionIdx) {
	uint32* pde = (uint32*)PDE_FOR_PROCESS(p);
	if (pde[sectionIdx]) {
		uintptr pageTableVirtualAddress = (uintptr)PT_FOR_PROCESS(p, sectionIdx);
		uint32* kernPt = KERN_PT_FOR_PROCESS_PTS(p);
		mmu_unmapPageInSection(pa, kernPt, pageTableVirtualAddress);
		pde[sectionIdx] = 0;
		invalidateTLBEntry(pageTableVirtualAddress, NULL);
	}
}

bool mmu_mapPagesInProcess(PageAllocator* pa, Process* p, uintptr virtualAddress, int numPages) {
	// printk("mmu_mapPagesInProcess p=%p va=%p n=%d\n", p, (void*)virtualAddress, numPages);
	uint8 pageType = KPageUser;
	if (numPages < 0) {
		// The only other page types we support user-side
		ASSERT(-numPages == KPageSharedPage || -numPages == KPageThreadSvcStack, -numPages);
		pageType = -numPages;
		numPages = 1;
	}
	ASSERT(numPages > 0, numPages);
	// User processes can only map up to 1GB due to how we've configured TTBCR
	ASSERT(virtualAddress <= KMaxUserAddress - numPages * KPageSize, virtualAddress, numPages);
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
	uint32 pageMappingType;
	if (pageType == KPageThreadSvcStack) {
		pageMappingType = KPteProcessKernelData;
	} else {
		pageMappingType = KPteUserData;
	}
	while (pte != endPte) {
		uint32 newPagePhysical = pageAllocator_alloc(pa, pageType, 1);
		if (!newPagePhysical) {
			// Erk, better cleanup
			mmu_unmapPagesInProcess(pa, p, virtualAddress, pte - pt);
			return false;
		}
		*pte = newPagePhysical | pageMappingType;
		pte++;
	}
	return true;
}

bool mmu_sharePage(PageAllocator* pa, Process* src, Process* dest, uintptr sharedPage) {
	ASSERT(sharedPage >= KSharedPagesBase, (uint32)src, sharedPage);
	ASSERT(sharedPage < KSharedPagesBase + KSharedPagesSize, (uint32)src, sharedPage);
	ASSERT((sharedPage & 0xFFF) == 0, (uint32)src, sharedPage);
	const int sectionIdx = sharedPage >> KSectionShift;
	uint32* srcPte = PT_FOR_PROCESS(src, sectionIdx) + PTE_IDX(sharedPage);

	uint32* destPde = (uint32*)PDE_FOR_PROCESS(dest);
	// Check the dest shared page section has been created
	if (!destPde[sectionIdx]) {
		bool ok = mmu_createUserSection(pa, dest, sectionIdx);
		if (!ok) return false;
	}
	uint32* destPte = PT_FOR_PROCESS(dest, sectionIdx) + PTE_IDX(sharedPage);
	ASSERT(*destPte == 0, (uint32)dest, sharedPage);
	*destPte = *srcPte;
	return true;
}

#if 0
bool mmu_mapKernelPageInProcess(Process* p, uintptr physicalAddress, uintptr virtualAddress, bool readWrite) {
	int sectionIdx = virtualAddress >> KSectionShift;
	uint32* const pt = PT_FOR_PROCESS(p, sectionIdx);
	uint32* pte = pt + PTE_IDX(virtualAddress);

	uint32* pde = (uint32*)PDE_FOR_PROCESS(p);
	ASSERT(pde[sectionIdx], virtualAddress); // Section must already be mapped
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
	//printk("mmu_unmapPagesInProcess %X\n", (uint)virtualAddress);
	ASSERT(numPages >= 0);
	ASSERT(virtualAddress <= KMaxUserAddress - numPages * KPageSize, virtualAddress, numPages);
	int sectionIdx = virtualAddress >> KSectionShift;
	uint32* pde = (uint32*)PDE_FOR_PROCESS(p);
	ASSERT(pde[sectionIdx], virtualAddress); // PT must be created
	uint32* pt = PT_FOR_PROCESS(p, sectionIdx);
	uint32* pte = pt + PTE_IDX(virtualAddress);
	uint32* endPte = pte + numPages;
	while (pte != endPte) {
		uintptr physicalAddress = *pte & ~(KPageSize - 1);
		pageAllocator_free(pa, physicalAddress);
		invalidateTLBEntry(virtualAddress, p);
		*pte = 0;
		pte++;
		virtualAddress += KPageSize;
	}
	//TODO invalidate caches, TLB
}

// Process == NULL means it's a kernel address
static void invalidateTLBEntry(uintptr virtualAddress, Process* p) {
	if (p) virtualAddress |= indexForProcess(p);
	asm("MCR p15, 0, %0, c8, c7, 1" : : "r" (virtualAddress)); // Invalidate TLB by MVA p218
}

Process* switch_process(Process* p) {
	if (!p) return NULL;
	Process* oldp = TheSuperPage->currentProcess;
	if (p == oldp) return NULL;

	uint32 asid = indexForProcess(p);

	SetTTBR(0, p->pdePhysicalAddress);

	// Set context ID register
	uint32 zero = 0;
	DSB_inline(zero);
	asm("MCR p15, 0, %0, c13, c0, 1" : : "r" (asid));
	ISB_inline(zero);

	TheSuperPage->currentProcess = p;
	// I think we're done - setting context ID does all the flushing required
	return oldp;
}

int mmu_processInit(Process* p) {
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
	return 0;
}

void mmu_processExited(PageAllocator* pa, Process* p) {
	// Clean up the page tables themselves
	for (int sectionIdx = 0; sectionIdx < (KMaxUserAddress >> KSectionShift); sectionIdx++) {
		mmu_freeUserSection(pa, p, sectionIdx);
	}
	// We'll leave the PDE allocated

	int asid = indexForProcess(p);
	asm("MCR p15, 0, %0, c8, c7, 2" : : "r" (asid)); // Invalidate TLB by ASID p218
}

Process* mmu_newProcess(PageAllocator* pa) {
	Process* result = NULL;
	SuperPage* s = TheSuperPage;
	if (s->numValidProcessPages < MAX_PROCESSES) {
		// Map ourselves a new one
		result = GetProcess(s->numValidProcessPages);
		uintptr phys = mmu_mapPageInSection(Al, (uint32*)KProcessesSection_pt, (uintptr)result, KPageProcess);
		if (phys) {
			mmu_finishedUpdatingPageTables();
			result->pdePhysicalAddress = 0;
			s->numValidProcessPages++;
		}
	}
	return result;
}
