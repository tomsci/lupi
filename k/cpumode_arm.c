#include <k.h>
#include <arm.h>

void iThinkYouOughtToKnowImFeelingVeryDepressed();
static void dumpRegisters(uint32* regs, uint32 pc, uint32 dataAbortFar);

// Contains code specific to the ARM register and CPU mode model

void NAKED dummy() {
	asm("BX lr");
}

NOINLINE NAKED uint32 GET32(uint32 addr) {
	asm("ldr r0,[r0]");
	asm("bx lr");
}

NOINLINE NAKED void PUT32(uint32 addr, uint32 val) {
    asm("str r1,[r0]");
    asm("bx lr");
}

NOINLINE NAKED byte GET8(uintptr ptr) {
	asm("LDRB r0, [r0]");
	asm("BX lr");
}

void NAKED undefinedInstruction() {
	asm("PUSH {r0-r12}");
	uint32* regs;
	asm("MOV %0, sp" : "=r" (regs));
	uint32 addr;
	asm("MOV %0, r14" : "=r" (addr));
	addr -= 4; // r14_und is the instruction after
	printk("Undefined instruction at %X\n", addr);
	dumpRegisters(regs, addr, 0);
	iThinkYouOughtToKnowImFeelingVeryDepressed();
}

uint32 getCpsr() {
	uint32 ret;
	GetCpsr(ret);
	return ret;
}

uint32 getSpsr() {
	uint32 ret;
	GetSpsr(ret);
	return ret;
}

void NAKED prefetchAbort() {
	asm("PUSH {r0-r12}");
	uint32* regs;
	asm("MOV %0, sp" : "=r" (regs));
	uint32 addr;
	asm("MOV %0, r14" : "=r" (addr));
	addr -= 4; // r14_abt is the instruction after
	printk("Prefetch abort at %X ifsr=%X\n", addr, getIFSR());
	dumpRegisters(regs, addr, 0);
	iThinkYouOughtToKnowImFeelingVeryDepressed();
}

//#define STACK_DEPTH_DEBUG

#ifdef STACK_DEPTH_DEBUG
#define UNUSED_STACK 0x1A1A1A1A

void svc_cleanstack() {
	uint32 p = svcStackBase(TheSuperPage->currentThread->index);
	uint32 endp = (uint32)&p; // Don't trash anything above us otherwise we'll break svc()
	for (; p != endp; p += sizeof(uint32)) {
		*(uint32*)p = UNUSED_STACK;
	}
}

void svc_checkstack(uint32 execId) {
	// Find low-water mark of stack
	uint32 p = svcStackBase(TheSuperPage->currentThread->index);
	const uint32 endp = p + KPageSize;
	for (; p != endp; p += sizeof(uint32)) {
		if (*(uint32*)p != UNUSED_STACK) {
			// Found it
			printk("Exec %d used %d bytes of stack\n", execId, endp - p);
			break;
		}
	}
}
#endif

void NAKED svc() {
	// user r4-r12 has already been saved user-side so we can use them for temps

	// r4 = TheSuperPage
	asm("MOV r4, %0" : : "i" (KSuperPageAddress));

	// r8 = svcPsrMode
	asm("LDRB r8, [r4, %0]" : : "i" (offsetof(SuperPage, svcPsrMode)));

	// If we've crashed, we must be being called from the debugger so use the
	// debugger svc stack
	asm("LDRB r9, [r4, %0]" : : "i" (offsetof(SuperPage, marvin)));
	asm("CMP r9, #0");
	asm("BNE .loadDebuggerStack");

	// Reenable interrupts (depending on what svcPsrMode says)
	ModeSwitchReg(r8);

	// Now setup the right stack
	asm("LDR r5, [r4, %0]" : : "i" (offsetof(SuperPage, currentThread)));
	asm("LDRB r6, [r5, %0]" : : "i" (offsetof(Thread, index)));
	asm("MOV r7, %0" : : "i" (KUserStacksBase));
	asm("ADD r13, r7, r6, LSL %0" : : "i" (USER_STACK_AREA_SHIFT));
	asm("ADD r13, r13, #4096"); // So r13 points to top of stack not base

	asm(".postStackSet:");
	// r14_svc is address to return to user side
	// Save it for ourselves in the case where we don't get preempted
	asm("PUSH {r14}");
	// And set r3 to point to it for handleSvc
	asm("MOV r3, sp");
	// Finally, align stack, might as well use r14 again
	asm("PUSH {r14}");

	#ifdef STACK_DEPTH_DEBUG
		asm("PUSH {r0-r3}");
		asm("MOV r11, r0"); // Save the exec id for later
		asm("BL svc_cleanstack");
		asm("POP {r0-r3}");
	#endif
	// r0, r1, r2 already have the correct data in them for handleSvc()
	asm("BL handleSvc");
	#ifdef STACK_DEPTH_DEBUG
		asm("PUSH {r0-r1}");
		asm("MOV r0, r11");
		asm("BL svc_checkstack");
		asm("POP {r0-r1}");
	#endif
	// Avoid leaking kernel info into user space (like we really care!)
	asm("MOV r2, #0");

	asm("POP {r3, r4}");
	asm("MOVS pc, r4");

	asm(".loadDebuggerStack:");
	asm("LDR r13, .debuggerStackTop");
	asm("B .postStackSet");
	LABEL_WORD(.debuggerStackTop, KLuaDebuggerSvcStackBase + 0x1000);
}

