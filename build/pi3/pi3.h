#ifndef LUPI_BUILD_PI3_H
#define LUPI_BUILD_PI3_H

#define AARCH64 // Architecture, cf ARMV6 and ARMV7-M
#define A64 // Instruction set, cf ARM and THUMB2
#define BCM2837

#define ARCH_HEADER <aarch64.h>

// #define HAVE_SCREEN
// #define HAVE_PITFT
// #define HAVE_MMU
#define IDENTITY_MMU
#define WORKING_LDREX
#define INTERRUPTS_OFF
#define LUPI_NO_IPC

#define KPeripheralPhys			0x3F000000ul
#define KPeripheralSize			0x00300000ul

#define KPhysicalRamBase		0x00000000ul
#define KPhysicalStackBase		0x00081000ul
#define KPhysPageAllocator		0x000C0000ul
#define KPhysicalPdeBase		0x00100000ul

#ifdef KLUA
#define KLuaHeapBase			0x00201000ul
#endif

#define KKernelCodesize			0x00080000ul

#if defined(HAVE_MMU) && !defined(IDENTITY_MMU)
// TODO finish these
#define KSectionZero			0xFFFFFFFFFFF00000ul
#define KKernelCodeBase			0xFFFFFFFFFFF00000ul
#define KSuperPageAddress		0xFFFFFFFFFFF80000ul
#define KPeripheralBase			0xFFFFFFFFFFC00000ul
#define KPageAllocatorAddr		0xFFFFFFFFFFFC0000ul
#define KKernelStackBase		0xFFFFFFFFFFF81000ul
#else
#define KSectionZero			0
#define KPeripheralBase			KPeripheralPhys
#define KSuperPageAddress		0x00080000ul
#define KKernelCodeBase			KPhysicalRamBase
#define KPageAllocatorAddr		KPhysPageAllocator
#define KKernelStackBase		KPhysicalStackBase
#endif

#define KKernelStackSize		0x00001000ul // 4kB


#define KPageAllocatorMaxSize	0x00040000ul // 256KB, enough for 1GB RAM

// Code 0-512KB
// Superpage 512-516
// Kern stack 516-520
// Stuff
// Page Allocator 768k-1MB
// Identity mappings 1MB-3MB

// TODO FIX THESE
#define KProcessesSection		0xF8100000ul

#define LoadSuperPageAddress(reg) asm("MOV " #reg ", %0" : : "i" (KSuperPageAddress))


// See BCM-2835-ARM-Peripherals p8
#define AUX_ENABLES		(KPeripheralBase + 0x00215004)
#define AUX_MU_IO_REG	(KPeripheralBase + 0x00215040)
#define AUX_MU_IER_REG	(KPeripheralBase + 0x00215044)
#define AUX_MU_IIR_REG	(KPeripheralBase + 0x00215048)
#define AUX_MU_LCR_REG	(KPeripheralBase + 0x0021504C)
#define AUX_MU_MCR_REG	(KPeripheralBase + 0x00215050)
#define AUX_MU_LSR_REG	(KPeripheralBase + 0x00215054)
#define AUX_MU_MSR_REG	(KPeripheralBase + 0x00215058)
#define AUX_MU_SCRATCH	(KPeripheralBase + 0x0021505C)
#define AUX_MU_CNTL_REG	(KPeripheralBase + 0x00215060)
#define AUX_MU_STAT_REG	(KPeripheralBase + 0x00215064)
#define AUX_MU_BAUD_REG	(KPeripheralBase + 0x00215068)


// See BCM-2835-ARM-Peripherals p112
#define IRQ_BASIC			(KPeripheralBase + 0xB200)
#define IRQ_PEND1			(KPeripheralBase + 0xB204)
#define IRQ_PEND2			(KPeripheralBase + 0xB208)
#define IRQ_FIQ_CONTROL		(KPeripheralBase + 0xB20C)
#define IRQ_ENABLE_1		(KPeripheralBase + 0xB210)
#define IRQ_ENABLE_2		(KPeripheralBase + 0xB214)
#define IRQ_ENABLE_BASIC	(KPeripheralBase + 0xB218)
#define IRQ_DISABLE_1		(KPeripheralBase + 0xB21C)
#define IRQ_DISABLE_2		(KPeripheralBase + 0xB220)
#define IRQ_DISABLE_BASIC	(KPeripheralBase + 0xB224)

#define AUX_INT				29
#define GPIO0_INT			49
#define GPIO1_INT			50
#define GPIO2_INT			51
#define GPIO3_INT			52
#define UART_INT			57

// BCM-2835-ARM-Peripherals p12 says you set bit 1 to enable receive. It's actually bit 0.
#define AUX_MU_EnableReceiveInterrupt	(1 << 0)
#define AUX_MU_ClearReceiveFIFO			(1 << 1)
#define AUX_MU_ClearTransmitFIFO		(1 << 2)
#define AUX_MU_IIR_ReceiveInterrupt		(1 << 2)

#define AUXENB_MiniUartEnable			1
#define AUXENB_Spi1Enable				2
#define AUXENB_Spi2Enable				4

#define KSystemClockFreq	250000000 // 250 MHz

// #include <memmap.h>

//// User memmap stuff ////

#define KUserBss				0x00007000ul
// Heap assumed to be immediately following BSS in process_init()
#define KUserHeapBase			0x00008000ul
// Note these next two are also defined in usersrc/ipc.c
#define KSharedPagesBase		0x0F000000ul
#define KSharedPagesSize		0x00100000ul
#define KUserStacksBase			0x0FE00000ul
#define KUserMemLimit			0x10000000ul

/**
I'm feeling generous.
*/
#define USER_STACK_SIZE (16*1024)

#define USER_STACK_AREA_SHIFT 15 // 32kB

/**
The format of each user stack area is as follows. Note the svc stack for a
thread is always 4kB, and that the area is rounded up to a power of 2 to make
calculating the svc stack address simpler.

	svc stack			1 page
	guard page			---------------
	user stack			USER_STACK_SIZE (16kB)
	guard page			---------------
	padding				--------------- (4kB)
*/

#endif // LUPI_BUILD_PI3_H
