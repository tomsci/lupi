#ifndef ARMV7_M_H
#define ARMV7_M_H

#define KSystemControlSpace		0xE000E000u

#define SCB_VTOR				0xE000ED08u

#define CFSR					0xE000ED28u

#define WFI(reg)				asm("WFI")
#define WFI_inline(val)			do { (void)val; asm("WFI"); } while(0)
#define DSB(reg)				asm("DSB")
#define DMB(reg)				asm("DMB")

#endif
