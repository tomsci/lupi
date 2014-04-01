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

#define IRQ_BASIC (KPeripheralBase + 0xB200)
#define IRQ_PEND1 (KPeripheralBase + 0xB204)
#define IRQ_PEND2 (KPeripheralBase + 0xB208)
#define IRQ_FIQ_CONTROL (KPeripheralBase + 0xB210)
#define IRQ_ENABLE_BASIC (KPeripheralBase + 0xB218)
#define IRQ_DISABLE_BASIC (KPeripheralBase + 0xB224)

uint32 GET32(uint32 addr);
void PUT32(uint32 addr, uint32 val);

//#define KGpioFunctionSelectPinMask (7)
//#define KGpioFunctionSelectOutput (1)
//#define PIN_SHIFT(n) ((n)*3) // n is relative to base of the relevant register (ie in function select register 1, to set pin 19 you'd shift by PIN_SHIFT(9)
//#define SetGpioFunctionForPin(reg, pin, val) reg = (((reg) & ~(KGpioFunctionSelectPinMask << PIN_SHIFT(pin))) | (((val) & KGpioFunctionSelectPinMask) << PIN_SHIFT(pin)))

#define KTimerControlReset 0x003E0020 // Prescale = 3E, InterruptEnable=1
#define KTimerControl23BitCounter (1<<1)
#define KTimerControlInterruptEnable (1<<5)
#define KTimerControlTimerEnable (1<<7)


/*
This will setup the system timer and start it running and producing interrupts
(which won't get delivered until they are enabled in CPSR)
*/

void irq_init() {
//	unsigned int ra;

	PUT32(IRQ_DISABLE_BASIC,1);

//	ra=GET32(GPFSEL1);
//	SetGpioFunctionForPin(ra, 6, KGpioFunctionSelectOutput); // Pin 16 in fn select register 1
//	PUT32(GPFSEL1,ra);
//
//	PUT32(GPSET0,1<<16); // Sets pin 16

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

void NAKED irq() {
	asm("push {r0-r12, lr}");
	printk("IRQ!\n");
	PUT32(ARM_TIMER_CLI,0);
	asm("pop  {r0-r12, lr}");
	asm("SUBS pc, r14, #4");
}
