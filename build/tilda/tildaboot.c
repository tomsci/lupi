#include <k.h>
#include <atags.h>
#include <armv7-m.h>

// Sets up exception vectors, configures NVIC and MPU, then calls Boot()
void NAKED _start() {
	// First thing we have is the exception vector table. The CPU automatically
	// loads the stack pointer and jumps to the address given by the reset vector
	// See p83
	WORD(KHandlerStackBase + KPageSize); // SP_main
	asm(".word tildaBoot"); // 1 = reset
	asm(".word unhandledException"); // 2 = reserved/NMI
	asm(".word unhandledException"); // 3 = Hard fault
	asm(".word unhandledException"); // 4 = MPU fault
	asm(".word unhandledException"); // 5 = Bus fault
	asm(".word unhandledException"); // 6 = Usage fault
	asm(".word 0"); // 7 = Reserved
	asm(".word 0"); // 8 = Reserved
	asm(".word 0"); // 9 = Reserved
	asm(".word 0"); // 10 = Reserved
	asm(".word svc"); // 11 = SVC
	asm(".word unhandledException"); // 12 = Reserved
	asm(".word 0"); // 13 = Reserved
	asm(".word pendSV"); // 14 = PendSV
	asm(".word sysTick"); // 15 = Sys tick
	asm(".word unhandledException"); // 16 = IRQ 0
	asm(".word unhandledException"); // 17 = IRQ 1
	asm(".word unhandledException"); // 18 = IRQ 2
	asm(".word unhandledException"); // 19 = IRQ 3
	asm(".word unhandledException"); // 20 = IRQ 4
	asm(".word unhandledException"); // 21 = IRQ 5
	asm(".word unhandledException"); // 22 = IRQ 6
	asm(".word unhandledException"); // 23 = IRQ 7
	asm(".word unhandledException"); // 24 = IRQ 8
	asm(".word unhandledException"); // 25 = IRQ 9
	asm(".word unhandledException"); // 26 = IRQ 10
	asm(".word unhandledException"); // 27 = IRQ 11
	asm(".word unhandledException"); // 28 = IRQ 12
	asm(".word unhandledException"); // 29 = IRQ 13
	asm(".word unhandledException"); // 30 = IRQ 14
	asm(".word unhandledException"); // 31 = IRQ 15
	asm(".word unhandledException"); // 32 = IRQ 16
	asm(".word usart0Interrupt"); // 33 = IRQ 17 (PERIPHERAL_ID_USART0)
	asm(".word unhandledException"); // 34 = IRQ 18
	asm(".word unhandledException"); // 35 = IRQ 19
	asm(".word unhandledException"); // 36 = IRQ 20
	asm(".word unhandledException"); // 37 = IRQ 21
	asm(".word unhandledException"); // 38 = IRQ 22
	asm(".word unhandledException"); // 39 = IRQ 23
	asm(".word unhandledException"); // 40 = IRQ 24
	asm(".word unhandledException"); // 41 = IRQ 25
	asm(".word unhandledException"); // 42 = IRQ 26
	asm(".word unhandledException"); // 43 = IRQ 27
	asm(".word unhandledException"); // 44 = IRQ 28
	asm(".word unhandledException"); // 45 = IRQ 29
}

#define CKGR_MOR	0x400E0620 // Main oscillator register
#define CKGR_PLLAR	0x400E0628 // PMC Clock Generator PLLA Register
#define EEFC0_FMR	0x400E0A00
#define EEFC1_FMR	0x400E0C00
#define WDT_MR		0x400E1A54

// p565
#define CKGR_MOR_KEY		(0x37 << 16)
#define CKGR_MOR_MOSCSEL	(1 << 24)
#define CKGR_MOR_MOSCRCEN	(1 << 3)
#define CKGR_MOR_MOSCXTEN	(1 << 0)

// p 575
#define PMC_SR_MOSCXTS	(1 << 0)
#define PMC_SR_MOSCSELS (1 << 16)
#define PMC_SR_MCKRDY	(1 << 3)
#define PMC_SR_LOCKA	(1 << 1)

// p569
#define PMC_MCKR_CSS_MAIN_CLK (1)
#define PMC_MCKR_CSS_PLLA_CLK (2)
#define PMC_MCKR_PRES_CLK_2 (1 << 4)

// Star Wars settings
#define TILDA_FWS 4
#define TILDA_MOSCXTST 8

#define TILDA_MOR (CKGR_MOR_KEY | (TILDA_MOSCXTST << 8) | CKGR_MOR_MOSCRCEN | CKGR_MOR_MOSCXTEN)
#define TILDA_PLLAR	((1 << 29) /* Bit 29 must be set */ \
					| (0xD << 16) /* MULA, PLLA Multiplier */ \
					| (0x3F << 8) /* PLLA Count */ \
					| (0x1) /* Divider bypassed */ )

