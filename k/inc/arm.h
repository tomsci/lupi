#ifndef LUPI_ARM_H
#define LUPI_ARM_H

// PSR control bits
#define KPsrIrqDisable 0x80
#define KPsrFiqDisable 0x40


#define KPsrModeSvc		0x13
#define KPsrModeAbort	0x17
#define KPsrModeUnd		0x1B
#define KPsrModeIrq		0x12

#endif
