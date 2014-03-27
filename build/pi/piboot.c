#include <k.h>
#include <mmu.h> // DEBUG

void uart_init();
void mmu_init();

#ifdef MMU_DISABLED

#define eBL(reg, label) asm("BL " label)
#define eB(reg, label) asm("B " label)

#else

// The linker has set the code address to 0xF8008000 because that's where we'll map it
// once we've setup the MMU. However before that happens, and branch instructions will be
// off by 0xF8000000 because the PI bootloader loads the kernel image at 0x8000.
#define eBL(reg, label) \
	asm("LDR " #reg ", =" label); \
	asm("SUB " #reg ", " #reg ", #0xF8000000"); \
	asm("BLX " #reg)

#define eB(reg, label) \
asm("LDR " #reg ", =" label); \
asm("SUB " #reg ", " #reg ", #0xF8000000"); \
asm("BX " #reg)

#endif

#ifdef MMU_DISABLED

// Sets up early stack, does Stuff
void NAKED _start() {
	// Early stack grows down from 0x8000. Code is at 0x8000 up
	asm("MOV sp, #0x8000");
	eBL(r1, "Boot");

//	eBL(r1, "mmu_init");
//	asm("LDR r0, =.mmuEnableReturn"); // Or where the linker thinks this is, anyway
//	eB(r1, "enable_mmu");
//
//	asm(".mmuEnableReturn:");
//	// Right, we've got some mem but our stack needs setting up again
//	asm("LDR sp, .stackaddr");

	asm("B hang");

//	LABEL_WORD(.stackaddr, KKernelStackBase+KKernelStackSize);
}

void goDoLuaStuff();
void interactiveLuaPrompt();
void putch(byte);

void Boot() {
	uart_init();
	printk("\n\nLuPi version %s\n", LUPI_VERSION_STRING);
	mmu_init();

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

	interactiveLuaPrompt();
}

#else

#error TODO

#endif

void NAKED dummy() {
	asm("BX lr");
}

void NAKED hang() {
	asm("B hang");
}
