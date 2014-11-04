// Specifically, an ILI9340
// width=240, height=320

#include <k.h>
#include <mmu.h> // For switch_process()
#include <exec.h>
#include "gpio.h"

#define WIDTH 240
#define HEIGHT 320

// From ILI9340.pdf
#define SWRESET 					0x01
#define SLEEP_OUT					0x11
#define GAMMA_SET					0x26
#define DISPLAY_OFF					0x28
#define DISPLAY_ON					0x29
#define COLUMN_ADDRESS_SET			0x2A
#define PAGE_ADDRESS_SET			0x2B
#define MEMORY_WRITE				0x2C
#define MEMORY_ACCESS_CONTROL		0x36
#define PIXEL_FORMAT_SET			0x3A
#define FRAME_RATE_CONTROL_1		0xB1
#define DISPLAY_FUNCTION_CONTROL	0xB6
#define POWER_CONTROL_1				0xC0
#define POWER_CONTROL_2				0xC1
#define VCOM_CONTROL_1				0xC5
#define VCOM_CONTROL_2				0xC7
#define POSITIVE_GAMMA_CORRECT		0xE0
#define NEGATIVE_GAMMA_CORRECT		0xE1

#define MAC_CTL_MX					0x40
#define MAC_CTL_MY					0x80
#define MAC_CTL_MV					0x20
#define MAC_CTL_BGR					0x08

// From CD00226473.pdf
#define REG_READ	0x80

#define SYS_CTRL1 0x03
#define SYS_CTRL2	0x04
#define INT_CTRL 	0x09
#define INT_EN		0x0A
#define INT_STA		0x0B
#define ADC_CTRL1	0x20
#define ADC_CTRL2	0x21
#define TSC_CTRL	0x40
#define TSC_CFG		0x41
#define FIFO_TH		0x4A
#define FIFO_CTRL_STA	0x4B
#define FIFO_SIZE	0x4C
#define TSC_DATA_X	0x4D
#define TSC_DATA_Y	0x4F

// SYS_CTRL1
#define TSC_SOFT_RESET			(2)
// SYS_CTRL2
#define GPIO_OFF				(1<<2)
// TSC_CTRL
#define TSC_STA					(1 << 7)
#define TSC_TRACKING_NONE		(0)
#define TSC_TRACKING_8			(0x2 << 4)
#define TSC_OP_MOD_XY			(0x1 << 1)
#define TSC_ENABLE				(1)
// TSC_CFG
#define TSC_SETTLING_500US		(2)
#define TSC_TOUCH_DET_DELAY_1MS	(0x4 << 3)
#define TSC_AVERAGE_CTRL_8		(0x3 << 6)
// INT_CTRL
#define TSC_INT_EDGE			(1<<1)
#define TSC_INT_ENABLE			(1)
// INT_EN, INT_STA
#define TSC_INT_TOUCH_DET		(1 << 0)
#define TSC_INT_FIFO_TH			(1 << 1)
#define TSC_INT_FIFO_OFLOW		(1 << 4)
// ADC_CTRL1
#define ADC_SAMPLE_80			(4<<4)
#define ADC_12BIT				(1<<3)
// ADC_CTRL2
#define ADC_FREQ_6_5_MHZ		(2)
// FIFO_CTRL_STA
#define FIFO_RESET				(1)

static void doWrite(bool openEndedTransaction, int n, ...);
#define write(args...) doWrite(false, NUMVARARGS(args), args)
void tft_beginUpdate(int xStart, int yStart, int xEnd, int yEnd);
uint32 tsc_register_read(uint8 reg, int regSize);
void tsc_register_write(uint8 reg, uint8 val);
static DRIVER_FN(tft_handleSvc);
static void tft_gpioInterruptDfcFn(uintptr arg1, uintptr arg2, uintptr arg3);

#define GPIO_DC 25 // Data/command signal, aka D/CX on the LCD controller

// TFT = the display, TSC = touch screen controller
#define TFT_SPI_CDIV	16 // 250Mhz/16 = 16MHz
#define TSC_SPI_CDIV	1024 // 250Mhz/500 = 500kHz
#define TFT_SPI_CS_POLL 0 // CSPOLn=0, LEN=0, INTR=0, INTD=0, CSPOL=0, CPOL=0, CPHA=0, CS=0
#define TSC_SPI_CS 		1 // As above except CS=1
#define begin_tftOperation() spi_beginTransaction(TFT_SPI_CS_POLL, TFT_SPI_CDIV)
#define begin_tscOperation() spi_beginTransaction(TSC_SPI_CS, TSC_SPI_CDIV)

