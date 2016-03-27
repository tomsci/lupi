#include <k.h>
#include <mmu.h>
#include <armv7-m.h>
#include <pageAllocator.h>

ASSERT_COMPILE(KKernelCodesize == (1 << KLogKernelCodesize));

#define KRasrCode		0x070B0001 // XN=0, AP=111, TEX=001, C=1, B=1, S=0
#define KRasrKernelData 0x110B0001 // XN=1, AP=001, TEX=001, C=1, B=1, S=0
#define KRasrUserData	0x130B0001 // XN=1, AP=011, TEX=001, C=1, B=1, S=0
#define KRasrPeripheral	0x11100001 // XN=1, AP=001, TEX=010, C=B=S=0
#define KRasrScb		0x11000001 // XN=1, AP=001, TEX=000, C=B=S=0

// This macro ensures that the constraints on addr and size alignment are correct
#define SET_REGION(num, addr, sizeShift, attribs) \
	PUT32(MPU_RNR, num); \
	/* Region addr must be aligned to the region size */ \
	ASSERT_COMPILE(((addr) & ((1 << (sizeShift)) - 1)) == 0); \
	PUT32(MPU_RBAR, addr); \
	PUT32(MPU_RASR, (((sizeShift) - 1) << RASR_SIZESHIFT) | (attribs))

/*
Enter and exit with MPU disabled
*/
void mmu_init() {
	// Default region map see p73
	// To configure a region, write RNR, RBAR and RASR

	// Region 0: code
	SET_REGION(0, KKernelCodeBase, KLogKernelCodesize, KRasrCode);

	// Region 1, 2 - user mem.
	SET_REGION(1, KRamBase, 16, KRasrUserData); // 1<<16 = 64KB
	SET_REGION(2, KRamBase + (1 << 16), 15, KRasrUserData); // 1<<15 = 32KB

	// Region 3 - kernel data. Note this will override region 2 which would
	// otherwise have granted user access to the superpage
	SET_REGION(3, KHandlerStackBase, KPageShift + 1, KRasrKernelData);

	// Region 4 - User BSS
	// BSS size is rounded up to 640 (512+128) bytes so we can map it as a
	// single 1KB region with 3 disabled subregions of 128 bytes each
	SET_REGION(4, KSuperPageAddress + 0xC00, 10, KRasrUserData | (0x07 << 8));

	// Region 5, 6 - Bit-banded user mem
	// r6 excludes 256KB (8KB real mem) for kernel, being 2 subregions
	SET_REGION(5, 0x22E00000, 21, KRasrUserData); // 1<<21 = 2MB (64KB real mem)
	SET_REGION(6, 0x23000000, 20, KRasrUserData | (0xC0 << 8)); // 1 << 20 = 1MB (32KB real mem)

	SET_REGION(7, 0, 0, 0); // Region 7 unused
}

void mmu_enable() {
	PUT32(MPU_CTRL, MPU_CTRL_PRIVDEFENA | MPU_CTRL_ENABLE);
	asm("DSB");
	asm("ISB");
}

Process* switch_process(Process* p) {
	if (!p) return NULL;
	Process* oldp = TheSuperPage->currentProcess;
	if (p == oldp) return NULL;
	TheSuperPage->currentProcess = p;
	return oldp;
}

void mmu_finishedUpdatingPageTables() {
	// Nothing needs doing
}
