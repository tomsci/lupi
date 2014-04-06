#include <k.h>
#include <arm.h>

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

uint32 GET32(uint32 addr);
void PUT32(uint32 addr, uint32 val);

#define KTimerControlReset 0x003E0020 // Prescale = 3E, InterruptEnable=1
#define KTimerControl23BitCounter (1<<1)
#define KTimerControlInterruptEnable (1<<5)
#define KTimerControlTimerEnable (1<<7)


/*
This will setup the system timer and start it running and producing interrupts
(which won't get delivered until they are enabled in CPSR, eg by calling irq_enable())
*/

void irq_init() {
	PUT32(IRQ_DISABLE_BASIC,1);

	PUT32(ARM_TIMER_CTL,KTimerControlReset & ~KTimerControlInterruptEnable); // disable interrupt
	PUT32(ARM_TIMER_LOD,1000000-1);
	PUT32(ARM_TIMER_RLD,1000000-1);
	PUT32(ARM_TIMER_DIV,0x000000F9);
	PUT32(ARM_TIMER_CLI,0);
	PUT32(ARM_TIMER_CTL, KTimerControlReset | KTimerControlTimerEnable | KTimerControl23BitCounter);

//	PUT32(ARM_TIMER_LOD,2000000-1);
//	PUT32(ARM_TIMER_RLD,2000000-1);
	PUT32(ARM_TIMER_CLI,0);
	PUT32(IRQ_ENABLE_BASIC,1);
//	PUT32(ARM_TIMER_LOD,4000000-1);
//	PUT32(ARM_TIMER_RLD,4000000-1);
	}

void NAKED irq_enable() {
	asm("MRS r0,cpsr");
	asm("bic r0, r0, %0" : : "i" (KPsrIrqDisable));
	asm("MSR cpsr_c, r0");
	asm("bx lr");
}

void handleIrq() {
	uint32 irqBasicPending = GET32(IRQ_BASIC);
	if (irqBasicPending & 1) {
		// Timer IRQ
		//TODO tick();
		PUT32(ARM_TIMER_CLI,0);
	}
	if (irqBasicPending & (1 << 8)) {
		// IRQ Pending Reg 1
		uint32 pending1 = GET32(IRQ_PEND1);
		if (pending1 & (1 << AUX_INT)) {
			// We don't enable SPI interrupts so we don't need to bother checking AUXIRQ
			uint32 iir = GET32(AUX_MU_IIR_REG);
			if (iir & AUX_MU_IIR_ReceiveInterrupt) {
				//TODO printk("Got char %c!\n", GET32(AUX_MU_IO_REG));
				PUT32(AUX_MU_IIR_REG, AUX_MU_ClearReceiveFIFO);
			}
		}
	}
	if (irqBasicPending & (1 << 9)) {
		// IRQ Pending Reg 2
		// TODO
	}
}

void NAKED irq() {
	asm("push {r0-r12, lr}");
	handleIrq();
	asm("pop  {r0-r12, lr}");
	asm("SUBS pc, r14, #4");
}
