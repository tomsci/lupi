#include <std.h>

// Otherwise atomic_setbool definition is wrong
ASSERT_COMPILE(sizeof(bool) == sizeof(byte));

#if defined(AARCH64)

//TODO

#elif defined(WORKING_LDREX)

/**
Returns the value previously at this location. If this function is interrupted
and ptr is set by an IRQ or another thread, it will retry.

Usage:

	uint32* ptr = (uint32*)&TheSuperPage->blockedUartReceiveIrqHandler;
	Thread* thrd = (Thread*)atomic_set(ptr, NULL);
	if (thrd) {
		// Then we sucessfully took ownership of the pointer
	}
*/
uint32 NAKED atomic_set(uint32* ptr, uint32 val) {
	asm("1:");
	asm("LDREX r2, [r0]");
	asm("STREX r3, r1, [r0]");
	asm("CMP r3, #0");
	asm("BEQ 1b");
	asm("MOV r0, r2");
	asm("BX lr");
}

/**
Atomically increments *ptr and returns the new value. In other words, it is an
atomic version of:

	*ptr += 1;
	return *ptr;
*/
uint32 NAKED atomic_inc(uint32* ptr) {
	asm("1:");
	asm("LDREX r1, [r0]");
	asm("ADD r1, r1, #1");
	asm("STREX r2, r1, [r0]");
	asm("CMP r2, #0");
	asm("BEQ 1b");
	asm("MOV r0, r1");
	asm("BX lr");
}

uint8 NAKED atomic_set8(uint8* ptr, uint8 val) {
	asm("1:");
	asm("LDREXB r2, [r0]");
	asm("STREXB r3, r1, [r0]");
	asm("CMP r3, #0");
	asm("BEQ 1b");
	asm("MOV r0, r2");
	asm("BX lr");
}

uint8 NAKED atomic_inc8(uint8* ptr) {
	asm("1:");
	asm("LDREXB r1, [r0]");
	asm("ADD r1, r1, #1");
	asm("STREXB r2, r1, [r0]");
	asm("CMP r2, #0");
	asm("BEQ 1b");
	asm("MOV r0, r1");
	asm("BX lr");
}

bool NAKED atomic_cas(uint32* ptr, uint32 expectedVal, uint32 newVal) {
	asm("1:");
	asm("LDREX r3, [r0]"); // r3 = *ptr
	asm("CMP r3, r1"); // Does *ptr == expectedVal?
	asm("BNE .cas_failed");
	asm("STREX r3, r2, [r0]"); // *ptr = newVal, r3 = strexSucceeded
	asm("CMP r3, #0");
	asm("BEQ 1b");
	asm("MOV r0, #1"); // Succeeded
	asm("BX lr");

	asm(".cas_failed:");
	asm("CLREX");
	asm("MOV r0, #0");
	asm("BX lr");
}

bool NAKED atomic_cas8(uint8* ptr, uint8 expectedVal, uint8 newVal) {
	asm("1:");
	asm("LDREXB r3, [r0]"); // r3 = *ptr
	asm("CMP r3, r1"); // Does *ptr == expectedVal?
	asm("BNE .cas_failed");
	asm("STREXB r3, r2, [r0]"); // *ptr = newVal, r3 = strexSucceeded
	asm("CMP r3, #0");
	asm("BEQ 1b"); // Interupted, so retry
	asm("MOV r0, #1"); // Succeeded
	asm("BX lr");
}

#else

#ifdef ARMV7_M

#define INTERRUPTS_OFF(contextReg, tempReg) \
	asm("MRS " #contextReg ", PRIMASK"); \
	asm("CPSID i")

#define INTERRUPTS_ON(contextReg) \
	asm("MSR PRIMASK, " #contextReg)

#else

#include <arm.h>

// Note these definitions don't assume interrupts were actually enabled prior to the call
#define INTERRUPTS_OFF(contextReg, tempReg) \
	asm("MRS " #contextReg ", cpsr"); \
	asm("ORR " #tempReg ", " #contextReg ", %0" : : "i" (KPsrIrqDisable)); \
	asm("MSR cpsr_c, " #tempReg)

#define INTERRUPTS_ON(contextReg) \
	asm("MSR cpsr_c, " #contextReg)

#endif // ARMV7_M

uint32 NAKED atomic_set(uint32* ptr, uint32 val) {
	INTERRUPTS_OFF(r2, r3);
	asm("LDR r3, [r0]");
	asm("STR r1, [r0]");
	asm("MOV r0, r3");
	INTERRUPTS_ON(r2);
	asm("BX lr");
}

uint32 NAKED atomic_inc(uint32* ptr) {
	INTERRUPTS_OFF(r2, r3);
	asm("LDR r1, [r0]");
	asm("ADD r1, r1, #1");
	asm("STR r1, [r0]");
	asm("MOV r0, r1");
	INTERRUPTS_ON(r2);
	asm("BX lr");
}

uint8 NAKED atomic_set8(uint8* ptr, uint8 val) {
	INTERRUPTS_OFF(r2, r3);
	asm("LDRB r3, [r0]");
	asm("STRB r1, [r0]");
	asm("MOV r0, r3");
	INTERRUPTS_ON(r2);
	asm("BX lr");
}

uint8 NAKED atomic_inc8(uint8* ptr) {
	INTERRUPTS_OFF(r2, r3);
	asm("LDRB r1, [r0]");
	asm("ADD r1, r1, #1");
	asm("STRB r1, [r0]");
	asm("MOV r0, r1");
	INTERRUPTS_ON(r2);
	asm("BX lr");
}

bool NAKED atomic_cas(uint32* ptr, uint32 expectedVal, uint32 newVal) {
	asm("PUSH {r4}");
	INTERRUPTS_OFF(r4, r3);
	asm("LDR r3, [r0]");
	asm("CMP r3, r1");
	asm("ITEE NE");
	asm("MOVNE r0, #0");
	asm("STREQ r2, [r0]");
	asm("MOVEQ r0, #1");
	INTERRUPTS_ON(r4);
	asm("POP {r4}");
	asm("BX lr");
}

bool NAKED atomic_cas8(uint8* ptr, uint8 expectedVal, uint8 newVal) {
	asm("PUSH {r4}");
	INTERRUPTS_OFF(r4, r3);
	asm("LDRB r3, [r0]");
	asm("CMP r3, r1");
	asm("ITEE NE");
	asm("MOVNE r0, #0");
#ifdef ARM
	// Sigh...
	asm("STREQB r2, [r0]");
#else
	asm("STRBEQ r2, [r0]");
#endif
	asm("MOVEQ r0, #1");
	INTERRUPTS_ON(r4);
	asm("POP {r4}");
	asm("BX lr");
}

#endif