void screen_init() {
	TheSuperPage->screenWidth = WIDTH;
	TheSuperPage->screenHeight = HEIGHT;
	TheSuperPage->screenFormat = EFiveSixFive;

	// Configure GPIOs for SPI pins
	uint32 gfpsel0 = GET32(GPFSEL0);
	uint32 gfpsel1 = GET32(GPFSEL1);
	uint32 gfpsel2 = GET32(GPFSEL2);
	SetGpioFunctionForPin(gfpsel0, 7, KGpioAlt0); // SPI0_CE1_N
	SetGpioFunctionForPin(gfpsel0, 8, KGpioAlt0); // SPI0_CE0_N
	SetGpioFunctionForPin(gfpsel0, 9, KGpioAlt0); // SPI0_MISO
	SetGpioFunctionForPin(gfpsel1, 10, KGpioAlt0); // SPI0_MOSI
	SetGpioFunctionForPin(gfpsel1, 11, KGpioAlt0); // SPI0_CLCK
	SetGpioFunctionForPin(gfpsel2, 24, KGpioModeInput); // interrupt thingy
	SetGpioFunctionForPin(gfpsel2, 25, KGpioModeOutput); // GPIO_DC
	PUT32(GPFSEL0, gfpsel0);
	PUT32(GPFSEL1, gfpsel1);
	PUT32(GPFSEL2, gfpsel2);

	// Pin 24 is a GPIO input connected to the interrupt for the touch screen
	// It requires a pull-up for reliable operation (apparently) and should be
	// configured for detecting falling edge
	gpio_setPull(KGpioEnablePullUp, GPPUDCLK0, 1<<24);
	uint32 gpfen = GET32(GPFEN0); // falling edge detect enabled
	PUT32(GPFEN0, gpfen | (1<<24));

	// And enable the gpio_int[0] interrupt
	uint32 irqen2 = GET32(IRQ_ENABLE_2);
	PUT32(IRQ_ENABLE_2, irqen2 | (1 << (GPIO0_INT - 32)));

	// Pin 25 is DC, data/command signal, and it's OUT_INIT_LOW
	gpio_set(GPIO_DC, 0);

	write(SWRESET);
	// While we're here about to wait, reset the touchscreen too
	tsc_register_write(SYS_CTRL1, TSC_SOFT_RESET);

	kern_sleep(5);
	write(DISPLAY_OFF);

	// Magical undocumented startup sequence from Arduino follows
	write(0xEF, 0x03, 0x80, 0x02);
	write(0xCF, 0x00, 0xC1, 0x30);
	write(0xED, 0x64, 0x03, 0x12, 0x81);
	write(0xE8, 0x85, 0x00, 0x78);
	write(0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02);
	write(0xF7, 0x20);
	write(0xEA, 0x00, 0x00);

	write(POWER_CONTROL_1, 0x23); // 4.6V apparently
	write(POWER_CONTROL_2, 0x10); // Wth is SAP[2:0]?
	write(VCOM_CONTROL_1, 0x3E, 0x28); // 0x3E = VCOMH voltage 4.25V, 0x28 = VCOML -1.5V
	write(VCOM_CONTROL_2, 0x86); // My head hurts

	write(MEMORY_ACCESS_CONTROL, MAC_CTL_MX | MAC_CTL_MY | MAC_CTL_MV | MAC_CTL_BGR);
	// Screen is now landscape
	TheSuperPage->screenWidth = HEIGHT;
	TheSuperPage->screenHeight = WIDTH;

	write(PIXEL_FORMAT_SET, 0x55); // 16-bit
	write(FRAME_RATE_CONTROL_1, 0x00, 0x10); // 1B = 70Hz default, 10 = 119Hz
	write(DISPLAY_FUNCTION_CONTROL, 0x08, 0x82, 0x27); // Stuff.
	write(0xF2, 0x00); // Undocumented: disable gamma?
	write(GAMMA_SET, 0x01);
	write(POSITIVE_GAMMA_CORRECT, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E,
						   0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00);
	write(NEGATIVE_GAMMA_CORRECT, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31,
						   0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F);
	write(SLEEP_OUT);
	kern_sleep(5);

	// And fill the window with some arbitrary colour
	tft_beginUpdate(0, 0, TheSuperPage->screenWidth-1, TheSuperPage->screenHeight-1);
	uint8 data[] = {0xE0, 0x1F}; // Purple
	for (int i = 0; i < WIDTH * HEIGHT; i++) {
		spi_write_poll(data, 2);
	}
	spi_endTransaction();
	write(DISPLAY_ON);

	// Time to set up touchscreen

	//uint32 id = tsc_register_read(0, 2);
	//printk("TSC chip id = %X\n", id);
	//printk("TSC chip ver = %X\n", tsc_register_read(0x02, 1));

	// Enable TSC and ADC clocks - have to do this before setting eg TSC_CTRL
	tsc_register_write(SYS_CTRL2, GPIO_OFF);

	tsc_register_write(TSC_CTRL, TSC_TRACKING_8 | TSC_OP_MOD_XY);
	tsc_register_write(TSC_CFG,
		TSC_SETTLING_500US | TSC_TOUCH_DET_DELAY_1MS | TSC_AVERAGE_CTRL_8);
	tsc_register_write(FIFO_TH, 4); // Seems as good a number as any right now
	tsc_register_write(INT_CTRL, TSC_INT_EDGE | TSC_INT_ENABLE);
	tsc_register_write(INT_EN, TSC_INT_TOUCH_DET | TSC_INT_FIFO_TH);
	tsc_register_write(ADC_CTRL1, ADC_SAMPLE_80 | ADC_12BIT);
	tsc_register_write(ADC_CTRL2, ADC_FREQ_6_5_MHZ);

	// Finally, enable touchscreen by ORing TSC_ENABLE
	tsc_register_write(TSC_CTRL, TSC_TRACKING_8 | TSC_OP_MOD_XY | TSC_ENABLE);
	tsc_register_write(FIFO_CTRL_STA, FIFO_RESET);
	tsc_register_write(FIFO_CTRL_STA, 0);

	kern_registerDriver(FOURCC("SCRN"), tft_handleSvc);
	kern_registerDriver(FOURCC("INPT"), tft_handleSvc);
}

