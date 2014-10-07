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
enum ExceptionStackFrame {
	EsfR0,
	EsfR1,
	EsfR2,
	EsfR3,
	EsfR12,
	EsfLr,
	EsfReturnAddress,
	EsfPsr,
};

// Valid values for the bottom 4 bits of EXC_RETURN
#define KExcReturnHandler		(1)
#define KExcReturnThreadMain	(9)
#define KExcReturnThreadProcess	(0xD)

// doc11057 p194
#define SYSTICK_CTRL	0xE000E010
#define SYSTICK_LOAD	0xE000E014
#define SYSTICK_VAL		0xE000E018
#define SYSTICK_CALIB	0xE000E01C

#define SYSTICK_CTRL_ENABLE		(1 << 0)
#define SYSTICK_CTRL_TICKINT	(1 << 1)
#define SYSTICK_CTRL_CLKSOURCE	(1 << 2)
#define SYSTICK_CTRL_COUNTFLAG	(1 << 16)

#define SVCallActive()	(GET32(SCB_SHCSR) & SHCSR_SVCALLACT)

#endif
