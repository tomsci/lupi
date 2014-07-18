#include <k.h>
#include <arm.h>

static NAKED USED void irqSwitch() {
	asm(".irqSwitch:");
	// Switch to IRQ move while keeping the stack the same
	asm("PUSH {r0, r14}"); // Make sure we don't trample these
	asm("MOV r0, r13");
	ModeSwitch(KPsrModeIrq | KPsrIrqDisable | KPsrFiqDisable);
	asm("MOV r13, r0");
	asm("POP {r0, r14}");
	asm("BX lr");
}

static NAKED USED void irqReturn() {
	asm(".irqReturn:");
	// Return from IRQ move while keeping the stack the same
	asm("PUSH {r0, r14}"); // Make sure we don't trample these
	asm("MOV r0, r13");
	ModeSwitch(KPsrModeSvc | KPsrIrqDisable | KPsrFiqDisable);
	asm("MOV r13, r0");
	asm("MOV r14, r1");
	asm("BX lr");
}

static NAKED bool atomic_cas_interrupted(uint32* ptr, uint32 expectedVal, uint32 newVal, uint32 interruptedVal) {
	asm("PUSH {r4, r14}");
	asm("1:");
	asm("LDREX r4, [r0]"); // r4 = *ptr
	asm("CMP r4, r1"); // Does *ptr == expectedVal?
	asm("BNE .cas_failed");
	// This is the point at which the LDREX/STREX becomes important - if something
	// changes *ptr between us checking it in the LDREX and when the STREX stores newVal
	// So simulate an IRQ
	asm("BL .irqSwitch");
	asm("LDREX r4, [r0]");
	asm("STREX r4, r3, [r0]"); // Set to interruptedVal
	asm("BL .irqReturn");

	asm("STREX r4, r2, [r0]"); // *ptr = newVal, r3 = strexSucceeded
	asm("CMP r4, #0");
	asm("BEQ 1b");
	asm("MOV r0, #1"); // Succeeded
	asm("BX lr");

	asm(".cas_failed:");
	asm("CLREX");
	asm("MOV r0, #0");
	asm("POP {r4, r14}");
	asm("BX lr");
	__builtin_unreachable(); // Silence compiler
}

static NAKED uint32 atomic_inc_interrupted(uint32* ptr) {
	asm("1:");
	asm("LDREX r1, [r0]");
	asm("ADD r1, r1, #1");

	// Again, this is the point in the read-modify-write that would mess us up
	asm("BL .irqSwitch");
	asm("LDREX r2, [r0]");
	asm("ADD r2, r2, #1");
	asm("STREX r4, r2, [r0]"); // Set to r2, ie *ptr + 2
	asm("BL .irqReturn");

	asm("STREX r2, r1, [r0]");
	asm("CMP r2, #0");
	asm("BEQ 1b");
	asm("MOV r0, r1");
	asm("BX lr");
	__builtin_unreachable(); // Silence compiler
}


void test_atomics() {
	ASSERT_MODE(KPsrModeSvc);
	kern_disableInterrupts(); // That's a given

	uint32 var = 0x01d;
	bool suceeded = atomic_cas_interrupted(&var, 0x01d, 0xBEEE5, 0xDEADBEE5);
	// This should not succeed, and should not update the variable, because it
	// was interrupted and changed before we'd sucessfully 
	ASSERT(!suceeded, var);
	ASSERT(var == 0xDEADBEE5, var); // The bees should all be dead

	printk("tested atomic_cas_interrupted ok.\n");

	var = 0x11223344;
	atomic_inc_interrupted(&var);
	ASSERT(var == 0x11223346, var);
}
