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
#define KExecThreadCreate		21

#define KExecDriverConnect		22
#define KExecStfu				23
#define KExecReplaceProcess		24

typedef enum {
	EValTotalRam,
	EValBootMode,
	EValScreenWidth,
	EValScreenHeight,
	EValScreenFormat,
} ExecGettableValue;

typedef enum {
	EFiveSixFive,
	EOneBitColumnPacked,
} ScreenBufferFormat;

#define KExecDriverScreenBlit		0
#define KExecDriverInputRequest	1

typedef enum {
	InputTouchUp = 0,
	InputTouchDown = 1,
	InputButtons = 2,
} InputType;

typedef enum {
	InputButtonUp,
	InputButtonDown,
	InputButtonLeft,
	InputButtonRight,
	InputButtonA,
	InputButtonB,
	InputButtonSelect,
	InputButtonLight,
} InputButton;

#define KExecDriverFlashErase 1
#define KExecDriverFlashStatus 2
#define KExecDriverFlashRead 3
#define KExecDriverFlashWrite 4

#define KExecDriverAudioPlay 1
#define KExecDriverAudioPlayLoop 2

#endif
