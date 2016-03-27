#ifndef LUPI_AARCH64_H
#define LUPI_AARCH64_H

#ifndef AARCH64
#error "Configured processor is not AArch64"
#endif

#define WFI()					ASM_JFDI("WFI")
#define WFI_inline(val)			do { (void)val; asm("WFI"); } while(0)
#define DSB(reg)				asm("DSB")
#define DMB(reg)				asm("DMB")
#define READ_SPECIAL(reg, var)	asm("MRS %0, " #reg : "=r" (var))
#define WRITE_SPECIAL(reg, var)	asm("MSR " #reg ", %0" : : "r" (var))

static inline uintptr getFAR() {
	uintptr ret;
	READ_SPECIAL(FAR_EL1, ret);
	return ret;
}

#endif // LUPI_AARCH64_H
