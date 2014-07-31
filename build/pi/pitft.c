// Specifically, an ILI9340
// width=240, height=320

#include <k.h>
#include <exec.h>
#include "gpio.h"

uint32 GET32(uint32 addr);
void PUT32(uint32 addr, uint32 val);

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

static void doWrite(bool openEndedTransaction, int n, ...);
#define write(args...) doWrite(false, NUMVARARGS(args), args)
void tft_beginUpdate(int xStart, int yStart, int xEnd, int yEnd);

#define GPIO_DC 25 // Data/command signal, aka D/CX on the LCD controller

#define TFT_SPI_CDIV				16 // 250Mhz/16 = 16MHz
#define TFT_SPI_CS_POLL 0 // CSPOLn=0, LEN=0, INTR=0, INTD=0, CSPOL=0, CPOL=0, CPHA=0, CS=0

void tft_init() {
	TheSuperPage->screenWidth = WIDTH;
	TheSuperPage->screenHeight = HEIGHT;

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

	// Pit 24 is some kind of interrupt for the touch screen
	// requires further configuration (TODO)

	// Pin 25 is DC, data/command signal, and it's OUT_INIT_LOW
	//gpio_setPull(KGpioDisablePullUpDown, GPPUDCLK0, (1<<24)|(1<<25));
	//gpio_setPull(KGpioEnablePullDown, GPPUDCLK0, (1<<24));
	//gpio_setPull(KGpioEnablePullUp, GPPUDCLK0, 1<<25); // Fire in the hole!
	gpio_set(GPIO_DC, 0);

	// Minimal configuration of SPI bus
	PUT32(SPI_CLK, TFT_SPI_CDIV);

	write(SWRESET);
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
	write(FRAME_RATE_CONTROL_1, 0x00, 0x1B); // 18 = 79Hz?? 1B would be 70Hz default which would be much more sensible
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

	kern_registerDriver(FOURCC("pTFT"), tft_handleSvc);
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
	spi_beginTransaction(TFT_SPI_CS_POLL);
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

#define CrashBorderWidth 5
#define BlitN(colour, n) for (int i = 0; i < n; i++) { spi_write_poll(colour, 2); }
void tft_drawCrashed() {
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

static DRIVER_FN(tft_handleSvc) {
	ASSERT(arg1 == KExecDriverTftBlit, arg1);

	// Format of *arg2 is { dataPtr, bitmapWidth, screenx, screeny, x, y, w, h }

	ASSERT(arg2 < KUserMemLimit && !(arg2 & 3), arg2);
	uint32* op = (uint32*)arg2;
	uint16* data = (uint16*)op[0];
	ASSERT((uintptr)data < KUserMemLimit && !((uintptr)data & 3), (uintptr)data);
	int bwidth = op[1];
	int screenx = op[2];
	int screeny = op[3];
	int x = op[4];
	int y = op[5];
	int w = op[6];
	int h = op[7];

	if (w == bwidth) {
		// No need to worry about stride, can blit whole lot at once
		tft_beginUpdate(screenx, screeny, screenx + w - 1, screeny + h - 1);
		spi_write_poll((uint8*)(data + y*bwidth + x), 2*w*h);
		spi_endTransaction();
	} else {
		for (int yidx = 0; yidx < h; yidx++) {
			tft_beginUpdate(screenx, screeny + yidx, screenx + w + 1, screeny + yidx);
			spi_write_poll((uint8*)(data + (y+yidx)*bwidth + x), 2*w);
		}
		spi_endTransaction();
	}
	return 0;
}
