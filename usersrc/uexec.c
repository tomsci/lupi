#include <stddef.h>
#include <lupi/exec.h>

typedef struct AsyncRequest AsyncRequest;

#define SLOW_EXEC1(code) \
	asm("MOV r1, r0"); \
	asm("MOV r0, %0" : : "i" (code)); \
	asm("PUSH {r4-r12}"); \
	asm("SVC 0"); \
	asm("POP {r4-r12}"); \
	asm("BX lr")

#define SLOW_EXEC2(code) \
	asm("MOV r2, r1"); \
	SLOW_EXEC1(code)

// User-side, fast execs are set up exactly the same as slow ones,
// except for ORing KFastExec
#define FAST_EXEC1(code) \
	asm("MOV r1, r0"); \
	asm("MOV r0, %0" : : "i" (code)); \
	asm("ORR r0, r0, %0" : : "i" (KFastExec)); \
	asm("PUSH {r4-r12}"); \
	asm("SVC 0"); \
	asm("POP {r4-r12}"); \
	asm("BX lr")

void* NAKED sbrk(ptrdiff_t inc) {
	SLOW_EXEC1(KExecSbrk);
}

void NAKED lupi_printstring(const char* str) {
	SLOW_EXEC1(KExecPrintString);
}

void NAKED exec_putch(uint ch) {
	SLOW_EXEC1(KExecPutch);
}

uint NAKED exec_getch() {
	SLOW_EXEC1(KExecGetch);
}

void NAKED exec_getch_async(AsyncRequest* request) {
	SLOW_EXEC1(KExecGetch_Async);
}

int NAKED exec_createProcess(const char* name) {
	SLOW_EXEC1(KExecCreateProcess);
}

uint64 NAKED exec_getUptime() {
	SLOW_EXEC1(KExecGetUptime);
}

void NAKED exec_threadExit(int reason) {
	SLOW_EXEC1(KExecThreadExit);
}

// returns number of completed requests
int NAKED exec_waitForAnyRequest() {
	FAST_EXEC1(KExecWaitForAnyRequest);
}

void NAKED exec_abort() {
	SLOW_EXEC1(KExecAbort);
}

void NAKED exec_reboot() {
	SLOW_EXEC1(KExecReboot);
}

uintptr NAKED exec_newSharedPage() {
	SLOW_EXEC1(KExecNewSharedPage);
}

int NAKED exec_createServer(uint32 serverId) {
	SLOW_EXEC1(KExecCreateServer);
}

int NAKED exec_connectToServer(uint32 server, void* ipcPage) {
	SLOW_EXEC2(KExecConnectToServer);
}

int NAKED exec_completeIpcRequest(AsyncRequest* ipcRequest, bool toServer) {
	SLOW_EXEC2(KExecCompleteIpcRequest);
}

void NAKED exec_requestServerMessage(AsyncRequest* serverRequest) {
	SLOW_EXEC1(KExecRequestServerMsg);
}

void NAKED exec_setTimer(AsyncRequest* request, uint64* time) {
	SLOW_EXEC2(KExecSetTimer);
}

int NAKED exec_getInt(ExecGettableValue val) {
	SLOW_EXEC1(KExecGetInt);
}
