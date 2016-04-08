#include <k.h>
// #include <mmu.h>
#include ARCH_HEADER
#include <atags.h>

#define SCR_EL3_VALUE (SCR_EL3_NS | BIT(4) | BIT(5) | SCR_EL3_SMD | SCR_EL3_RW)
#define OSC_FREQ 1000000 // No idea

// CPU Extended Control Register, EL1
#define CPUECTLR_EL1		S3_1_c15_c2_1
#define CPUECTLR_EL1_SMPEN	(1 << 6)

// SCTLR_EL2, System Control Register (EL2)
#define SCTLR_EL2_RES1		(BIT(4) | BIT(5) | BIT(11) | BIT(16) | BIT(18) | BIT(22) | BIT(23) | BIT(28) | BIT(29))
#define SCTLR_EL2_VALUE		SCTLR_EL2_RES1

#define HCR_EL2_VALUE		HCR_EL2_RW

#define SCTLR_EL1_VALUE		SCTLR_EL1_RES1

void NAKED start() {
	LOAD_WORD(x0, OSC_FREQ);
	asm("MSR CNTFRQ_EL0, x0");

	// Needed?
	asm("MSR CNTVOFF_EL2, xzr");

	// Set Secure Configuration Register
	LOAD_WORD(x0, SCR_EL3_VALUE);
	asm("MSR SCR_EL3, x0");

	// CPU Extended Control Register
	asm("MOV x0, %0" : : "i" (CPUECTLR_EL1_SMPEN));
	asm("MSR S3_1_c15_c2_1, x0");

	LOAD_WORD(x0, SCTLR_EL2_VALUE);
	asm("MSR SCTLR_EL2, x0");

	// Switch to EL2
	asm("MOV x0, %0" : : "i" (SPSR_D | SPSR_A | SPSR_I | SPSR_F | SPSR_EL2h));
	asm("MSR SPSR_EL3, x0");
	// Stupid hack to avoid using ADR which doesn't work in clang...
	// asm("ADR x0, .start_el2");
	asm("BL .get_pc");
	// x0 now points to the instruction immediately following this comment
	asm("ADD x0, x0, #12"); // +12 to get to .start_el2
	asm("MSR ELR_EL3, x0");
	asm("ERET");

	asm(".start_el2:");
	LOAD_WORD(x0, HCR_EL2_VALUE);
	asm("MSR HCR_EL2, x0");
	LOAD_WORD(x0, SCTLR_EL1_VALUE);
	asm("MSR SCTLR_EL1, x0");

	// Switch to EL1
	asm("MOV x0, %0" : : "i" (SPSR_D | SPSR_A | SPSR_I | SPSR_F | SPSR_EL1h));
	asm("MSR SPSR_EL2, x0");
	asm("BL .get_pc");
	asm("ADD x0, x0, #12"); // +12 to get to .start_el1
	asm("MSR ELR_EL2, x0");
	asm("ERET");

	asm(".start_el1:");
	// Get CPU number
	asm("MRS x0, MPIDR_EL1");
	asm("AND x0, x0, #3");

	asm("CBZ x0, 1f"); // start_cpu
	// All other CPUs can go hang
	asm("B _hang");
	// Don't add any instructions between here and start_cpu0

	asm("1:"); asm("start_cpu0:");
	// Early stack at 520KB-516KB
	LOAD_WORD(x0, KPhysicalStackBase + KKernelStackSize);
	asm("MOV sp, x0");

	// Init MMU (don't enable yet)
#ifdef HAVE_MMU
	asm("BL _mmu_init");
#endif

	// Set exception vectors (this will give the MMU-enabled VA)
	asm("LDR x0, =.vectors");
	asm("MSR VBAR_EL1, x0");

	// Boot
	// aarch64 boot code doesn't configure ATAGS so hardcode what it should do
	asm("MOV x0, #0x100"); // atags addr = 0x100
	asm("BL _Boot");
	asm("B _hang");
	// Done!

	asm(".get_pc:");
	asm("MOV x0, lr");
	asm("RET");

	asm(".balign 0x400"); // Leave a hole for the ATAGS data 0x100-0x300

	// See ARMv8 ARM p1452 §D1.10.2 Exception Vectors
	asm(".balign 2048");
	asm(".vectors:");
	// 0x000: Exception taken from Current Exception level with SP_EL0.
	asm(".sync_exception_from_current_level:");
	asm("B _unhandledException"); // Sync
	asm(".balign 128");
	asm("B _unhandledException"); // IRQ
	asm(".balign 128");
	asm("B _unhandledException"); // FIQ
	asm(".balign 128");
	asm("B _unhandledException"); // SError

	asm(".balign 128");
	// 0x200: Exception taken from Current Exception level with SP_ELx, x>0.
	asm("B _unhandledException"); // Sync
	asm(".balign 128");
	asm("B _unhandledException"); // IRQ
	asm(".balign 128");
	asm("B _unhandledException"); // FIQ
	asm(".balign 128");
	asm("B _unhandledException"); // SError

	asm(".balign 128");
	// 0x400: Exception taken from Lower Exception level, where the implemented level immediately lower than the target level is using AArch64.
	// AKA the ones we care about. First is "synchronous exceptions"

	// See ARMv8 ARM p1453 §D1.10.4 "If the exception is a synchronous exception
	// or an SError interrupt, information characterizing the reason for the
	// exception is saved in the ESR_ELx at the Exception level the exception is
	// taken to"
	asm(".sync_exception_from_lower_level:");
	asm("MRS x30, ESR_EL1");
	// asm("CMP w30, %0, LSL %1" : : "i" (ESR_EC_SVC), "i" (ESR_EC_SHIFT));
	asm("MOV w29, %0" : : "i" (ESR_EC_SVC));
	asm("CMP w29, w30");
	asm("B.NE 1f");
	asm("B _svc");

	asm(".balign 128");
	asm("B _unhandledException"); // IRQ
	asm(".balign 128");
	asm("B _unhandledException"); // FIQ
	asm(".balign 128");
	asm("B _unhandledException"); // SError

	// Anything else, don't care
	asm(".balign 2048");
	asm("1:");
	asm("B _unhandledException");
}

#undef memcpy
void* memcpy(void* dst, const void* src, unsigned long n) {
	// TODO less completely dumb memcpy
	if (n) {
		const char* s = (const char*)src;
		char* d = dst;
		while (n--) { *d++ = *s++; }
	}
	return dst;
}

int memcmp(const void *s1, const void *s2, unsigned long n) {
	// Likewise
	for (const uint8 *c1 = (const uint8*)s1, *c2 = (const uint8*)s2;
		n > 0; ++c1, ++c2, --n) {
		int r = *c2 - *c1;
		if (r) return r;
	}
	return 0;
}

// Wouldn't it be nice if this was documented somewhere?

#define PM_RSTC						(KPeripheralBase + 0x0010001c)
#define PM_WDOG						(KPeripheralBase + 0x00100024)
#define PM_PASSWORD					0x5a000000
#define PM_RSTC_WRCFG_MASK			0x00000030
#define PM_RSTC_WRCFG_FULL_RESET	0x00000020

NORETURN reboot() {
	uint32 val;
	// I think this sets the watchdog timeout to 10 ticks (~150us)
	PUT32(PM_WDOG, 10 | PM_PASSWORD);
	// And this sets the behaviour when it expires?
	val = GET32(PM_RSTC);
	val &= ~PM_RSTC_WRCFG_MASK;
	val |= PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET;
	PUT32(PM_RSTC, val);
	// This doesn't reboot immediately, so hang until it does
	hang();
}
