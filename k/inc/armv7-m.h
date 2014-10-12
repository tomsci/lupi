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
#define SCB_AIRCR				0xE000ED0C // App Interrupt & Reset Control Reg
#define SCB_CCR					0xE000ED14 // Configuration and Control Register
#define SCB_SHPR1				0xE000ED18 // System handler priority registers
#define SCB_SHPR2				0xE000ED1C // (p181)
#define SCB_SHPR3				0xE000ED20
#define SCB_SHCSR				0xE000ED24 // System Handler Control & State
#define SCB_CFSR				0xE000ED28 // Configurable Fault Status Reg
#define SCB_HFSR				0xE000ED2C // Hard Fault Status Register (p191)
#define SCB_MMAR				0xE000ED34 // Mem Management Fault Address Reg
#define SCB_BFAR				0xE000ED38 // Bus Fault Address Register

// p200
#define MPU_TYPE				0xE000ED90
#define MPU_CTRL				0xE000ED94
#define MPU_RNR					0xE000ED98 // Region Number Register
#define MPU_RBAR				0xE000ED9C // Region Base Address Register
#define MPU_RASR				0xE000EDA0 // Region Attribute and Size Register

#define MPU_CTRL_PRIVDEFENA		(1 << 2)
#define MPU_CTRL_ENABLE			(1 << 0)

// p172
#define ICSR_PENDSVCLR			(1 << 27)
#define ICSR_PENDSVSET			(1 << 28)
#define ICSR_VECTACTIVE_MASK	(0x1F)

// p176
#define AIRCR_VECTKEY			(0x05FA << 16)
#define AIRCR_SYSRESETREQ		(1 << 2)

// p179
#define CCR_NONBASETHRDENA		(1 << 0)
#define CCR_STKALIGN			(1 << 9)

// p183
#define SHCSR_USGFAULTENA		(1 << 18)
#define SHCSR_BUSFAULTENA		(1 << 17)
#define SHCSR_MEMFAULTENA		(1 << 16)
#define SHCSR_PENDSVACT			(1 << 10)
#define SHCSR_SVCALLACT			(1 << 7)

#define SHPR_SVCALL				(SCB_SHPR2 + 3)
#define SHPR_PENDSV				(SCB_SHPR3 + 2)
#define SHPR_SYSTICK			(SCB_SHPR3 + 3)

// pp186-189
#define CFSR_BFARVALID			(1 << 15)
#define CFSR_MMARVALID			(1 << 7)

// p69
#define CONTROL_PSP				(1 << 1)
#define CONTROL_NPRIV			(1 << 0)

#define WFI()					ASM_JFDI("WFI")
#define WFI_inline(val)			do { (void)val; asm("WFI"); } while(0)
#define DSB(reg)				asm("DSB")
#define DMB(reg)				asm("DMB")
#define READ_SPECIAL(reg, var)	asm("MRS %0, " #reg : "=r" (var))
#define WRITE_SPECIAL(reg, var)	asm("MSR " #reg ", %0" : : "r" (var))

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

// Valid values for EXC_RETURN
#define KExcReturnHandler		(0xFFFFFFF1)
#define KExcReturnThreadMain	(0xFFFFFFF9)
#define KExcReturnThreadProcess	(0xFFFFFFFD)

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
#define SvOrPendSvActive() (GET32(SCB_SHCSR) & (SHCSR_SVCALLACT | SHCSR_PENDSVACT))
#define SVCallCurrent()	((GET32(SCB_ICSR) & ICSR_VECTACTIVE_MASK) == EXCEPTION_NUMBER_SVCALL)

static inline uint32 getFAR() {
	uint32 cfsr = GET32(SCB_CFSR);
	if (cfsr & CFSR_MMARVALID) {
		return GET32(SCB_MMAR);
	} else if (cfsr & CFSR_BFARVALID) {
		return GET32(SCB_BFAR);
	} else {
		return 0;
	}
}

#define RFE_TO_MAIN(addrReg) \
	asm("MOV r1, " #addrReg); \
	asm("MOV r0, sp"); \
	asm("ADD sp, sp, #40"); \
	asm("BL pushDummyExceptionStackFrame"); /* returns esf addr */ \
	asm("MOV sp, r0"); \
	/* Compiler is smart enough to make this MOV a MVN.w */ \
	asm("MOV r0, %0" : : "i" (KExcReturnThreadMain)); \
	asm("BX r0")

#endif
