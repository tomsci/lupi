#include <k.h>
#include <mmu.h>
#include <pageAllocator.h>

/*
 * Page Directory = "First-level translation table"
 * PDE = "first-level translation descriptor"
 *
 * Page Table     = "Second-level translation table"
 * PTR = "Second-level translation descriptor"
 */

#define TTBCR_N 2 // Max user address space = 1GB, 1024 PDEs
#define KNumUserPdes 1024


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
//#define KPdeSectionPeripheral	0x00002412 // NS=0, nG=0, S=0, APX=b001, TEX=b010, XN=1, P=C=B=0
#define KPdeSectionPeripheral	KPdeSectionKernelData
#define KPdePageTable			(0x00000001 | PDE_PAGETABLE_NS_BIT) // NS=0, P=0

// See p357
// 11 10  9  8 | 7 6 5 4 | 3 2 1 0
// ------------|---------|---------
// nG  S APX --TEX-- -AP | C B 1 XN
#define KPteKernelCode			0x00000212 // C=B=0, XN=0, APX=b101, S=0, TEX=0, nG=0
#define KPteKernelData			0x00000013 // C=B=0, XN=1, APX=b001, S=0, TEX=0, nG=0
//#define KPtePeripheralMem		0x00000093 // C=B=0, XN=1, APX=b001, S=0, TEX=b010, nG=0


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

#define zeroPages(ptr, n) \
	for (uint32 *p = ptr, *end = (uint32*)(((uint8*)ptr) + (n << KPageShift)); p != end; p++) {\
		*p = 0; \
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
	zeroPages(pde, 4);

	// Map peripheral memory as a couple of sections
	int peripheralMemIdx = KPeripheralBase >> KAddrToPdeIndexShift;
	for (int i = 0; i < KPeripheralSize >> KSectionShift; i++) {
		pde[peripheralMemIdx + i] = (KPeripheralPhys + (i << KSectionShift)) | KPdeSectionPeripheral;
	}

	// Map section zero
	pde[KSectionZero >> KAddrToPdeIndexShift] = KPhysicalSect0Pte | KPdePageTable;
	uint32* sectPte = (uint32*)KPhysicalSect0Pte;
	zeroPages(sectPte, 1);
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

	// And the section zero pte
	sectPte[PTE_IDX(KSectionZeroPte)] = KPhysicalSect0Pte | KPteKernelData;

	SetTTBR(0, KPhysicalPdeBase);
	SetTTBR(1, KPhysicalPdeBase);
	SetTTBCR(0);
	// Set DACR to get the hell out of the way
	uint32 everythingIsPermitted = 0xFFFFFFFF;
	asm("MCR p15, 0, %0, c3, c0, 0" : : "r" (everythingIsPermitted));
}

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

static void mmu_doMapPagesInSection(uintptr virtualAddress, uint32* sectPte, uintptr physicalAddress, int numPages) {
	for (int i = 0; i < numPages; i++) {
		uint32 phys = physicalAddress + (i << KPageShift);
		sectPte[PTE_IDX(virtualAddress) + i] = phys | KPteKernelData;
	}
}

void mmu_mapSect0Data(uintptr virtualAddress, uintptr physicalAddress, int npages) {
	uint32* sectPte = (uint32*)KSectionZeroPte;
	mmu_doMapPagesInSection(virtualAddress, sectPte, physicalAddress, npages);
}

void mmu_mapSection(PageAllocator* pa, uintptr virtualAddress) {
	// To map a whole section we need 256 contiguous pages *aligned on a section boundary*
	uintptr phys = pageAllocator_allocAligned(pa, KPageUsed, KPagesInSection, 1<<KSectionShift);
	uint32* pde = (uint32*)KKernelPdeBase;
	pde[virtualAddress >> KAddrToPdeIndexShift] = phys | KPdeSectionKernelData;
}

void mmu_mapSectionAsPages(PageAllocator* pa, uintptr virtualAddress, uintptr pteAddr) {
	// Get a shiny new page from the allocator, to hold the page table
	uint32 pageTablePhysical = pageAllocator_alloc(pa, KPageUsed, 1);
	// Map the PTE page into section zero (pteAddr is assumed to be in section zero)
	mmu_mapSect0Data(pteAddr, pageTablePhysical, KPageSize);
	// Now update so we can write to the new PTE page
	mmu_finishedUpdatingPageTables();
	zeroPages((uint32*)pteAddr, 1); // The section starts out with all pages unmapped

	// And update the kerk PDE with this PTE
	uint32* pde = (uint32*)KKernelPdeBase;
	pde[virtualAddress >> KAddrToPdeIndexShift] = pageTablePhysical | KPdePageTable;
}

void mmu_mapPageInSection(PageAllocator* pa, uint32* pte, uintptr virtualAddress) {
	uint32 newPagePhysical = pageAllocator_alloc(pa, KPageUsed, 1);
	pte[PTE_IDX(virtualAddress)] = newPagePhysical | KPteKernelData;
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
