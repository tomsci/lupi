#include <k.h>
#include <arm.h>
#include "gpio.h"

//#define SLOW_TIME

// See BCM-2835-ARM-Peripherals p196
#define ARM_TIMER_LOD (KPeripheralBase + 0xB400)
#define ARM_TIMER_VAL (KPeripheralBase + 0xB404)
#define ARM_TIMER_CTL (KPeripheralBase + 0xB408)
#define ARM_TIMER_CLI (KPeripheralBase + 0xB40C)
#define ARM_TIMER_RIS (KPeripheralBase + 0xB410)
#define ARM_TIMER_MIS (KPeripheralBase + 0xB414)
#define ARM_TIMER_RLD (KPeripheralBase + 0xB418)
#define ARM_TIMER_DIV (KPeripheralBase + 0xB41C)
#define ARM_TIMER_CNT (KPeripheralBase + 0xB420)

//#define SYSTIMERCLO (KPeripheralBase + 0x3004)

bool tick();
void uart_got_char(byte b);
void tft_gpioHandleInterrupt();

#define KTimerControlReset 0x00F90020 // Prescale = 0xF9=249, InterruptEnable=1
#define KTimerControl23BitCounter (1<<1)
#define KTimerControlInterruptEnable (1<<5)
#define KTimerControlTimerEnable (1<<7)
#define KTimerControlFreeRunningTimerEnable (1<<9)

/*
This will setup the system timer and start it running and producing interrupts
(which won't get delivered until they are enabled in CPSR, eg by calling irq_enable())
*/

void irq_init() {
	PUT32(IRQ_DISABLE_BASIC,1);

	PUT32(ARM_TIMER_CTL,KTimerControlReset & ~KTimerControlInterruptEnable); // disable interrupt
#ifdef SLOW_TIME
	// One tick per second
	PUT32(ARM_TIMER_LOD,1000000-1);
	PUT32(ARM_TIMER_RLD,1000000-1);
#else
	// Tick every 1ms
	PUT32(ARM_TIMER_LOD,1000-1);
	PUT32(ARM_TIMER_RLD,1000-1);
#endif

	PUT32(ARM_TIMER_DIV,0x000000F9); // F9=249 means the 250MHz system timer will be divided down to 1MHz
	PUT32(ARM_TIMER_CLI,0);
	PUT32(ARM_TIMER_CTL, KTimerControlReset | KTimerControlTimerEnable | KTimerControl23BitCounter | KTimerControlFreeRunningTimerEnable);

//	PUT32(ARM_TIMER_LOD,2000000-1);
//	PUT32(ARM_TIMER_RLD,2000000-1);
	PUT32(ARM_TIMER_CLI,0);
	PUT32(IRQ_ENABLE_BASIC,1);
//	PUT32(ARM_TIMER_LOD,4000000-1);
//	PUT32(ARM_TIMER_RLD,4000000-1);
	}

bool handleIrq(void* savedRegs) {
	//printk("IRQ!\n");
	bool threadTimeExpired = false;
	uint32 irqBasicPending = GET32(IRQ_BASIC);
	if (irqBasicPending & 1) {
		// Timer IRQ
		threadTimeExpired = tick();
		PUT32(ARM_TIMER_CLI,0);
	}
	if (irqBasicPending & (1 << 8)) {
		// IRQ Pending Reg 1
		uint32 pending1 = GET32(IRQ_PEND1);
		if (pending1 & (1 << AUX_INT)) {
			// We don't enable SPI interrupts so we don't need to bother checking AUXIRQ
			uint32 iir = GET32(AUX_MU_IIR_REG);
			if (iir & AUX_MU_IIR_ReceiveInterrupt) {
				//printk("CNT=%d\n", GET32(ARM_TIMER_CNT));
				//printk("Got char %c!\n", GET32(AUX_MU_IO_REG));
				byte b = GET32(AUX_MU_IO_REG);
				uart_got_char(b);
				PUT32(AUX_MU_IIR_REG, AUX_MU_ClearReceiveFIFO);
			}
		}
	}
	if (irqBasicPending & (1 << 9)) {
		// IRQ Pending Reg 2
		uint32 pending2 = GET32(IRQ_PEND2);
		//printk("IRQ pending2: %X\n", pending2);
		if (pending2 & (1 << (GPIO0_INT - 32))) {
			uint32 gpioInts = GET32(GPEDS0);
			if (gpioInts & (1<<24)) {
				tft_gpioHandleInterrupt();
			}
		}
	}
	bool dfcsPending = irq_checkDfcs();
	if (threadTimeExpired || dfcsPending) {
		uint32 spsr;
		GetSpsr(spsr);
		bool threadWasInSvc = (spsr & KPsrModeMask) == KPsrModeSvc;
		if (threadWasInSvc) {
			atomic_setbool(&TheSuperPage->rescheduleNeededOnSvcExit, true);
			// Don't reschedule immediately
		} else {
			saveCurrentRegistersForThread(savedRegs);
			return true; // Reschedule right now
		}
	}
	return false; // Normally, just return from the IRQ
}

void NAKED irq() {
	asm("PUSH {r0-r12, r14}");
	asm("MOV r0, sp"); // Full descending stack means sp now points to the regs we saved
	asm("BL handleIrq");
	asm("CMP r0, #0"); // Has thread timeslice expired?
	asm("BLNE reschedule_irq"); // If so, handleIrq() will have saved thread state, so just reschedule()
	// Otherwise, return to thread
	asm("POP  {r0-r12, r14}");
	asm("SUBS pc, r14, #4");
}
