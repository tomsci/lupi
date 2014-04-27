#ifndef LUPI_EXEC_H
#define LUPI_EXEC_H

#define KExecSbrk			1
#define KExecPrintString	2
#define KExecPutch			3
#define KExecGetch			4
#define KExecCreateProcess	5
#define KExecGetUptime		6
#define KExecThreadExit		7
#define KExecWaitForAnyRequest	8
#define KExecGetch_Async	9


#define KAsyncFlagPending   1 // Is added to the RunLoop pendingRequests
#define KAsyncFlagAccepted	2 // Has been passed to the kernel
#define KAsyncFlagCompleted 4 // Has been completed by the kernel
#define KAsyncFlagIntResult 8

#endif
