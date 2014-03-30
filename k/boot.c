#include <k.h>
#include <mmu.h>

void uart_init();
//void goDoLuaStuff();
void interactiveLuaPrompt();

void Boot() {
	uart_init();
	printk("\n\nLuPi version %s\n", LUPI_VERSION_STRING);

	printk("Start of code base is:\n");
	printk("%X = %X\n", KKernelCodeBase, *(uint32*)KKernelCodeBase);

#ifdef KLUA
	interactiveLuaPrompt();
#endif
}
