#include <k.h>
#include <armv7-m.h>

void NAKED hang() {
	WFI(); // Stops us spinning like crazy
	asm("B hang");
}

void NAKED dummy() {
	asm("BX lr");
}

NOINLINE NAKED uint32 GET32(uint32 addr) {
	asm("LDR r0, [r0]");
	asm("BX lr");
}

NOINLINE NAKED void PUT32(uint32 addr, uint32 val) {
    asm("STR r1, [r0]");
    asm("BX lr");
}

NOINLINE NAKED byte GET8(uint32 ptr) {
	asm("LDRB r0, [r0]");
	asm("BX lr");
}

NOINLINE NAKED void PUT8(uint32 addr, byte val) {
    asm("STRB r1, [r0]");
    asm("BX lr");
}

ExceptionStackFrame* getExceptionStackFrame(uint32* spmain, uint32 excReturn) {
	// Must be called from handler mode (ie sp == sp_main)
	ExceptionStackFrame* esf;
	if (excReturn == KExcReturnThreadProcess) {
		// Exception frame will be on process stack
		asm("MRS %0, PSP" : "=r" (esf));
	} else {
		// It's on our stack, stackUsed bytes above current stack pointer
		esf = (ExceptionStackFrame*)spmain;
	}
	return esf;
}

int stackFrameSize(const ExceptionStackFrame* esf) {
	uint32 size = 32; // By default for non-FP-enabled
	if (esf->psr & (1 << 9)) size += 4; // Indicates padding
	return size;
}

ExceptionStackFrame* pushDummyExceptionStackFrame(uint32* sp, uint32 returnAddr) {
	// Push an ESF onto the stack below sp that returns to returnAddr
	uint32 psr = 1 << 24; // The thumb bit
	if ((((uintptr)sp) & 0x4) && (GET32(SCB_CCR) & CCR_STKALIGN)) {
		sp--;
		psr |= (1 << 9);
	}
	ExceptionStackFrame* esf = (ExceptionStackFrame*)((uintptr)sp - 32);
	esf->psr = psr;
	esf->r0 = 0;
	esf->r1 = 0;
	esf->r2 = 0;
	esf->r3 = 0;
	esf->r12 = 0;
	esf->lr = 0;
	esf->returnAddress = returnAddr;
	return esf;
}

// Generic Exception Handlers

void NAKED unhandledException() {
	asm("PUSH {r4-r11}");

	// asm("PUSH {r12, r14}");
	// printk("unhandledException!\n");
	// asm("POP {r12, r14}");

	asm("MOV r0, sp"); // r0 = &(r4-r11)
	asm("MOV r1, r14"); // r1 = the EXC_RETURN
	asm("BL dumpRegisters");
	asm("B iThinkYouOughtToKnowImFeelingVeryDepressed");
}

void NAKED svc() {
	// Since we are the lowest-priority exception we don't have to worry about
	// saving and restoring R4-R11 because uexec.c saves them before the svc
	// and no other handlers can be active.
	asm("MOV r4, lr"); // r4 = EXC_RETURN

	// However we do need to worry about fetching r0-r3 because we may be here
	// after returning from a higher-priority exception and not directly from
	// thread mode. Also have to handle case of MSP as well as PSP, for when
	// we're crashed, so call getExceptionStackFrame to fetch it.
	asm("MOV r0, sp");
	asm("MOV r1, r4"); // r1 = r4 = EXC_RETURN
	asm("BL getExceptionStackFrame");

	asm("MOV r5, r0"); // r5 = exceptionFrame
	asm("LDM r5, {r0-r2}");
	asm("MOV r3, #0"); // We don't need this param for armv7-m but handleSvc still expects it
	asm("BL handleSvc");
	// And now, have to write the result in r0 and r1 back to the ESF
	asm("STM r5, {r0, r1}");
	asm("BX r4");
}

void dumpRegisters(uint32* regs, uint32 excReturn) {
	// excReturn should be 0 if you just want to dump out the non-general
	// registers and SCB stuff.

	// regs must be eight registers R4 - R11 if excReturn is nonzero, otherwise
	// can be NULL if not bothered
	if (excReturn) {
		const ExceptionStackFrame* esf = getExceptionStackFrame(regs + 8, excReturn);
		uint32 r13 = (uintptr)esf + stackFrameSize(esf);

		uint32 ipsr;
		READ_SPECIAL(PSR, ipsr);

		printk(  "\nr0:  %X r1:  %X r2:  %X r3:  %X\n", esf->r0, esf->r1, esf->r2, esf->r3);
		if (regs) {
			printk("r4:  %X r5:  %X r6:  %X r7:  %X\n", regs[0], regs[1], regs[2], regs[3]);
			printk("r8:  %X r9:  %X r10: %X r11: %X\n", regs[4], regs[5], regs[6], regs[7]);
		}
		printk("r12: %X r13: %X r14: %X r15: %X\n", esf->r12, r13, esf->lr, esf->returnAddress);
		printk("PSR: %X ISR# %d EXC_RETURN: %X\n", esf->psr, ipsr & 0x1FF, excReturn);

		if (!TheSuperPage->marvin) {
			// First time we hit this, populate crashRegisters
			uint32* cr = TheSuperPage->crashRegisters;
			memcpy(cr, esf, 4 * sizeof(uint32)); // R0 - R3
			memcpy(cr + 4, regs, 8 * sizeof(uint32)); // R4 - R11
			cr[12] = esf->r12;
			cr[13] = r13;
			cr[14] = esf->lr;
			cr[15] = esf->returnAddress;
			cr[16] = esf->psr;
		}
	}
	uint32 cfsr = GET32(SCB_CFSR);
	printk("CFSR %X HFSR %X ICSR %X SHCSR %X\n", cfsr, GET32(SCB_HFSR),
		GET32(SCB_ICSR), GET32(SCB_SHCSR));

	uint32 primask, faultmask, basepri;
	READ_SPECIAL(PRIMASK, primask);
	READ_SPECIAL(FAULTMASK, faultmask);
	READ_SPECIAL(BASEPRI, basepri);
	printk("PRIMASK: %d FAULTMASK: %d BASEPRI: %d\n", primask, faultmask, basepri);
	if (cfsr & CFSR_BFARVALID) {
		printk("BFAR: %X\n", GET32(SCB_BFAR));
	}
	if (cfsr & CFSR_MMARVALID) {
		printk("MMAR: %X\n", GET32(SCB_MMAR));
	}
#ifdef KLUA
	// First word of Heap structure is the top addr
	printk("KLua heap top: %X\n", GET32(KLuaHeapBase));
#endif
}
