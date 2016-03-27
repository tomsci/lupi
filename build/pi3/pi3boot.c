#include <k.h>
#include <mmu.h>
#include ARCH_HEADER
#include <atags.h>

// Sets up early stack, enables MMU, then calls Boot()
void NAKED _start() {
	//TODO!
	asm("loop:");
	asm("cmp x0, #0");
	asm("b loop");
}
