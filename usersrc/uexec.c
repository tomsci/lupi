#include <stddef.h>
#include <lupi/exec.h>

void* NAKED sbrk(ptrdiff_t inc) {
	asm("MOV r1, r0");
	asm("MOV r0, %0" : : "i" (KExecSbrk));
	asm("SVC 0");
	asm("BX lr");
}

void NAKED lupi_printstring(const char* str) {
	asm("MOV r1, r0");
	asm("MOV r0, %0" : : "i" (KExecPrintString));
	asm("SVC 0");
	asm("BX lr");
}
