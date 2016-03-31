#include <k.h>
#include <mmu.h>
#include ARCH_HEADER
#include <atags.h>

// Sets up early stack, enables MMU, then calls Boot()
void NAKED start() {
	// Early stack from 0x8000
	asm("MOV x0, #32768");
	asm("MOV sp, x0");
	// Save ATAGS
	asm("MOV x19, x2");

	// Init MMU (don't enable yet)
	// asm("BL mmu_init");

	// Set exception vectors (this will give the MMU-enabled VA)
	asm("LDR x0, =.vectors");
	asm("MSR VBAR_EL1, x0");

	// Boot
	asm("MOV x0, x19");
	// asm("BL Boot");
	asm("B _hang");

	// Done!
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
