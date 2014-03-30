#ifndef LUPI_ARM_H
#define LUPI_ARM_H

// PSR control bits
#define KPsrIrqDisable 0x80
#define KPsrFiqDisable 0x40


#define KPsrModeSvc		0x13
#define KPsrModeAbort	0x17
#define KPsrModeUnd		0x1B

static inline uint32 getFAR() {
	uint32 ret;
	asm("MRC p15, 0, %0, c6, c0, 0" : "=r" (ret));
	return ret;
}

static inline uint32 getDFSR() {
	uint32 ret;
	asm("MRC p15, 0, %0, c5, c0, 0" : "=r" (ret));
	return ret;
}

static inline uint32 getIFSR() {
	uint32 ret;
	asm("MRC p15, 0, %0, c5, c0, 1" : "=r" (ret));
	return ret;
}

#endif
