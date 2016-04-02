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

// Seriously, clang is just so bad at this (or at least the mach-o backend is)
// offset is number of instructions
#define CBZ_instr(reg, offset)	(0xB4000000 | (((offset) & 0x3FFFF) << 5) | reg)
#define CBZ(reg, offset)		asm(".word %c0" : : "i" (CBZ_instr(reg, offset)))

static inline uintptr getFAR() {
	uintptr ret;
	READ_SPECIAL(FAR_EL1, ret);
	return ret;
}

// reg + offset should point to where x19 should go. Reg is not updated.
#define SAVE_CALLEE_PRESERVED_REGISTERS(reg, offset) \
	asm("STP x19, x20, [" #reg ", %0]" : : "i" (offset + 0)); \
	asm("STP x21, x22, [" #reg ", %0]" : : "i" (offset + 16)); \
	asm("STP x23, x24, [" #reg ", %0]" : : "i" (offset + 32)); \
	asm("STP x25, x26, [" #reg ", %0]" : : "i" (offset + 48)); \
	asm("STP x27, x28, [" #reg ", %0]" : : "i" (offset + 64)); \
	asm("STP x29, x30, [" #reg ", %0]" : : "i" (offset + 80))

#define LOAD_CALLEE_PRESERVED_REGISTERS(reg, offset) \
	asm("LDP x19, x20, [" #reg ", %0]" : : "i" (offset + 0)); \
	asm("LDP x21, x22, [" #reg ", %0]" : : "i" (offset + 16)); \
	asm("LDP x23, x24, [" #reg ", %0]" : : "i" (offset + 32)); \
	asm("LDP x25, x26, [" #reg ", %0]" : : "i" (offset + 48)); \
	asm("LDP x27, x28, [" #reg ", %0]" : : "i" (offset + 64)); \
	asm("LDP x29, x30, [" #reg ", %0]" : : "i" (offset + 80))


// Note, doesn't preserve r18 (currently)
#define SAVE_TEMP_REGISTERS(reg, offset) \
	asm("STP x0,  x1,  [" #reg ", %0]" : : "i" (offset + 0)); \
	asm("STP x2,  x3,  [" #reg ", %0]" : : "i" (offset + 16)); \
	asm("STP x4,  x5,  [" #reg ", %0]" : : "i" (offset + 32)); \
	asm("STP x6,  x7,  [" #reg ", %0]" : : "i" (offset + 48)); \
	asm("STP x8,  x9 , [" #reg ", %0]" : : "i" (offset + 64)); \
	asm("STP x10, x11, [" #reg ", %0]" : : "i" (offset + 80)); \
	asm("STP x12, x13, [" #reg ", %0]" : : "i" (offset + 96)); \
	asm("STP x14, x15, [" #reg ", %0]" : : "i" (offset + 112)); \
	asm("STP x16, x17, [" #reg ", %0]" : : "i" (offset + 128))

#define LOAD_TEMP_REGISTERS(reg, offset) \
	asm("LDP x0,  x1,  [" #reg ", %0]" : : "i" (offset + 0)); \
	asm("LDP x2,  x3,  [" #reg ", %0]" : : "i" (offset + 16)); \
	asm("LDP x4,  x5,  [" #reg ", %0]" : : "i" (offset + 32)); \
	asm("LDP x6,  x7,  [" #reg ", %0]" : : "i" (offset + 48)); \
	asm("LDP x8,  x9 , [" #reg ", %0]" : : "i" (offset + 64)); \
	asm("LDP x10, x11, [" #reg ", %0]" : : "i" (offset + 80)); \
	asm("LDP x12, x13, [" #reg ", %0]" : : "i" (offset + 96)); \
	asm("LDP x14, x15, [" #reg ", %0]" : : "i" (offset + 112)); \
	asm("LDP x16, x17, [" #reg ", %0]" : : "i" (offset + 128))

// savedRegisters should point to the *start* of a Thread::savedRegisters
#define SAVE_CALLEE_PRESERVED_REGISTERS_TO_THREAD(savedRegisters) \
	SAVE_CALLEE_PRESERVED_REGISTERS(savedRegisters, 19*8)

#define LOAD_CALLEE_PRESERVED_REGISTERS_FROM_THREAD(savedRegisters) \
	LOAD_CALLEE_PRESERVED_REGISTERS(savedRegisters, 19*8)

#define EL1_SAVE_ALL_REGISTERS(regs, scratch) \
	SAVE_TEMP_REGISTERS(regs, 0); \
	SAVE_CALLEE_PRESERVED_REGISTERS(regs, 19*8); \
	asm("MRS " #scratch ", ELR_EL1"); \
	asm("STR " #scratch ", [" #regs ", %0]" : : "i" (KSavedPc * sizeof(uintptr))); \
	asm("MRS " #scratch ", SPSR_EL1"); \
	asm("STR " #scratch ", [" #regs ", %0]" : : "i" (KSavedSpsr * sizeof(uintptr)))

// sp is assumed to be pointing at savedRegisters
#define LOAD_ALL_REGISTERS() \
	LOAD_TEMP_REGISTERS(sp, 0); \
	LOAD_CALLEE_PRESERVED_REGISTERS(sp, 19*8)

#define RESET_SP_EL1_AND_ERET() \
	asm("MOV x30, %0" : : "i" (KSectionZero)); \
	asm("ADD x30, x30, %0" : : "i" ((KKernelStackBase + KKernelStackSize) - KSectionZero)); \
	asm("MOV sp, x30"); \
	asm("ERET")

// Exception Syndrome Register, Exception Class bits
#define ESR_EC_MASK	(63u << 26)
#define ESR_EC_SVC	(21u << 26)

#define SPSR_D		BIT(9)
#define SPSR_A		BIT(8)
#define SPSR_I		BIT(7)
#define SPSR_F		BIT(6)
#define SPSR_EL1t	(4) // Return to EL1 but with sp = SP_EL0
#define SPSR_EL2h	(9)

// Secure Configuration Register
#define SCR_EL3_NS	BIT(0)	// Non-secure EL0&EL1
#define SCR_EL3_SMD	BIT(7)	// Secure Monitor Disable
#define SCR_EL3_RW	BIT(10)	// Aarch64 all the way

#endif // LUPI_AARCH64_H