void tft_gpioHandleInterrupt() {
	//printk("tft_gpioHandleInterrupt!\n");
	dfc_queue(tft_gpioInterruptDfcFn, 0, 0, 0);
	// Deassert the GPIO interrupt so we don't immediate loop back in here when
	// we reenable interrupts. Note the TSC interrupts are still asserted
	// meaning the TSC won't reassert the GPIO interrupt until we handle then in
	// tft_gpioInterruptDfcFn.
	PUT32(GPEDS0, 1<<24);
}

#define MAX_VARGS 16
static void doWrite(bool openEndedTransaction, int n, ...) {
	ASSERT(n <= MAX_VARGS, n);
	va_list args;
	uint8 buf[MAX_VARGS];

	va_start(args, n);
	for (int i = 0; i < n; i++) {
		buf[i] = (uint8)va_arg(args, uint);
	}
	va_end(args);

	gpio_set(GPIO_DC, 0);
	begin_tftOperation();
	spi_write_poll(&buf[0], 1); // Command byte

	gpio_set(GPIO_DC, 1);
	if (n > 1) {
		spi_write_poll(&buf[1], n-1);
	}
	// Note we don't set GPIO_DC back to anything on exit, we leave it in data
	// state
	if (!openEndedTransaction) {
		spi_endTransaction();
	}
}

/**
Call this before writing framebuffer data to the SPI link. After you're
finished, you must call `spi_endTransaction()`. Note that `xend` and `yend` are
inclusive, ie they refer to the index of last pixel you will be writing to.
*/
void tft_beginUpdate(int xStart, int yStart, int xEnd, int yEnd) {
	write(COLUMN_ADDRESS_SET, xStart >> 8, xStart & 0xFF, xEnd >> 8, xEnd & 0xFF);
	write(PAGE_ADDRESS_SET, yStart >> 8, yStart & 0xFF, yEnd >> 8, yEnd & 0xFF);
	doWrite(true, 1, MEMORY_WRITE);
}

uint32 tsc_register_read(uint8 reg, int regSize) {
	begin_tscOperation();
	uint32 ret;
	if (regSize == 1) {
		uint8 cmd[] = { reg | REG_READ, 0 };
		spi_readwrite_poll(cmd, sizeof(cmd), true);
		ret = cmd[1];
	} else if (regSize == 2) {
		uint8 cmd[] = { reg | REG_READ, (reg+1) | REG_READ, 0 };
		spi_readwrite_poll(cmd, sizeof(cmd), true);
		ret = (((uint32)(cmd[1])) << 8) | (cmd[2]);
	/*
	} else if (regSize == 4) {
		uint8 cmd[] = { reg | REG_READ, (reg+1) | REG_READ, (reg+2) | REG_READ, (reg+3) | REG_READ, 0 };
		spi_readwrite_poll(cmd, sizeof(cmd), true);
		ret = (((uint32)(cmd[1])) << 24) | (cmd[2] << 16) | (cmd[3] << 8) | (cmd[4]);
	*/
	} else {
		ASSERT(false, regSize);
	}
	spi_endTransaction();
	return ret;
}

void tsc_register_write(uint8 reg, uint8 val) {
	//printk("Writing %x to reg %x\n", val, reg);
	begin_tscOperation();
	uint8 cmd[] = { reg, val };
	spi_write_poll(cmd, sizeof(cmd));
	spi_endTransaction();
}

