#include <k.h>
#include ARCH_HEADER

void iThinkYouOughtToKnowImFeelingVeryDepressed();

void NAKED hang() {
	asm("MOV w0, #0");
	WFI(); // Stops us spinning like crazy
	asm("B _hang");
}

NOINLINE void NAKED dummy() {
	asm("RET");
}

NOINLINE NAKED uint32 GET32(uintptr addr) {
	asm("LDR w0, [x0]");
	asm("RET");
}

NOINLINE NAKED void PUT32(uintptr addr, uint32 val) {
    asm("STR w1, [x0]");
    asm("RET");
}

NOINLINE NAKED byte GET8(uintptr ptr) {
	asm("LDRB w0, [x0]");
	asm("RET");
}

void dumpRegisters(const uintptr* regs) {
/*
  x0-x3: 0000000000000000 0000000000000000 0000000000000000 0000000000000000
  x4-x7: 0000000000000000 0000000000000000 0000000000000000 0000000000000000
 x8-x11: 0000000000000000 0000000000000000 0000000000000000 0000000000000000
x12-x15: 0000000000000000 0000000000000000 0000000000000000 0000000000000000
...
savedpc: 0000000000000000   spsr: 00000000         savedsp: 0000000000000000
    far: 0000000000000000   "_el1:00000000             esr: 00000000
*/

	uintptr esr; // Exception syndrome register
	uintptr spsr_el1;
	READ_SPECIAL(ESR_EL1, esr);
	READ_SPECIAL(SPSR_EL1, spsr_el1);

	printk("  x0-x3: %lX %lX %lX %lX\n", regs[0], regs[1], regs[2], regs[3]);
	printk("  x4-x7: %lX %lX %lX %lX\n", regs[4], regs[5], regs[6], regs[7]);
	printk(" x8-x11: %lX %lX %lX %lX\n", regs[8], regs[9], regs[10],regs[11]);
	printk("x12-x15: %lX %lX %lX %lX\n", regs[12],regs[13],regs[14],regs[15]);
	printk("x16-x19: %lX %lX %lX %lX\n", regs[16],regs[17],regs[18],regs[19]);
	printk("x20-x23: %lX %lX %lX %lX\n", regs[20],regs[21],regs[22],regs[23]);
	printk("x24-x27: %lX %lX %lX %lX\n", regs[24],regs[25],regs[26],regs[27]);
	printk("x28-x31: %lX %lX %lX %lX\n", regs[28],regs[29],regs[30],regs[31]);
	printk("savedpc: %lX   spsr: %X         savedsp: %lX\n",
		regs[KSavedPc], (uint32)regs[KSavedSpsr], regs[KSavedSp]);
	printk("    far: %lX   \"_el1:%X             esr: %X\n",
		getFAR(), (uint32)spsr_el1, (uint32)esr);

	if (!TheSuperPage->marvin) {
		// First time we hit this, populate crashRegisters
		uintptr* cr = TheSuperPage->crashRegisters;
		memcpy(cr, regs, SIZEOF_SAVED_REGS);
	}
}

// TODO a lot of this can probably be simplified by moving the code into the appropriate bit of
// vectors, since "we've crashed" will correspond to sync_exception_from_current_level
NORETURN NAKED svc() {
	// r4-r15 are free to use
	LoadSuperPageAddress(x4);

	// // If we've crashed, we must be being called from the debugger so use the
	// // debugger svc stack
	// asm("LDRB w6, [x4, %0]" : : "i" (offsetof(SuperPage, marvin))); // w6 = marvin
	// asm("CBNZ w6, .loadDebuggerStack");

	// Otherwise get the appropriate thread svc stack
	asm("LDR x7, [x4, %0]" : : "i" (offsetof(SuperPage, currentThread))); // x7 = currentThread
	asm("LDRB w6, [x7, %0]" : : "i" (offsetof(Thread, index))); // w6 = currentThread->index

	asm("MOV w8, %0" : : "i" (KUserStacksBase)); // w8 - KUserStacksBase
	asm("ADD w8, w8, w6, lsl %0" : : "i" (USER_STACK_AREA_SHIFT));// w8 += svcStackOffset(w6)
	asm("ADD sp, x8, #4096"); // So sp points to top of stack not base

	// It makes rescheduling and crash debugging easier if we save the required
	// registers directly to the thread object rather than to the stack
	asm("ADD x8, x7, %0" : : "i" (offsetof(Thread, savedRegisters))); // x8 = &currentThread->savedRegisters
	SAVE_CALLEE_PRESERVED_REGISTERS_TO_THREAD(x8);
	asm("MOV x19, x8"); // x19 = &currentThread->savedRegisters

	asm(".postStackSet:");
	// asm("BL handleSvc");
	asm("MOV x8, x19");
	LOAD_CALLEE_PRESERVED_REGISTERS_FROM_THREAD(x8);
	// TODO technically only have to restore x19 if we reach here?
	RESET_SP_EL1_AND_ERET();

	// asm(".loadDebuggerStack:");
	// asm("ADR x15, .debuggerStackTop");
	// asm("LDR x15, [x15]");
	// // Just save regs to stack, as we can't be preempted in this mode
	// asm("SUB sp, x15, %0" : : "i" (SIZEOF_SAVED_REGS));
	// SAVE_CALLEE_PRESERVED_REGISTERS_TO_THREAD(sp);
	// asm("MOV x19, sp");
	// asm("B .postStackSet");
	// LABEL_ADDRESS(.debuggerStackTop, KLuaDebuggerSvcStackBase + KLuaDebuggerSvcStackSize);
}

const char KUnhandledExceptionStr[] = "Unhandled exception!\n";

// We are in EL1, exception may be from EL0 or EL1
// TODO worry about what stack we're using?
NORETURN NAKED unhandledException() {
	asm("ADD sp, sp, %0" : : "i" (SIZEOF_SAVED_REGS));
	EL1_SAVE_ALL_REGISTERS(sp, x30);

	asm("LDR x0, =_KUnhandledExceptionStr");
	asm("BL _printk");
	asm("MOV x0, sp");
	asm("BL _dumpRegisters");
	// asm("BL iThinkYouOughtToKnowImFeelingVeryDepressed");
	asm("B _hang");
}
