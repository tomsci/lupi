#include <k.h>
#include <armv7-m.h>
#include <exec.h>

void spi_init();
void flash_init();
static void setPeripheralInterruptPriority(int peripheralId, uint8 priority);
static void configureButtons(uint32 pio, uint32 mask, uint32 peripheralId);
static void buttonEvent(InputButton but, uint32 pressed);
static DRIVER_FN(inputHandleSvc);

#define BUTTON_A		(1 << 15) // PC15, "48"
#define BUTTON_B		(1 << 16) // PC16, "47"

#define BUTTON_UP		(1 << 2) // PD2, "27"
#define BUTTON_DOWN		(1 << 1) // PD1, "26"
#define BUTTON_LEFT		(1 << 3) // PD3, "28"
#define BUTTON_RIGHT	(1 << 0) // PD0, "25"
#define BUTTON_CENTRE	(1 << 15) // PA15, "24"

#define BUTTON_LIGHT	(1 << 3) // PA3, "60"

#define A_BUTTONS		(BUTTON_CENTRE | BUTTON_LIGHT)
#define C_BUTTONS		(BUTTON_A | BUTTON_B)
#define D_BUTTONS		(BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT)

// #define AUDIO_CLIP_TEST
// #define AUDIO_FLASH_TEST

#if defined(AUDIO_CLIP_TEST) || defined(AUDIO_FLASH_TEST)
static void audioTest();
#endif

void board_init() {
	// Enable more specific fault handlers
	PUT32(SCB_SHCSR, SHCSR_USGFAULTENA | SHCSR_BUSFAULTENA | SHCSR_MEMFAULTENA);

	// Highest priority - sys tick (0x4).
	PUT8(SHPR_SYSTICK, KPrioritySysTick);

	// Next highest, peripheral interrupts (0x8)
	setPeripheralInterruptPriority(PERIPHERAL_ID_USART0, KPriorityPeripheral);
	setPeripheralInterruptPriority(PERIPHERAL_ID_PIOA, KPriorityPeripheral);
	setPeripheralInterruptPriority(PERIPHERAL_ID_PIOC, KPriorityPeripheral);
	setPeripheralInterruptPriority(PERIPHERAL_ID_PIOD, KPriorityPeripheral);

	// Lowest, SVC and pendSV 0xA
	PUT8(SHPR_SVCALL, KPrioritySvc);
	PUT8(SHPR_PENDSV, KPrioritySvc);

	// Spec suggests this reg contains 10ms but it's definitely only 1ms
	uint32 oneMsInSysTicks = GET32(SYSTICK_CALIB) & 0x00FFFFFF;
	PUT32(SYSTICK_LOAD, oneMsInSysTicks); // 1ms ticks
	//PUT32(SYSTICK_LOAD, oneMsInSysTicks * 1000); // 1s ticks
	// Not setting SYSTICK_CTRL_CLKSOURCE means systick is running at MCLK/8
	PUT32(SYSTICK_CTRL, SYSTICK_CTRL_ENABLE | SYSTICK_CTRL_TICKINT);

	configureButtons(PIOA, A_BUTTONS, PERIPHERAL_ID_PIOA);
	configureButtons(PIOC, C_BUTTONS, PERIPHERAL_ID_PIOC);
	configureButtons(PIOD, D_BUTTONS, PERIPHERAL_ID_PIOD);

	kern_enableInterrupts();
	spi_init();
	flash_init();

	// Piezo buzzer is connected to DAC0
	PUT32(PMC_PCER1, 1 << (PERIPHERAL_ID_DACC - 32));

#if defined(AUDIO_CLIP_TEST) || defined(AUDIO_FLASH_TEST)
	audioTest();
#endif

	kern_registerDriver(FOURCC("INPT"), inputHandleSvc);
}

static void setPeripheralInterruptPriority(int peripheralId, uint8 priority) {
	ASSERT((priority & 0xF) == 0);
	uint32 addr = NVIC_IPR0 + peripheralId;
	PUT32(addr, priority);
}

static void configureButtons(uint32 pio, uint32 mask, uint32 peripheralId) {
	PUT32(PMC_PCER0, 1 << peripheralId); // Enable clock
	const int debounce_usecs = 1000; // 1ms
	PUT32(pio + PIO_SCDR, (debounce_usecs / 31) - 1); // apparently

	PUT32(pio + PIO_ODR, mask); // Output disabled (ie they're inputs)
	PUT32(pio + PIO_ENABLE, mask); // PIO enabled
	PUT32(pio + PIO_PUER, mask); // Pull-up enable
	PUT32(pio + PIO_IFER, mask); // Input filter enable
	PUT32(pio + PIO_DIFSR, mask); // Use Debounce as input filter
	PUT32(pio + PIO_IER, mask); // Enable interrupts

	// This starts out with junk in for some reason
	GET32(pio + PIO_ISR);
	PUT32(NVIC_ICPR0, 1 << peripheralId); // Clear pending before enabling
	PUT32(NVIC_ISER0, 1 << peripheralId);
}

void pioaInterrupt() {
	uint32 changed = GET32(PIOA + PIO_ISR);
	uint32 values = GET32(PIOA + PIO_PDSR);
	if (changed & BUTTON_CENTRE) buttonEvent(InputButtonSelect, values & BUTTON_CENTRE);
	if (changed & BUTTON_LIGHT) buttonEvent(InputButtonLight, values & BUTTON_LIGHT);
}

