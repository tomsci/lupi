#include <k.h>
#include <armv7-m.h>
#include <exec.h>
#include <err.h>
#include "pio.h"

// p1373
#define DACC_MR		0x400C8004
#define DACC_CHER	0x400C8010 // DACC Channel Enable Register
#define DACC_CHDR	0x400C8014 // DACC Channel Disable Register
#define DACC_CDR	0x400C8020 // DACC Conversion Data Register
#define DACC_IER	0x400C8024 // DACC Interrupt Enable Register
#define DACC_IDR	0x400C8028 // DACC Interrupt Disable Register
#define DACC_ISR	0x400C8030 // DACC Status register

// ISR, IMR, IER, IDR
#define DACC_TXRDY		(1 << 0)
#define DACC_TXBUFE		(1 << 3)

#define TC_CCR0			0x40080000 // TC Channel Control Register
#define TC_CMR0			0x40080004
#define TC_RA0			0x40080014
#define TC_RC0			0x4008001C

#define TC_CMR_WAVE		(1 << 15)
#define TC_CCR_CLKEN	(1 << 0)
#define TC_CCR_CLKDIS	(1 << 1)
#define TC_CCR_SWTRG	(1 << 2)

// #define AUDIO_CLIP_TEST
// #define AUDIO_FLASH_TEST

#if defined(AUDIO_CLIP_TEST) || defined(AUDIO_FLASH_TEST)
static void audioTest();
#endif

static DRIVER_FN(audioSvc);

void audio_init() {
	// Piezo buzzer is connected to DAC0
	PUT32(PMC_PCER1, 1 << (PERIPHERAL_ID_DACC - 32));
	PUT32(NVIC_ISER1, 1 << (PERIPHERAL_ID_DACC - 32));

#if defined(AUDIO_CLIP_TEST) || defined(AUDIO_FLASH_TEST)
	audioTest();
#else
	PUT32(PMC_PCER0, 1 << (PERIPHERAL_ID_TC0));
	PUT32(TC_CMR0, TC_CMR_WAVE |
		0 |			// TCLK0
		2 << 13 |	// WAVSEL=2 means UP_RC
		3 << 18);	// ACPC (RC Compare Effect on TIOA) 3=Toggle
	PUT32(TC_RC0, 1905/4); // 1905 is 22050 times a second and RC0 period is a quarter waveform
	PUT32(DACC_MR,
		1 << 8 | // Set REFRESH to something like 20us
		1 << 1 | // TRGSEL=1, TIO Output of the Timer Counter Channel 0
		1);		 // TRGEN=1, enable external trigger
	PUT32(DACC_CHER, 1 << 0); // Enable channel 0
#endif

	kern_registerDriver(FOURCC("BEEP"), audioSvc);
}

void flash_readData(uint8* buf, uintptr address, int len);
static void fillAudioBuf();
static void stop();

#define KAudioBufBase ((uint8*)KNfcRamBase)
#define KAudioBufEnd (KAudioBufBase + 4096)

static void fillAudioBuf_dfc(uintptr arg0, uintptr arg1, uintptr arg2) {
	fillAudioBuf();
}

static void fillAudioBuf() {
	uint8* ptrToFill;
	int size;
	int availableAudioData = TheSuperPage->audioEnd - TheSuperPage->audioAddr;
	if (availableAudioData == 0) {
		printk("Filling buf with no more data!\n");
		stop();
		return;
	}
	if (!TheSuperPage->audioBufPtr) {
		// Buf empty, fill whole thing
		ptrToFill = KAudioBufBase;
		size = min(4096, availableAudioData);
		TheSuperPage->audioBufPtr = KAudioBufBase;
	} else if (TheSuperPage->audioBufNewestDataEnd == KAudioBufEnd) {
		// Fill bottom half
		ptrToFill = KAudioBufBase;
		size = min(2048, availableAudioData);
	} else {
		// Fill top half
		ptrToFill = KAudioBufBase + 2048;
		size = min(2048, availableAudioData);
	}
	flash_readData(ptrToFill, TheSuperPage->audioAddr, size);
	TheSuperPage->audioAddr += size;
	TheSuperPage->audioBufNewestDataEnd = ptrToFill + size;
	// printk("audioBufNewestDataEnd = %p\n", TheSuperPage->audioBufNewestDataEnd);
}

static void play(uint32 addr, int len) {
	TheSuperPage->audioAddr = addr;
	TheSuperPage->audioEnd = addr + len;
	// printk("Playing audio from %x to %x\n", addr, addr+len);
	fillAudioBuf();

	// Preload DACC fifo
	// printk("Preloading DACC\n");
	while ((GET32(DACC_ISR) & DACC_TXRDY)) {
		PUT32(DACC_CDR, ((uint32)(*TheSuperPage->audioBufPtr++) << 4));
	}
	// Enable DACC_TXRDY DACC interrupt
	// printk("Enabling DACC_TXRDY\n");
	PUT32(DACC_IER, DACC_TXRDY);
	// printk("About to start TC\n");
	// Start timer running
	PUT32(TC_CCR0, TC_CCR_CLKEN | TC_CCR_SWTRG);
}

static void stop() {
	TheSuperPage->audioEnd = 0;
	PUT32(TC_CCR0, TC_CCR_CLKDIS);
	PUT32(DACC_IDR, DACC_TXRDY);
	// Clear pending interrupt in case it stays up
	PUT32(NVIC_ICPR1, 1 << (PERIPHERAL_ID_DACC - 32));
}

static DRIVER_FN(audioSvc) {
	switch(arg1) {
		case KExecDriverAudioPlay: {
			if (TheSuperPage->audioEnd) return KErrAlreadyExists;
			uintptr* args = (uintptr*)arg2;
			play(args[0], args[1]);
			break;
		}
	default:
		ASSERT(false);
	}
	return 0;
}

void daccInterrupt() {
	// uint32 isr = GET32(DACC_ISR);
	// printk("daccInterrupt %x\n", isr);
	while ((GET32(DACC_ISR) & DACC_TXRDY)) {
		PUT32(DACC_CDR, ((uint32)(*TheSuperPage->audioBufPtr++) << 4));
		if (TheSuperPage->audioBufPtr == TheSuperPage->audioBufNewestDataEnd) {
			printk("Underflow!\n");
			stop();
			return;
		} else if (TheSuperPage->audioBufPtr == KAudioBufBase + 2048) {
			// Half way, fill bottom
			dfc_queue(fillAudioBuf_dfc, 0, 0, 0);
		} else if (TheSuperPage->audioBufPtr == KAudioBufEnd) {
			TheSuperPage->audioBufPtr = KAudioBufBase;
			// Wrapped around, so fill top buf
			dfc_queue(fillAudioBuf_dfc, 0, 0, 0);
		}
	}
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

#define FLASH_CHIPSELECT (SPI0 + SPI_CSR0)
#define READ_DATA		0x03

uint8 spi_readOne();

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
		// uint8 data = spi_readOne();
		PUT32(DACC_CDR, ((uint32)data) << 3);
		// 45us is the time between samples for 22kHz audio
		// 20us seems approximately right for the
		nanowait(20000);
		// nanowait(45000);
	}
}

#endif // AUDIO_FLASH_TEST
