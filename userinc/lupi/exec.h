#ifndef LUPI_EXEC_H
#define LUPI_EXEC_H

#define KFastExec 				0x00800000
#define KDriverHandle			0x00400000

#define KExecSbrk				1
#define KExecPrintString		2
#define KExecPutch				3
#define KExecGetch				4
#define KExecCreateProcess		5
#define KExecGetUptime			6
#define KExecThreadExit			7
#define KExecWaitForAnyRequest	8
#define KExecGetch_Async		9
#define KExecAbort				10

#define KExecNewSharedPage		11
#define KExecReleaseSharedPage	12
#define KExecCreateServer		13
#define KExecConnectToServer	14
#define KExecCompleteIpcRequest	15
#define KExecRequestServerMsg	16

#define KExecSetTimer			17
#define KExecReboot				18
#define KExecGetInt				19
#define KExecThreadYield		20

#define KExecDriverConnect		21

typedef enum {
	EValTotalRam,
	EValBootMode,
	EValScreenWidth,
	EValScreenHeight,
} ExecGettableValue;

#define KExecDriverTftBlit		0
#define KExecDriverTftInputRequest	1

#endif
