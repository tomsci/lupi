#include "piboot.h"

// Sets up early stack, does Stuff
void NAKED _start() {
	// Skip over the exception vectors when we actually run this code during init
	eB(r0, ".postVectors");
	// undefined instruction vector
	asm("B undefinedInstruction");	// +0x04
	asm("B svc");					// +0x08
	asm("B prefetchAbort");			// +0x0C
	asm("B dataAbort");				// +0x10
	asm("NOP");						// +0x14
	asm("B irq");					// +0x18
	asm("B fiq");					// +0x1C

	asm(".postVectors:");

	// Set r13_und
	asm("MSR cpsr_c, %0" : : "i" (KPsrModeUnd | KPsrIrqDisable | KPsrFiqDisable));
	asm("MOV sp, %0" : : "i" (KPhysicalAbortModeStackBase + PAGE_SIZE));
	asm("MSR cpsr_c, %0" : : "i" (KPsrModeSvc | KPsrIrqDisable | KPsrFiqDisable));

	// Set r13_abt
	asm("MSR cpsr_c, %0" : : "i" (KPsrModeAbort | KPsrIrqDisable | KPsrFiqDisable));
	asm("MOV sp, %0" : : "i" (KPhysicalAbortModeStackBase + PAGE_SIZE));
	asm("MSR cpsr_c, %0" : : "i" (KPsrModeSvc | KPsrIrqDisable | KPsrFiqDisable));

	// Set the Vector Base Address Register (§3.2.43)
	asm("ADR r0, _start"); // r0 = &_start
	asm("MCR p15, 0, r0, c12, c0, 0");

	// Early stack grows down from 0x8000. Code is at 0x8000 up
	asm("MOV sp, #0x8000");

	eBL(r3, "mmu_init");
	eBL(r3, "makeCrForMmuEnable"); // r0 is now CR
	asm("LDR r1, =.mmuEnableReturn"); // Or where the linker thinks this is, anyway
	eB(r3, "mmu_setControlRegister");
	asm(".mmuEnableReturn:");

//	// Right, we've got some mem but our stack needs setting up again
//	asm("LDR sp, .stackaddr");

	eBL(r1, "Boot");
	asm("B hang");

//	LABEL_WORD(.stackaddr, KKernelStackBase+KKernelStackSize);
}

void Boot() {
	uart_init();
	printk("\n\nLuPi version %s\n", LUPI_VERSION_STRING);
	//mmu_init();

//	uintptr returnAddr;
//	asm("LDR %0, =.postMmuEnable" : "=r" (returnAddr));
//	//printk("About to enable MMU...\n");
//	mmu_enable(returnAddr);

//	asm(".postMmuEnable:");
//	printk("About to do undefined instruction...\n");
//	WORD(0xE3000000);

	printk("About to access invalid memory...\n");
	uint32* inval = (uint32*)0x4800000;
	printk("*0x4800000 = %x\n", *inval);

	printk("Shouldn't get here\n");

	//superpage_init();

//	uint reg;
//	asm("mrs %0,CPSR" : "=r"(reg));
//	printk("CPSR = 0x%x\n", reg);
//	asm("mrs %0,SPSR" : "=r"(reg));
//	printk("SPSR = 0x%x\n", reg);
//
//	asm("mrc p15, 0, %0, c1, c1, 0" : "=r" (reg));
//	printk("Secure Configuration Register = %x\n", reg);
//
//	asm("mrc p15, 0, %0, c12, c0, 0" : "=r" (reg));
//	printk("Vector base address register = %x\n", reg);

	//worddump(0, 0x400);

	//printk("CodeAndStuffSection PTE:\n");
	//worddump((void*)KPhysicalStuffPte, 0x100);

	//char* cmdline = (char*)0x100;
	//printk("cmdline.txt = \n");
	//hexdump(0, 4096);

	//goDoLuaStuff();

	//interactiveLuaPrompt();
}

void NAKED dummy() {
	asm("BX lr");
}

void NAKED hang() {
	asm("B hang");
}

void NAKED undefinedInstruction() {
	uint32 addr;
	asm("MOV %0, r14" : "=r" (addr));
	addr -= 4; // r14_und is the instruction after
	printk("Undefined instruction at 0x%X\n", addr);
	hang();
}

void NAKED prefetchAbort() {
	uint32 addr;
	asm("MOV %0, r14" : "=r" (addr));
	addr -= 4; // r14_abt is the instruction after
	printk("Prefetch abort at 0x%X ifsr=%X far=%X\n", addr, getIFSR(), getFAR());
	hang();
}

void svc() {
	//TODO
	asm("MOVS pc, r14");
}

void NAKED dataAbort() {
	uint32 addr;
	asm("MOV %0, r14" : "=r" (addr));
	addr -= 8; // r14_abt is 8 bytes after (PC always 2 ahead for mem access)
	printk("Data abort at 0x%X dfsr=%X far=%X\n", addr, getDFSR(), getFAR());

	hang();
}

void NAKED irq() {
	//TODO
	asm("SUBS pc, r14, #4");
}

void NAKED fiq() {
	//TODO
	asm("SUBS pc, r14, #4");
}
