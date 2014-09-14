#include <k.h>
#include <mmu.h>
#include <arm.h>
#include <atags.h>

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

	// Save ATAGS ptr for later, see
	// http://www.simtec.co.uk/products/SWLINUX/files/booting_article.html#d0e612
	asm("MOV r5, r2");

#ifdef NON_SECURE
	// Drop to non-secure mode by setting Secure Config Register
	// Must be done before mmu_init and mmu_setControlRegister otherwise we'll set up the
	// Secure config registers instead of the nonsecure
	asm("MOV r0, %0" : : "i" (KScrAW | KScrFW | KScrNS));
	asm("MCR p15, 0, r0, c1, c1, 0");
#endif

	asm("BL mmu_init");
#ifdef ICACHE_IS_STILL_BROKEN
	asm("BL makeCrForMmuEnable"); // r0 is now CR
	/* This next instruction fetches the virtual address (ie where we told the linker we were
	 * going to put stuff when we built the kernel) of the next instruction. When we enable the
	 * MMU all our code will magically move from physical addresses at 0x8000 to 0xF8008000,
	 * therefore we can't simply rely on LR or a PC-relative branch.
	 */
	asm("LDR r1, =.mmuEnableReturn");
	asm("BL mmu_setControlRegister");
#else
	asm("LDR r14, =.mmuEnableReturn");
	asm("B mmu_enable");
#endif

	asm(".mmuEnableReturn:");
	// From this point on, we are running with MMU on, and code is actually located where
	// the linker thought it was (ie KKernelCodeBase not KPhysicalCodeBase)

	// Set r13_und
	ModeSwitch(KPsrModeUnd | KPsrIrqDisable | KPsrFiqDisable);
	asm("LDR sp, .abtStack");

	// Set r13_abt
	ModeSwitch(KPsrModeAbort | KPsrIrqDisable | KPsrFiqDisable);
	asm("LDR sp, .abtStack");

	// Set r13_irq
	ModeSwitch(KPsrModeIrq | KPsrIrqDisable | KPsrFiqDisable);
	asm("LDR sp, .irqStack");

	// And back to supervisor mode to set our MMU-enabled stack address
	ModeSwitch(KPsrModeSvc | KPsrIrqDisable | KPsrFiqDisable);
	asm("LDR sp, .svcStack");

	// Set the Vector Base Address Register (§3.2.43)
	asm("LDR r0, .vectors");
	asm("MCR p15, 0, r0, c12, c0, 0");

	asm("MOV r0, r5"); // r0 = atags
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

void NAKED fiq() {
	printk("FIQ???\n");
	asm("SUBS pc, r14, #4");
}

extern char *strstr(const char *s, const char *find);

#define BOARDREV_STR "bcm2708.boardrev="

void parseAtags(uint32* ptr, AtagsParams* params) {
	// Grr there's a perfectly good atag for board rev (ATAG_REVISION) but the Pi
	// has to go dump it in a string we have to parse
	params->boardRev = 0;
	params->totalRam = 0;

	for (;;) {
		uint32 tagsize = ptr[0];
		uint32 tag = ptr[1];
		switch (tag) {
		case ATAG_NONE:
			return;
		case ATAG_CORE:
			// Pi doesn't put anything interesting here
			break;
		case ATAG_MEM:
			// There can in theory be multiple mem tags (although the Pi doesn't
			// do this)
			params->totalRam += ptr[2];
			break;
		case ATAG_CMDLINE: {
			// Pi puts it all in here...
			// Yuk, using user functions here. Sorry...
			char* cmdline = (char*)(ptr + 2);
			char* board = strstr(cmdline, BOARDREV_STR);
			if (board) {
				board = board + sizeof(BOARDREV_STR)-1;
				// Hackiest reimplementation of strtol follows...
				while (*board && *board != ' ') {
					char ch = *board;
					if (ch == 'x') { board++; continue; }
					int val = ch >= 'a' ? ch+0xA-'a' : ch >= 'A' ? ch+0xA-'A' : ch-'0';
					params->boardRev = (params->boardRev << 4) + val;
					board++;
				}
			}
			break;
		}
		default:
			break;
		}
		ptr += tagsize; // tagsize is in words, so this is the correct thing to do
	}
}

// Wouldn't it be nice if this was documented somewhere?

#define PM_RSTC						(KPeripheralBase + 0x0010001c)
#define PM_WDOG						(KPeripheralBase + 0x00100024)
#define PM_PASSWORD					0x5a000000
#define PM_RSTC_WRCFG_MASK			0x00000030
#define PM_RSTC_WRCFG_FULL_RESET	0x00000020

NORETURN reboot() {
	uint32 val;
	// I think this sets the watchdog timeout to 10 ticks (~150us)
	PUT32(PM_WDOG, 10 | PM_PASSWORD);
	// And this sets the behaviour when it expires?
	val = GET32(PM_RSTC);
	val &= ~PM_RSTC_WRCFG_MASK;
	val |= PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET;
	PUT32(PM_RSTC, val);
	// This doesn't reboot immediately, so hang until it does
	hang();
}
