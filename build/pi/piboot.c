#include <mmu.h>
#include <arm.h>

// Sets up early stack, enables MMU, then calls Boot()
void NAKED _start() {
	// Skip over the exception vectors when we actually run this code during init
	asm("B .postVectors"); // Branches to labels are program-relative so not a problem
	asm("B undefinedInstruction");	// +0x04
	asm("B svc");					// +0x08
	asm("B prefetchAbort");			// +0x0C
	asm("B dataAbort");				// +0x10
	asm("NOP");						// +0x14
	asm("B irq");					// +0x18
	asm("B fiq");					// +0x1C

	asm(".postVectors:");

	// Early stack grows down from 0x8000. Code is at 0x8000 up
	asm("MOV sp, #0x8000");

	asm("BL mmu_init");
	asm("BL makeCrForMmuEnable"); // r0 is now CR
	asm("LDR r1, =.mmuEnableReturn");
	asm("BL mmu_setControlRegister");
	asm(".mmuEnableReturn:");
	// From this point on, we are running with MMU on, and code is actually located where
	// the linker thought it was (ie KKernelCodeBase not KPhysicalCodeBase)

	// Set r13_und
	asm("MSR cpsr_c, %0" : : "i" (KPsrModeUnd | KPsrIrqDisable | KPsrFiqDisable));
	asm("LDR sp, .abtStack");

	// Set r13_abt
	asm("MSR cpsr_c, %0" : : "i" (KPsrModeAbort | KPsrIrqDisable | KPsrFiqDisable));
	asm("LDR sp, .abtStack");

	// Set r13_irq
	asm("MSR cpsr_c, %0" : : "i" (KPsrModeIrq | KPsrIrqDisable | KPsrFiqDisable));
	asm("LDR sp, .irqStack");

	// And back to supervisor mode to set our MMU-enabled stack address
	asm("MSR cpsr_c, %0" : : "i" (KPsrModeSvc | KPsrIrqDisable | KPsrFiqDisable));
	asm("LDR sp, .svcStack");

	// Set the Vector Base Address Register (§3.2.43)
	asm("LDR r0, .vectors");
	asm("MCR p15, 0, r0, c12, c0, 0");

	asm("BL Boot");
	asm("B hang");

	LABEL_WORD(.vectors, KKernelCodeBase);
	LABEL_WORD(.abtStack, KAbortStackBase + KPageSize);
	LABEL_WORD(.irqStack, KIrqStackBase + KPageSize);
	LABEL_WORD(.svcStack, KKernelStackBase + KKernelStackSize);
}

#if 0
void StuffWeDontDoAnyMore() {
//	printk("About to do undefined instruction...\n");
//	WORD(0xE3000000);

//	printk("Start of code base is:");
//	printk("%X\n", *(uint32*)KKernelCodeBase);

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
#endif

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

static inline uint32 getFAR() {
	uint32 ret;
	asm("MRC p15, 0, %0, c6, c0, 0" : "=r" (ret));
	return ret;
}

static inline uint32 getDFSR() {
	uint32 ret;
	asm("MRC p15, 0, %0, c5, c0, 0" : "=r" (ret));
	return ret;
}

static inline uint32 getIFSR() {
	uint32 ret;
	asm("MRC p15, 0, %0, c5, c0, 1" : "=r" (ret));
	return ret;
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