void piocInterrupt() {
	uint32 changed = GET32(PIOC + PIO_ISR);
	uint32 values = GET32(PIOC + PIO_PDSR);
	if (changed & BUTTON_A) buttonEvent(InputButtonA, values & BUTTON_A);
	if (changed & BUTTON_B) buttonEvent(InputButtonB, values & BUTTON_B);
}

void piodInterrupt() {
	uint32 changed = GET32(PIOD + PIO_ISR);
	uint32 values = GET32(PIOD + PIO_PDSR);
	if (changed & BUTTON_UP) buttonEvent(InputButtonUp, values & BUTTON_UP);
	if (changed & BUTTON_DOWN) buttonEvent(InputButtonDown, values & BUTTON_DOWN);
	if (changed & BUTTON_LEFT) buttonEvent(InputButtonLeft, values & BUTTON_LEFT);
	if (changed & BUTTON_RIGHT) buttonEvent(InputButtonRight, values & BUTTON_RIGHT);
}

static void completeInputRequest_dfc(uintptr arg1, uintptr arg2, uintptr arg3) {
	int numSamples = atomic_set(&TheSuperPage->inputRequestBufferPos, 0);
	if (numSamples > TheSuperPage->inputRequestBufferSize) {
		numSamples = TheSuperPage->inputRequestBufferSize;
	}
	// printk("completeInputRequest_dfc completing %d samples\n", numSamples);
	thread_requestComplete(&TheSuperPage->inputRequest, numSamples);
}

static void buttonEvent(InputButton but, uint32 high) {
	const uint32 pressed = !high;
	// printk("Button %d %s\n", but, pressed ? "down" : "up");
	SuperPage* sp = TheSuperPage;
	if (pressed) sp->buttonStates |= (1 << but);
	else sp->buttonStates &= ~(1 << but);

	if (sp->inputRequest.userPtr == 0) return; // Nothing else we can do
	int pos = atomic_inc(&sp->inputRequestBufferPos) - 1;
	// printk("pos = %d inputRequestBufferSize = %d\n", pos, sp->inputRequestBufferSize);
	if (pos >= sp->inputRequestBufferSize) return; // TMI
	// We wouldn't attempt to do this inside a Handler if not for the fact that
	// there is no penalty writing to user mem on a platform with no MMU
	uint32* buf = (uint32*)sp->inputRequestBuffer;
	buf[pos*3] = InputButtons;
	buf[pos*3 + 1] = sp->buttonStates;
	buf[pos*3 + 2] = GET32((uintptr)&sp->uptime); // Yay for LSB-only coding

	if (pos == 0) {
		// We need to queue a completion
		// printk("Queuing completeInputRequest_dfc\n");
		dfc_queue(completeInputRequest_dfc, 0, 0, 0);
	}
}

static DRIVER_FN(inputHandleSvc) {
	ASSERT(arg1 == KExecDriverInputRequest, arg1);
	int err = kern_setInputRequest(arg2);
	if (err) return err;

	return TheSuperPage->buttonStates;
}


#if defined(AUDIO_CLIP_TEST) || defined(AUDIO_FLASH_TEST)

void nanowait(int ns) {
	// ns must be less than 10000000, ie one full SysTick period
	// only accurate to the speed of SysTick, MCK/8 ie +/- 95ns
	int systicks = (ns * 105) / 10000;
	// printk("Waiting %d systicks\n", systicks);
	int current = GET32(SYSTICK_VAL);
	int target = current - systicks;
	if (target < 0) {
		// Need to wait for wrap around
		target = target + 0x2904;
		volatile uint64* uptimePtr = &TheSuperPage->uptime;
		uint64 curUp = *uptimePtr;
		while (*uptimePtr == curUp) {
			WFI();
		}
	}
	while (GET32(SYSTICK_VAL) > target) { /* Spin */ }
}

#endif // audio test common

#ifdef AUDIO_CLIP_TEST

#include "../../modules/tetris/tetrisclip.c"

static void audioTest() {
	PUT32(DACC_CHER, 1 << 0); // Enable channel 0
	for (int i = 0; i < sizeof(audio); i++) {
		// Source is 8 bit and the DACC takes 12-bit input, so multiply up
		PUT32(DACC_CDR, ((uint32)audio[i]) << 4);
		// 45us is the time between samples for 22kHz audio
		nanowait(45000);
	}
}

#endif // AUDIO_CLIP_TEST

#ifdef AUDIO_FLASH_TEST

#include "pio.h"
#define FLASH_CHIPSELECT (SPI0 + SPI_CSR0)
#define READ_DATA		0x03

// Stream from flash
static void audioTest() {
	kern_sleep(5);
	PUT32(DACC_MR, 1 << 8); // Set REFRESH to something like 20us
	PUT32(DACC_CHER, 1 << 0); // Enable channel 0
	spi_beginTransaction(FLASH_CHIPSELECT);
	uint8 cmd[] = { READ_DATA, 0, 0, 0 };
	spi_readwrite_poll(cmd, sizeof(cmd), 0);
	const int musicLen = 1707228; // length of tetrisa.pcm
	for (int i = 0; i < musicLen; i++) {
		// And just read the data byte by byte from SPI. In theory the flash bus is so much faster
		// than the audio sample rate (42MHz vs 22kHz) that the flash access time should be
		// inconsequential. In practice, it isn't.
		uint8 data = 0;
		spi_readwrite_poll(&data, 1, KSpiFlagWriteback | (i+1 == musicLen ? KSpiFlagLastXfer : 0));
		PUT32(DACC_CDR, ((uint32)data) << 3);
		// 45us is the time between samples for 22kHz audio
		// 20us seems approximately right for the
		nanowait(20000);
	}
}


#endif // AUDIO_FLASH_TEST
