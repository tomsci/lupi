#include <stddef.h>
#include <lupi/exec.h>

typedef struct AsyncRequest AsyncRequest;

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

void NAKED exec_getch_async(AsyncRequest* request) {
	EXEC1(KExecGetch_Async);
}

int NAKED exec_createProcess(const char* name) {
	EXEC1(KExecCreateProcess);
}

uint64 NAKED exec_getUptime() {
	EXEC1(KExecGetUptime);
}

void NAKED exec_threadExit(int reason) {
	EXEC1(KExecThreadExit);
}

// returns number of completed requests
int NAKED exec_waitForAnyRequest() {
	EXEC1(KExecWaitForAnyRequest);
}

void NAKED exec_abort() {
	EXEC1(KExecAbort);
}
