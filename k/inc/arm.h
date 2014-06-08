#ifndef LUPI_ARM_H
#define LUPI_ARM_H

// PSR control bits p100
#define KPsrIrqDisable 0x80 // I
#define KPsrFiqDisable 0x40 // F

#define KPsrModeMask	0x1F
#define KPsrModeUsr		0x10
#define KPsrModeSvc		0x13
#define KPsrModeAbort	0x17
#define KPsrModeUnd		0x1B
#define KPsrModeIrq		0x12
#define KPsrModeSystem	0x1F

#define ModeSwitch(cpsr) asm("MSR cpsr_c, %0" : : "i" (cpsr))

// Secure configuration register p184
#define KScrAW			(1<<5)
#define KScrFW			(1<<5)
#define KScrNS			(1<<0)


// reg must contain zero for all of these
#define DMB(reg)				asm("MCR p15, 0, " #reg ", c7, c10, 5")
#define DSB(reg)				asm("MCR p15, 0, " #reg ", c7, c10, 4")
#define DSB_inline(var)			asm("MCR p15, 0, %0, c7, c10, 4" : : "r" (var))


// c7 c5 - p211
#define ISB(reg)				asm("MCR p15, 0, " #reg ", c7, c5, 4") // AKA prefetch flush, IMB()
#define ISB_inline(var)			asm("MCR p15, 0, %0, c7, c5, 4" : : "r" (var))
#define InvalidateIcache(reg)	asm("MCR p15, 0, " #reg ", c7, c5, 0") // Also flushes BTAC
#define FlushBTAC(reg)			asm("MCR p15, 0, " #reg ", c7, c5, 6")

#define InvalidateTLB(reg)		asm("MCR p15, 0, " #reg ", c8, c5, 0");


#define WFI(reg)				asm("MCR p15, 0, " #reg ", c7, c4, 0")

#define GetKernelStackTop(cond, reg) \
	asm("MOV" #cond " " #reg ", %0" : : "i" (KSectionZero)); \
	asm("ADD" #cond " " #reg ", " #reg ", %0" : : "i" (KKernelStackBase + KKernelStackSize - KSectionZero))

#endif