void PUT8(uint32 addr, byte val);
void dummy();
#define WaitForPMC(bit) do { dummy(); } while (!(GET32(PMC_SR) & (bit)))

// For boot debugging
#define SET_TX() PUT32(PIOA + PIO_SODR, LED_TX)
#define CLR_TX() PUT32(PIOA + PIO_CODR, LED_TX)
#define SET_RX() PUT32(PIOC + PIO_SODR, LED_RX)
#define CLR_RX() PUT32(PIOC + PIO_CODR, LED_RX)

void Boot(uintptr atagsPhysAddr);

void tildaBoot() {
	// Reset begins in priviledged Thread mode. (p80 §12.6.2.1)

	// Configure the RX and TX LEDs
	PUT32(PIOA + PIO_PER, LED_TX);
	PUT32(PIOA + PIO_OER, LED_TX);
	PUT32(PIOA + PIO_CODR, LED_TX);
	PUT32(PIOC + PIO_PER, LED_RX);
	PUT32(PIOC + PIO_OER, LED_RX);
	PUT32(PIOC + PIO_CODR, LED_RX);

	// Configure flash access
	PUT32(EEFC0_FMR, TILDA_FWS << 8);
	PUT32(EEFC1_FMR, TILDA_FWS << 8);

	// See p549 §29.2.12 Programming sequence

	// Configure main oscillator
	if (!(GET32(CKGR_MOR) & CKGR_MOR_MOSCSEL)) {
		PUT32(CKGR_MOR, TILDA_MOR);
		// And wait until oscillator stabilised
		WaitForPMC(PMC_SR_MOSCXTS);
	}

	// Switch oscilllator to 3-20MHz crystal
	PUT32(CKGR_MOR, TILDA_MOR | CKGR_MOR_MOSCSEL);
	// And wait for done
	WaitForPMC(PMC_SR_MOSCSELS);

	// Init PLLA
	PUT32(CKGR_PLLAR, TILDA_PLLAR);
	WaitForPMC(PMC_SR_LOCKA);

	// Set clock source to main clock
	PUT32(PMC_MCKR, PMC_MCKR_PRES_CLK_2 | PMC_MCKR_CSS_MAIN_CLK);
	WaitForPMC(PMC_SR_MCKRDY);

	// Now switch to PLLA
	PUT32(PMC_MCKR, PMC_MCKR_PRES_CLK_2 | PMC_MCKR_CSS_PLLA_CLK);
	WaitForPMC(PMC_SR_MCKRDY);

	// Disable watchdog
	PUT32(WDT_MR, 1 << 15); // WDDIS

	// Set VTOR so we can unmap address zero
	PUT32(SCB_VTOR, KKernelCodeBase);

	Boot(0);
}

void parseAtags(uint32* ptr, AtagsParams* params) {
	// We should look up what spec ATSAM we are, probably
	params->totalRam = KRamSize;
	params->boardRev = 0xE; // MkE
}

static void setPeripheralInterruptPriority(int peripheralId, uint8 priority) {
	ASSERT((priority & 0xF) == 0);
	uint32 addr = NVIC_IPR0 + peripheralId;
	PUT32(addr, priority);
}

void irq_init() {
	// Enable more specific fault handlers
	PUT32(SCB_SHCSR, SHCSR_USGFAULTENA | SHCSR_BUSFAULTENA | SHCSR_MEMFAULTENA);

	// Highest priority - sys tick (0x4).
	PUT8(SHPR_SYSTICK, KPrioritySysTick);

	// Next highest, peripheral interrupts (0x8)
	setPeripheralInterruptPriority(PERIPHERAL_ID_USART0, KPriorityPeripheral);

	// Lowest, SVC and pendSV 0xA
	PUT8(SHPR_SVCALL, KPrioritySvc);
	PUT8(SHPR_PENDSV, KPrioritySvc);

	// Spec suggests this reg contains 10ms but it's definitely only 1ms
	uint32 oneMsInSysTicks = GET32(SYSTICK_CALIB) & 0x00FFFFFF;
	PUT32(SYSTICK_LOAD, oneMsInSysTicks); // 1ms ticks
	//PUT32(SYSTICK_LOAD, oneMsInSysTicks * 1000); // 1s ticks
	// Not setting SYSTICK_CTRL_CLKSOURCE means systick is running at MCLK/8
	PUT32(SYSTICK_CTRL, SYSTICK_CTRL_ENABLE | SYSTICK_CTRL_TICKINT);
}

NORETURN reboot() {
	PUT32(SCB_AIRCR, AIRCR_VECTKEY | AIRCR_SYSRESETREQ);
	hang();
}
