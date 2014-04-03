#ifndef LUPI_ARM_H
#define LUPI_ARM_H

// PSR control bits
#define KPsrIrqDisable 0x80
#define KPsrFiqDisable 0x40

#define KPsrModeSvc		0x13
#define KPsrModeAbort	0x17
#define KPsrModeUnd		0x1B
#define KPsrModeIrq		0x12


// reg must contain zero for all of these
#define DMB(reg)				asm("MCR p15, 0, " #reg ", c7, c10, 5")
#define	DSB(reg)				asm("MCR p15, 0, " #reg ", c7, c10, 4")
#define	ISB(reg)				asm("MCR p15, 0, " #reg ", c7, c5, 4") // AKA prefetch flush

#define InvalidateIcache(reg)	asm("MCR p15, 0, " #reg ", c7, c5, 0")

#define WFI(reg)				asm("MCR p15, 0, " #reg ", c7, c4, 0")


#endif
