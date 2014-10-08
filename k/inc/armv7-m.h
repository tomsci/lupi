#ifndef ARMV7_M_H
#define ARMV7_M_H

#define KSystemControlSpace		0xE000E000

// p157
#define NVIC_ISER0				0xE000E100
#define NVIC_ISER1				0xE000E104
#define NVIC_ICER0				0xE000E180
#define NVIC_ICER1				0xE000E184

#define NVIC_IPR0				0xE000E400
#define NVIC_IPR1				0xE000E404
#define NVIC_IPR2				0xE000E408
#define NVIC_IPR3				0xE000E40C
#define NVIC_IPR4				0xE000E410
#define NVIC_IPR5				0xE000E414
#define NVIC_IPR6				0xE000E418
#define NVIC_IPR7				0xE000E41C

// p169
#define SCB_ICSR				0xE000ED04 // Interrupt & Control State Register
#define SCB_VTOR				0xE000ED08 // Vector table offset register
#define SCB_SHPR1				0xE000ED18 // System handler priority registers
#define SCB_SHPR2				0xE000ED1C // (p181)
#define SCB_SHPR3				0xE000ED20
#define SCB_SHCSR				0xE000ED24 // System Handler Control & State
#define SCB_CFSR				0xE000ED28 // Configurable Fault Status Reg
#define SCB_HFSR				0xE000ED2C // Hard Fault Status Register (p191)
#define SCB_MMAR				0xE000ED34 // Mem Management Fault Address Reg
#define SCB_BFAR				0xE000ED38 // Bus Fault Address Register

// p172
#define ICSR_PENDSVCLR			(1 << 27)
#define ICSR_PENDSVSET			(1 << 28)
#define ICSR_VECTACTIVE_MASK	(0x1F)

// p183
#define SHCSR_USGFAULTENA		(1 << 18)
#define SHCSR_BUSFAULTENA		(1 << 17)
#define SHCSR_MEMFAULTENA		(1 << 16)
#define SHCSR_SVCALLACT			(1 << 7)

// pp186-189
#define CFSR_BFARVALID			(1 << 15)
#define CFSR_MMARVALID			(1 << 7)

#define WFI(reg)				asm("WFI")
#define WFI_inline(val)			do { (void)val; asm("WFI"); } while(0)
#define DSB(reg)				asm("DSB")
#define DMB(reg)				asm("DMB")

// See p643 of the ARM arch v7m
typedef struct ExceptionStackFrame {
	uint32 r0;
	uint32 r1;
	uint32 r2;
	uint32 r3;
	uint32 r12;
	uint32 lr;
	uint32 returnAddress;
	uint32 psr;
} ExceptionStackFrame;

ASSERT_COMPILE(sizeof(ExceptionStackFrame) == 8*4);

ExceptionStackFrame* getExceptionStackFrame(uint32* spmain, uint32 excReturn);
int stackFrameSize(const ExceptionStackFrame* esf);

// Valid values for the bottom 4 bits of EXC_RETURN
#define KExcReturnHandler		(1)
#define KExcReturnThreadMain	(9)
#define KExcReturnThreadProcess	(0xD)

static inline ExceptionStackFrame* getThreadExceptionStackFrame() {
	return getExceptionStackFrame(0, KExcReturnThreadProcess);
}

// doc11057 p194
#define SYSTICK_CTRL	0xE000E010
#define SYSTICK_LOAD	0xE000E014
#define SYSTICK_VAL		0xE000E018
#define SYSTICK_CALIB	0xE000E01C

#define SYSTICK_CTRL_ENABLE		(1 << 0)
#define SYSTICK_CTRL_TICKINT	(1 << 1)
#define SYSTICK_CTRL_CLKSOURCE	(1 << 2)
#define SYSTICK_CTRL_COUNTFLAG	(1 << 16)

#define EXCEPTION_NUMBER_SVCALL	11

#define SVCallActive()	(GET32(SCB_SHCSR) & SHCSR_SVCALLACT)
#define SVCallCurrent()	((GET32(SCB_ICSR) & ICSR_VECTACTIVE_MASK) == EXCEPTION_NUMBER_SVCALL)
#endif