void NAKED dataAbort() {
	asm("PUSH {r0-r12, r14}");
	uint32* regs;
	asm("MOV %0, sp" : "=r" (regs));
	uint32 addr;
	asm("MOV %0, r14" : "=r" (addr));
	addr -= 8; // r14_abt is 8 bytes after (PC always 2 ahead for mem access)
	printk("Data abort at %X dfsr=%X far=%X\n", addr, getDFSR(), getFAR());
	dumpRegisters(regs, addr, getFAR());
	iThinkYouOughtToKnowImFeelingVeryDepressed();
	// We might want to return from this if we were already aborted - note we return to
	// r14-4 not r14-8, ie we skip over the instruction that caused the exception
	asm("POP {r0-r12, r14}");
	asm("SUBS pc, r14, #4");
}

static uintptr stackBaseForMode(uint32 mode) {
	switch (mode) {
		case KPsrModeUsr: return userStackForThread(TheSuperPage->currentThread);
		case KPsrModeSvc:
			// TODO will probably have to revisit this once again...
			if (TheSuperPage->currentThread) {
				return svcStackBase(TheSuperPage->currentThread->index);
			} else {
				return KKernelStackBase;
			}
		case KPsrModeAbort: return KAbortStackBase;
		case KPsrModeUnd: return KAbortStackBase;
		case KPsrModeIrq: return KIrqStackBase;
		case KPsrModeSystem:
			// We only ever use this mode when kernel debugging where we're using the top of the
			// debugger heap section
			return KLuaDebuggerStackBase;
		default:
			return 0;
	}
}

static void dumpRegisters(uint32* regs, uint32 pc, uint32 dataAbortFar) {
	uint32 spsr, r13, r14;
	asm("MRS %0, spsr" : "=r" (spsr));
	const uint32 crashMode = spsr & KPsrModeMask;
	if (crashMode == KPsrModeUsr) {
		uint32 bnked[2] = {0x13131313,0x14141414};
		uint32* bankedStart = bnked;
		// The compiler will 'optimise' out the STM into a single "str %0, [sp]" unless
		// I include the volatile. The fact there's the small matter of the '^' which it is
		// IGNORING when making that decision... aaargh!
		ASM_JFDI("STM %0, {r13, r14}^" : : "r" (bankedStart));
		r13 = bnked[0];
		r14 = bnked[1];
	} else {
		// Mode is something privileged - switch back to it briefly to get r13 and 14
		uint32 currentMode;
		asm("MRS %0, cpsr" : "=r" (currentMode));
		int zero = 0;
		uint32 tempMode = crashMode | KPsrIrqDisable | KPsrFiqDisable; // Keep interrupts off
		asm("MSR cpsr_c, %0" : : "r" (tempMode)); // ModeSwitch(tempMode)
		DSB_inline(zero);
		asm("MOV %0, r13" : "=r" (r13));
		asm("MOV %0, r14" : "=r" (r14));
		asm("MSR cpsr_c, %0" : : "r" (currentMode)); // ModeSwitch(currentMode)
	}

	printk("r0:  %X r1:  %X r2:  %X r3:  %X\n", regs[0],  regs[1],  regs[2],  regs[3]);
	printk("r4:  %X r5:  %X r6:  %X r7:  %X\n", regs[4],  regs[5],  regs[6],  regs[7]);
	printk("r8:  %X r9:  %X r10: %X r11: %X\n", regs[8],  regs[9],  regs[10], regs[11]);
	printk("r12: %X r13: %X r14: %X r15: %X\n", regs[12], r13,      r14,      pc);
	printk("CPSR was %X\n", spsr);
	uintptr stackBase = stackBaseForMode(crashMode);
	if (r13 < stackBase && dataAbortFar && dataAbortFar < stackBase) {
		printk("BLOWN STACK!\n");
	}
	if (!TheSuperPage->marvin) {
		// First time we hit this, populate crashRegisters
		uint32* cr = TheSuperPage->crashRegisters;
		memcpy(cr, regs, 12*sizeof(uint32));
		cr[13] = r13;
		cr[14] = r14;
		cr[15] = pc;
		cr[16] = spsr;
	}
}
