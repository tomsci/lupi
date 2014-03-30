#include <k.h>
#include <mmu.h>

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


// reg must contain zero for all of these
#define DMB(reg)				asm("MCR p15, 0, " #reg ", c7, c10, 5")
#define	DSB(reg)				asm("mcr p15, 0, " #reg ", c7, c10, 4")
#define	ISB(reg)				asm("mcr p15, 0, " #reg ", c7, c5, 4") // AKA prefetch flush

#define InvalidateIcache(reg)	asm("MCR p15, 0, " #reg ", c7, c5, 0")

#define SetTTBR(n, val)			asm("MCR p15, 0, %0, c2, c0, " #n : : "r" (val)) // p192
#define SetTTBCR(val)			asm("MCR p15, 0, %0, c2, c0, 2"   : : "r" (val)) // p193

#define KNumPdes 4096 // number of Page Directory Entries is always 4096x1MB entries, on a 32-bit machine (unless we're using truncated PDs, see below)

#define KNumPhysicalRamPages (KPhysicalRamSize >> KPageShift)


// See p356
//           19 18 17 16 | 15 14 13 12 | 11 10 9 8 | 7 6 5 4 | 3 2 1 0
//           ------------|-------------|-----------|---------|--------
// Section   NS  0 nG  S |APX ---TEX-- | --AP- P --Domain- XN| C B 1 0
// PageTable .... Page table address ....      P --Domain- 0 |NS 0 0 1

#define KPdeSectionKernelData	0x00000412 // NS=0, nG=0, S=0, APX=b001, XN=1, P=C=B=0
#define KPdeSectionJfdi			0x00000402 // NS=0, nG=0, S=0, APX=b001, XN=0, P=C=B=0
//#define KPdeSectionPeripheral	0x00002412 // NS=0, nG=0, S=0, APX=b001, TEX=b010, XN=1, P=C=B=0
#define KPdePageTable			0x00000001 // NS=0, P=0

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

uint32 makeCr() {
	uint32 cr;
	asm("MRC p15, 0, %0, c1, c0, 0" : "=r" (cr));
	//printk("Control register init = 0x%X\n", cr);
	cr = cr & ~CR_I; // unset I
	cr = cr | CR_XP;
	cr = cr | CR_M;
	//printk("Control register going to be 0x%x\n", cr);
	return cr;
}

//void mmu_enable(uintptr returnAddr) {
//	uint32 cr = makeCr();
//	asm("MCR p15, 0, %0, c1, c0, 0" : : "r" (cr));
//}

// Does not return; jumps to virtual address returnAddr
void NAKED mmu_enable(uintptr returnAddr) {
	asm("PUSH {r0}");
	asm("BL makeCr");
	asm("MOV r1, r0"); // r1 = CR
	asm("POP {r0}"); // r0 = returnAddr again

	asm("MOV r2, #0"); // r2 = 0
	DSB(r2);

	asm("MCR p15, 0, r1, c1, c0, 0"); // Boom!
	ISB(r2); // Prevent prefetch from when MMU was disabled from going beyond this point

	asm("BX r0");
	//TODO returnAddr will need to reenable instruction cache (and maybe data cache)
}

/*
inline void* memset(void* ptr, byte val, int len) {
	// TODO a more efficient version
	byte* b = (byte*)ptr;
	byte* end = b + len;
	while (b != end) {
		*b++ = val;
	}
	return ptr;
}
*/

static void zeroPages(void* ptr, int numPages) {
	uint32* p = ptr;
	while (numPages--) {
		void* end = p + (PAGE_SIZE / 4);
		while (p != end) {
			*p++ = 0;
		}
	}
}

