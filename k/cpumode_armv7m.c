#include <k.h>
#include <armv7-m.h>

ExceptionStackFrame* getExceptionStackFrame(uint32* spmain, uint32 excReturn) {
	// Must be called from handler mode (ie sp == sp_main)
	ExceptionStackFrame* esf;
	if ((excReturn & 0xF) == KExcReturnThreadProcess) {
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
	// Wow this is so much easier than on ARM
	// Since we are the lowest-priority exception we don't have to worry about
	// saving and restoring R4-R11
	// Don't care about R12, just want to keep the stack aligned
	asm("PUSH {r12, lr}");
	asm("MOV r3, #0"); // We don't need this param for armv7-m but handleSvc still expects it
	asm("BL handleSvc");
	// Hmm at this point I think I need to stash r0, r1 in the relevant position
	// in the return stack
	asm("POP {r12, lr}");
	asm("STR r0, [sp, %0]" : : "i" (offsetof(ExceptionStackFrame, r0)));
	asm("STR r1, [sp, %0]" : : "i" (offsetof(ExceptionStackFrame, r1)));
	asm("BX lr");
}


void dumpRegisters(uint32* regs, uint32 excReturn) {
	// Must be called in handler mode, ie SP=SP_main
	// regs = eight registers R4 - R11
	const ExceptionStackFrame* esf = getExceptionStackFrame(regs + 8, excReturn);
	uint32 r13 = (uintptr)esf + stackFrameSize(esf);

	uint32 ipsr;
	asm("MRS %0, psr" : "=r" (ipsr));

	printk("r0:  %X r1:  %X r2:  %X r3:  %X\n", esf->r0,  esf->r1, esf->r2, esf->r3);
	printk("r4:  %X r5:  %X r6:  %X r7:  %X\n", regs[0],  regs[1], regs[2], regs[3]);
	printk("r8:  %X r9:  %X r10: %X r11: %X\n", regs[4],  regs[5], regs[6], regs[7]);
	printk("r12: %X r13: %X r14: %X r15: %X\n", esf->r12, r13,     esf->lr, esf->returnAddress);
	printk("CPSR was %X\n", esf->psr);
	printk("ISR Number: %d EXC_RETURN=%X\n", ipsr & 0x1FF, excReturn);
	uint32 cfsr = GET32(SCB_CFSR);
	printk("CFSR: %X HFSR: %X\n", cfsr, GET32(SCB_HFSR));
	// uint32 primask, faultmask, basepri;
	// asm("MRS %0, PRIMASK" : "=r" (primask));
	// asm("MRS %0, FAULTMASK" : "=r" (faultmask));
	// asm("MRS %0, BASEPRI" : "=r" (basepri));
	// printk("PRIMASK: %d FAULTMASK: %d BASEPRI: %d\n", primask, faultmask, basepri);
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