#define CrashBorderWidth 5
#define BlitN(colour, n) for (int i = 0; i < n; i++) { spi_write_poll(colour, 2); }
void screen_drawCrashed() {
	uint8 red[] = {0xF8, 0x00};
	int w = TheSuperPage->screenWidth;
	int h = TheSuperPage->screenHeight;
	// Top
	tft_beginUpdate(0, 0, w-1, CrashBorderWidth-1);
	BlitN(red, w * CrashBorderWidth);
	// Left
	tft_beginUpdate(0, 0, CrashBorderWidth-1, h-1);
	BlitN(red, CrashBorderWidth * h);
	// Right
	tft_beginUpdate(w - CrashBorderWidth, 0, w-1, h-1);
	BlitN(red, CrashBorderWidth * h);
	// Bottom
	tft_beginUpdate(0, h - CrashBorderWidth, w-1, h-1);
	BlitN(red, w * CrashBorderWidth);
	spi_endTransaction();
}

static int doBlit(uintptr arg2);
static int doInputRequest(uintptr arg2);

static DRIVER_FN(tft_handleSvc) {
	switch(arg1) {
		case KExecDriverScreenBlit:
			return doBlit(arg2);
		case KExecDriverInputRequest:
			return doInputRequest(arg2);
		default:
			ASSERT(false, arg1);
	}
}

static int doBlit(uintptr arg2) {
	// Format of *arg2 is { dataPtr, bitmapWidth, screenx, screeny, x, y, w, h }

	ASSERT_USER_PTR32(arg2);
	uint32* op = (uint32*)arg2;
	uint16* data = (uint16*)op[0];
	ASSERT_USER_PTR16(data);
	const int bwidth = op[1];
	const int screenx = op[2];
	const int screeny = op[3];
	const int x = op[4];
	const int y = op[5];
	const int w = op[6];
	const int h = op[7];
	//printk("Blitting %d,%d,%dx%d to %d,%d\n", x, y, w, h, screenx, screeny);

	if (w == bwidth) {
		// No need to worry about stride, can blit whole lot at once
		tft_beginUpdate(screenx, screeny, screenx + w - 1, screeny + h - 1);
		spi_write_poll((uint8*)(data + y*bwidth + x), 2*w*h);
		spi_endTransaction();
	} else {
		for (int yidx = 0; yidx < h; yidx++) {
			tft_beginUpdate(screenx, screeny + yidx, screenx + w - 1, screeny + yidx);
			spi_write_poll((uint8*)(data + (y+yidx)*bwidth + x), 2*w);
		}
		spi_endTransaction();
	}
	return 0;
}

static void drainFifoAndCompleteRequest() {
	int numSamples = tsc_register_read(FIFO_SIZE, 1);
	if (numSamples == 0 && !TheSuperPage->needToSendTouchUp) return;

	int n = min(numSamples, TheSuperPage->inputRequestBufferSize);
	switch_process(processForThread(TheSuperPage->inputRequest.thread));
	uint32* udataPtr = (uint32*)(TheSuperPage->inputRequestBuffer);
	for (int i = 0; i < n; i++) {
		uint16 x = tsc_register_read(TSC_DATA_X, 2);
		uint16 y = tsc_register_read(TSC_DATA_Y, 2);
		*udataPtr++ = InputTouchDown;
		*udataPtr++ = ((uint32)x << 16) | (uint32)y;
		//printk("FIFO %d = %d,%d\n", i, x, y);
	}

	// Clear the FIFO threshold now we've drained it
	tsc_register_write(INT_STA, TSC_INT_FIFO_TH);

	if (TheSuperPage->needToSendTouchUp && n < TheSuperPage->inputRequestBufferSize) {
		TheSuperPage->needToSendTouchUp = false;
		*udataPtr++ = InputTouchUp;
		// And dummy x and y
		*udataPtr++ = 0;
		n++;
	}

	thread_requestComplete(&TheSuperPage->inputRequest, n);
}

static int doInputRequest(uintptr arg2) {
	int err = kern_setInputRequest(arg2);
	if (err) return err;

	drainFifoAndCompleteRequest(); // Will only complete if needed
	return 0;
}

static void tft_gpioInterruptDfcFn(uintptr arg1, uintptr arg2, uintptr arg3) {
	if ((tsc_register_read(INT_STA, 1) & TSC_INT_TOUCH_DET) &&
		(tsc_register_read(TSC_CTRL, 1) & TSC_STA) == 0) {
		TheSuperPage->needToSendTouchUp = true;
		//printk("Touch up with %d samples to go!\n", tsc_register_read(FIFO_SIZE, 1));
	}
	// We've made a note of important TOUCH_DET events
	tsc_register_write(INT_STA, TSC_INT_TOUCH_DET);

	if (!TheSuperPage->inputRequest.userPtr) return; // Nothing else doing
	drainFifoAndCompleteRequest();
}