/*
Enter and exit with MMU disabled
Sets up the minimal set of page tables at KPhysicalPdeBase

Note: Anything within the brackets of pde[xyz] or pte[xyz] should NOT contain the word 'physical'.
If it does, I've confused the fact that the PTE offsets refer to the VIRTUAL addresses.
*/
void REAL_mmu_init() {
	// Set up just enough mapping to allow access to the kern PDE, so we can enable the MMU and do
	// the rest of the PTE setup in virtual addresses
	uint32* pde = (uint32*)KPhysicalPdeBase;

	// Start off with the entire PDE invalidated
	// 00000000 in a PDE or PTE means 'no access', conveniently
	//memset(pde, 0, KNumPdes * sizeof(uint32));
	zeroPages(pde, 4); // Full PDE takes up 16kB, ie 4 pages

	// Map the kern PDEs themselves
	pde[KKernelPdeBase >> KAddrToPdeIndexShift] = KPhysicalPdeBase | KPdeSectionKernelData;

	// Map peripheral memory.
	int peripheralMemIdx = KPeripheralBase >> KAddrToPdeIndexShift;
	for (int i = 0; i < KPeripheralSize >> KOneMegShift; i++) {
		pde[peripheralMemIdx + i] = (KPeripheralPhys + (i << KOneMegShift)) | KPdeSectionKernelData;
	}

	// Set a page table for code and stuff
	pde[KCodeAnStuffSection >> KAddrToPdeIndexShift] = KPhysicalStuffPte | KPdePageTable;
	uint32* stuffPte = (uint32*)KPhysicalStuffPte;
	zeroPages(stuffPte, 1);

	// Map the PTE, which is itself within the KCodeAnStuffSection
	stuffPte[0] = KPhysicalStuffPte | KPteKernelCode;

	//TODO SuperPage

	// Map the kernel stack
	const int stackPageOffset = (KKernelStackBase & KSectionMask) >> KPageShift;
	stuffPte[stackPageOffset] = KPhysicalStackBase | KPteKernelData;
	stuffPte[stackPageOffset+1] = (KPhysicalStackBase + PAGE_SIZE) | KPteKernelData;

	// Map the code pages
	const int numCodePages = (KKernelCodesize >> KPageShift);
	const int codeStartPageOffset = (KKernelCodeBase & KSectionMask) >> KPageShift;
	ASSERT_COMPILE(numCodePages == 24);
	ASSERT_COMPILE(codeStartPageOffset == 8);
	for (int i = 0; i < numCodePages; i++) {
		int entry = codeStartPageOffset + i;
		stuffPte[entry] = (KPhysicalCodeBase + (i << KPageShift)) | KPteKernelCode;
	}

	// TODO what should TTBR0 be set to before we have any actual processes?
	// For now set to same as TTBR1
	uint32 ttbr0 = KPhysicalPdeBase; // We don't set any of the other bits in TTBR1
	asm("MCR p15, 0, %0, c2, c0, 0" : : "r" (ttbr0)); // p192


	uint32 ttbr1 = KPhysicalPdeBase; // We don't set any of the other bits in TTBR1
	asm("MCR p15, 0, %0, c2, c0, 1" : : "r" (ttbr1)); // p192

	// we will use TTBR0 for everything, temporarily
	uint32 ttbcr = 0; //TODO TTBCR_N;
	asm("MCR p15, 0, %0, c2, c0, 2" : : "r" (ttbcr)); // p193
	// That's it, all the remaining mappings can be done with MMU enabled
}

void mmu_init() {
	uint32* pde = (uint32*)KPhysicalPdeBase;
	zeroPages(pde, 4);

	uint32 phys = 0;
	for (uint32 i = 0; i < KNumPdes; i++) {
		uint32 entry = phys | KPdeSectionJfdi;
		//printk("PDE for %X = %X\n", i * (1024*1024), entry);
		pde[i] = entry;
		phys += 1 MB;
	}

	// Map peripheral memory.
	int peripheralMemIdx = KPeripheralBase >> KAddrToPdeIndexShift;
	for (int i = 0; i < KPeripheralSize >> KOneMegShift; i++) {
		pde[peripheralMemIdx + i] = (KPeripheralPhys + (i << KOneMegShift)) | KPdeSectionKernelData;
	}

	pde[0x4800000 >> KAddrToPdeIndexShift] = 0; // No access here

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
	SetTTBR(0, KPhysicalPdeBase);
	SetTTBR(1, KPhysicalPdeBase);
	SetTTBCR(0);
	// Set DACR to get the hell out of the way
	uint32 everythingIsPermitted = 0xFFFFFFFF;
	asm("MCR p15, 0, %0, c3, c0, 0" : : "r" (everythingIsPermitted));
}

/*
void mmu_freePagesForProcess(Process* p) {
	// First, indicate to our allocator that all the pages mapped to user mem are now available

	// Then, free up the pages used by the page tables
}
*/
