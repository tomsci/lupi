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

	// Region 1, 2 - user mem. Note region 1 uses a subregion disable to block
	// access to the kernel mem via the 0x20070000 alias
	SET_REGION(1, KRamBase, 16, KRasrUserData | (1 << 8)); // 1<<16 = 64KB
	SET_REGION(2, KRamBase + (1 << 16), 15, KRasrUserData); // 1<<15 = 32KB

	// Region 3 - kernel data
	SET_REGION(3, KSuperPageAddress, KPageShift + 1, KRasrKernelData);

	// Region 4,5 - User BSS
	// BSS is 544 bytes (32 + 512) so map as two regions
	SET_REGION(4, KUserBss, 5, KRasrUserData); // 1<<5 = 32 bytes
	SET_REGION(5, KUserBss + 32, 9, KRasrUserData); // 1<<9 = 512 bytes

	// Region 6 - peripheral
	SET_REGION(6, KPeripheralBase, 29, KRasrPeripheral); // 0x20000000 = 1<<29

	// Region 7 - SCB
	SET_REGION(7, KSystemControlSpace, KPageShift, KRasrScb);
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
