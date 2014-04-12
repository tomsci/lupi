#include <stddef.h>
#include <lupi/exec.h>

#define EXEC1(code) \
	asm("MOV r1, r0"); \
	asm("MOV r0, %0" : : "i" (code)); \
	asm("SVC 0"); \
	asm("BX lr") \


void* NAKED sbrk(ptrdiff_t inc) {
	EXEC1(KExecSbrk);
}

void NAKED lupi_printstring(const char* str) {
	EXEC1(KExecPrintString);
}

void NAKED exec_putch(uint ch) {
	EXEC1(KExecPutch);
}

uint NAKED exec_getch() {
	EXEC1(KExecGetch);
}

int NAKED exec_createProcess(const char* name) {
	EXEC1(KExecCreateProcess);
}
