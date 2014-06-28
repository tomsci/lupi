#include <stddef.h>

void* sbrk(ptrdiff_t inc);

#define REGSET(rx, val) \
	asm("MOV " #rx ", #" #val); \
	asm("ADD " #rx ", " #rx ", r0")

#define CHECKREG(rx, val) \
	asm("MOV r14, #" #val); \
	asm("ADD r14, r0"); \
	asm("CMP " #rx ", r14"); \
	asm("BNE .boom")

	//asm("MOVNE r14, #0xC0");
	//asm("LDRNE r0, [r14]")


void NAKED SetRegsAndSvc(uint32 i) {
	asm("PUSH {r4-r12, lr}");
	asm("PUSH {r0}");
	REGSET(r1, 0x11000000);
	REGSET(r2, 0x22000000);
	REGSET(r3, 0x33000000);
	REGSET(r4, 0x44000000);
	REGSET(r5, 0x55000000);
	REGSET(r6, 0x66000000);
	REGSET(r7, 0x77000000);
	REGSET(r8, 0x88000000);
	REGSET(r9, 0x99000000);
	REGSET(r10, 0xAA000000);
	REGSET(r11, 0xBB000000);
	REGSET(r12, 0xCC000000);

	asm("PUSH {r0-r3}");
	asm("MOV r0, #0");
	asm("BLX sbrk");
	asm("POP {r0-r3}");

	asm("POP {r0}");
	CHECKREG(r1, 0x11000000);
	CHECKREG(r2, 0x22000000);
	CHECKREG(r3, 0x33000000);
	CHECKREG(r4, 0x44000000);
	CHECKREG(r5, 0x55000000);
	CHECKREG(r6, 0x66000000);
	CHECKREG(r7, 0x77000000);
	CHECKREG(r8, 0x88000000);
	CHECKREG(r9, 0x99000000);
	CHECKREG(r10, 0xAA000000);
	CHECKREG(r11, 0xBB000000);
	CHECKREG(r12, 0xCC000000);

	asm("POP {r4-r12, pc}");

	LABEL_WORD(.boom, 0xE7FC0CF5); // Undefined instruction
}

int newProcessEntryPoint() {
	int j = 1;
	while (j+=16) {
		for (int i = 0; i < 16; i++) {
			SetRegsAndSvc(i+j);
		}
	}
	return 0;
}
