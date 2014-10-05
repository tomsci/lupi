#ifndef ARMV7_M_H
#define ARMV7_M_H

#define KSystemControlSpace		0xE000E000u

// p169
#define SCB_VTOR				0xE000ED08u // Vector table offset register
#define SCB_SHPR1				0xE000ED18u // System handler priority registers
#define SCB_SHPR2				0xE000ED1Cu // (p181)
#define SCB_SHPR3				0xE000ED20u
#define SCB_SHCSR				0xE000ED24u // System Handler Control & State
#define SCB_CFSR				0xE000ED28u // Configurable Fault Status Reg
#define SCB_HFSR				0xE000ED2Cu // Hard Fault Status Register (p191)
#define SCB_MMAR				0xE000ED34u // Mem Management Fault Address Reg
#define SCB_BFAR				0xE000ED38u // Bus Fault Address Register

// p183
#define SHCSR_USGFAULTENA		(1 << 18)
#define SHCSR_BUSFAULTENA		(1 << 17)
#define SHCSR_MEMFAULTENA		(1 << 16)

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

#define KExcReturnHandler		(1)
#define KExcReturnThreadMain	(9)
#define KExcReturnThreadProcess	(0xD)

#endif
