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


//#define KKernelMemBase 0xF0000000
//#define KUserProcessPdeBase  0xF0000000
//#define KUserProcessPageBase 0xF0100000
//#define KPageAllocatorBase   0xF0....

//static uint32* getPageDirectoryForProcess(uint processIdx) {
	//TODO
//}

// reg must contain zero
#define DMB(reg) asm("MCR p15, 0, " #reg " c7, c10, 5")


#define KNumPdes 4096 // number of Page Directory Entries is always 4096x1MB entries, on a 32-bit machine (unless we're using truncated PDs, see below)

#define KNumPhysicalRamPages (KPhysicalRamSize >> KPageShift)


#define KPdeSectionKernelData	0x00080412 // NS=1, AP=b01, XN=1, C=B=0
#define KPdePageTable			0x00000009

#define KPteKernelCode			0x00000212 // C=B=0, XN=0, APX=b101, S=?? TEX=0, nG=0
#define KPteKernelData			0x00000013 // C=B=0, XN=1, APX=b001, S=??, TEX=0, nG=0


// Control register bits, see §3.2.7
#define CR_XP (1<<23) // Extended page tables, not sure if impacts ARMv6 compatability mode
#define CR_I  (1<<12) // Enable Instruction cache
#define CR_C  (1<<2)  // Enable Data cache
#define CR_M  (1)     // Enable MMU


// Does not return; jumps to virtual address returnAddr
void NAKED enable_mmu(uintptr returnAddr) {
	// Disable instruction cache (I bit)
	asm("MRC p15, 0, r1, c1, c0, 0"); // r1 = CR
	asm("BIC r1, r1, #%a0" : : "i" (CR_I)); // unset I
	asm("MOV r2, #0");
	asm("MCR p15, 0, r2, c7, c5, 0"); // invalidate icache
	asm("ORR r1, r1, #%a0" : : "i" (CR_M)); // enable M (MMU)
	asm("MCR p15, 0, r1, c1, c0, 0"); // Boom!
	// What happens now? Have I just invalidated PC? Maybe it's already in the pipeline.
	asm("BX r0");
	//TODO returnAddr will need to reenable instruction cache (and maybe data cache)
}

inline void* memset(void* ptr, byte val, int len) {
	// TODO a more efficient version
	byte* b = (byte*)ptr;
	byte* end = b + len;
	while (b != end) {
		*b++ = val;
	}
	return ptr;
}

#define zeroPage(addr) memset(addr, 0, PAGE_SIZE)

/*
Enter and exit with MMU disabled
Sets up the minimal set of page tables at KPhysicalPdeBase

Note: Anything within the brackets of pde[xyz] or pte[xyz] should NOT contain the word 'physical'.
If it does, I've confused the fact that the PTE offsets refer to the VIRTUAL addresses.
*/
void mmu_init() {
	// Set up just enough mapping to allow access to the kern PDE, so we can enable the MMU and do
	// the rest of the PTE setup in virtual addresses
	uint32* pde = (uint32*)KPhysicalPdeBase;

	// Start off with the entire PDE invalidated
	// 00000000 in a PDE or PTE means 'no access', conveniently
	memset(pde, 0, KNumPdes * sizeof(uint32));

	// Map the kern PDEs themselves
	pde[KKernelPdeBase >> KAddrToPdeIndexShift] = KPhysicalPdeBase | KPdeSectionKernelData;

	// Set a page table for code and stuff
	pde[KCodeAnStuffSection >> KAddrToPdeIndexShift] = KPhysicalStuffPte | KPdePageTable;
	uint32* stuffPte = (uint32*)KPhysicalStuffPte;
	zeroPage(stuffPte);

	// Map the PTE, which is itself within the KCodeAnStuffSection
	stuffPte[0] = KPhysicalStuffPte | KPteKernelCode;

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

	// TODO TTBCR

	// That's it, all the remaining mappings can be done with MMU enabled
}

void mmu_freePagesForProcess(Process* p) {
	// First, indicate to our allocator that all the pages mapped to user mem are now available

	// Then, free up the pages used by the page tables
}

