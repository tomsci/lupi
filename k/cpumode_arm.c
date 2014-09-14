#include <k.h>
#include <arm.h>

void iThinkYouOughtToKnowImFeelingVeryDepressed();

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
	asm("MOV r3, r14"); // r14_svc is address to return to user side
	// Also save it for ourselves in the case where we don't get preempted
	asm("MOV r4, r14");

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
	asm("MOV r3, #0");

	asm("MOVS pc, r4"); // r4 is where we stashed the user return address

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
