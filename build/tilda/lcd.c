#include "pio.h"
#include <exec.h>
#include <err.h>

#define WIDTH 128
#define HEIGHT 64

#define LCD_SPI_CHIPSELECT	(SPI0 + SPI_CSR2)
#define SPI0_NPCS2			(1 << 21) // PB21 peripheral B, "104" (board pin "52"?)
#define LCD_CS_PIN			SPI0_NPCS2
#define LCD_BACKLIGHT		(1 << 1) // PC1, "33" ??
#define LCD_RESET			(1 << 2) // PC2, "34"
#define LCD_A0				(1 << 6) // PC6, "38
#define LCD_POWER			(1 << 8) // PC8, "40"
#define LCD_C_PINS			(LCD_BACKLIGHT | LCD_RESET | LCD_A0 | LCD_POWER)

#define GPIO_LCD_POWER		(PIOC | 8)
#define GPIO_LCD_CS			(PIOB | 21)
#define GPIO_LCD_RESET		(PIOC | 2)
#define GPIO_LCD_A0			(PIOC | 6)
#define GPIO_LCD_BACKLIGHT	(PIOC | 1)

// See st7565r.pdf p50
#define CMD_SET_RESISTOR_RATIO	0x20
#define CMD_SET_POWER_CONTROL	0x28
#define CMD_SET_VOLUME_FIRST	0x81
#define CMD_SET_VOLUME_SECOND	0
#define CMD_SET_DISP_START_LINE	0x40
#define CMD_SET_ADC_REVERSE		0xA1
#define CMD_SET_BIAS_9			0xA2
#define CMD_SET_ALLPTS_NORMAL	0xA4
#define CMD_DISPLAY_REVERSE		0xA6
#define CMD_DISPLAY_OFF			0xAE
#define CMD_DISPLAY_ON			0xAF
#define CMD_SET_COM_NORMAL		0xC0

#define CMD_SET_PAGE			0xB0
#define CMD_SET_COLUMN_LO		0x00
#define CMD_SET_COLUMN_HI		0x10

#define NUM_PAGES				8

static void write(uint8 cmd);
static void setBrightness(uint8 val);
static DRIVER_FN(screen_handleSvc);

void screen_init() {
	TheSuperPage->screenWidth = WIDTH;
	TheSuperPage->screenHeight = HEIGHT;
	TheSuperPage->screenFormat = EOneBitColumnPacked;

	// LCD_POWER, LCD_A0, LCD_RESET are all PIOC outputs
	PUT32(PIOC + PIO_IDR, LCD_C_PINS); // Disable interrupts
	PUT32(PIOC + PIO_PUDR, LCD_C_PINS); // Pull-up disable
	PUT32(PIOC + PIO_OER, LCD_C_PINS); // Output enabled
	PUT32(PIOC + PIO_ENABLE, LCD_C_PINS); // PIO enabled

	// Configure the chip select pin
	PUT32(PIOB + PIO_IDR, LCD_CS_PIN);
	PUT32(PIOB + PIO_PUDR, LCD_CS_PIN);
	PUT32(PIOB + PIO_ODR, LCD_CS_PIN); // Output disable
	PUT32(PIOB + PIO_ABSR, LCD_CS_PIN); // Select Peripheral B
	PUT32(PIOB + PERIPHERAL_ENABLE, LCD_CS_PIN);

	gpio_set(GPIO_LCD_POWER, 0); // That's what it says...
	gpio_set(GPIO_LCD_A0, 0);
	// gpio_set(GPIO_LCD_CS, 1);
	gpio_set(GPIO_LCD_RESET, 0);

	kern_sleep(200);

	gpio_set(GPIO_LCD_RESET, 1);
	// gpio_set(GPIO_LCD_CS, 1); // CS is active low so this disables

	uint32 csr =
		(1 << 24) | // DLYBCT = 32*1 cycles delay between writes
		(21 << 8) |	// SCBR = 21 with our 84MHz clock means 4MHz SPI clock
		SPI_MODE0;	// We are a nice boring mode0 device (CPOL=0 CPHA=0)
	PUT32(LCD_SPI_CHIPSELECT, csr);

	// SPI.begin(LCD_CS)
	// | PinConfigure(LCD_CS) (again?)
	// | setClockDivider(_pin, 21); // Default speed set to 4Mhz
	// | setDataMode(_pin, SPI_MODE0);
	// | setBitOrder(_pin, MSBFIRST);
	// | // SPI_CSR_DLYBCT(1) keeps CS enabled for 32 MCLK after a completed
	// | // transfer. Some device needs that for working properly.
	// | setDLYBCT(_pin, 1);

	spi_beginTransaction(LCD_SPI_CHIPSELECT);
	write(CMD_SET_BIAS_9);
	write(CMD_SET_ADC_REVERSE);
	write(CMD_SET_COM_NORMAL);
	write(CMD_SET_DISP_START_LINE);
	write(CMD_SET_POWER_CONTROL | 0x4);
	kern_sleep(50);
	write(CMD_SET_POWER_CONTROL | 0x6);
	kern_sleep(50);
	write(CMD_SET_POWER_CONTROL | 0x7);
	write(CMD_SET_RESISTOR_RATIO | 0x7);
	write(CMD_SET_ALLPTS_NORMAL);
	write(CMD_DISPLAY_ON);
	setBrightness(0x08);
	// gpio_set(GPIO_LCD_BACKLIGHT, 0);

	// Write some initial data
	write(CMD_SET_DISP_START_LINE);
	for (int p = 0; p < NUM_PAGES; p++) {
		write(CMD_SET_PAGE | p);
		write(CMD_SET_COLUMN_LO | 4);
		write(CMD_SET_COLUMN_HI);
		// Do we need to wait for the above to complete before setting A0?
		gpio_set(GPIO_LCD_A0, 1);
		for (int i = 0; i < WIDTH; i++) {
			uint8 pattern = (i & 1) ? 0x55 : 0xAA;
			spi_write_poll(&pattern, 1);
		}
	}
	// spi_endTransaction();

	kern_registerDriver(FOURCC("SCRN"), screen_handleSvc);
}

static void write(uint8 cmd) {
	gpio_set(GPIO_LCD_A0, 0);
	spi_write_poll(&cmd, 1);
}

static void setBrightness(uint8 val) {
	write(CMD_SET_VOLUME_FIRST);
	write(CMD_SET_VOLUME_SECOND | (val & 0x3f));
}

static void doBlit(uintptr arg2) {
	// Format of *arg2 is { dataPtr, bitmapWidth, screenx, screeny, x, y, w, h }

	ASSERT_USER_PTR32(arg2);
	uint32* op = (uint32*)arg2;
	const uint8* data = (uint8*)op[0];
	ASSERT_USER_PTR8(data);
	const int bwidth = op[1];
	const int screenx = op[2];
	const int screeny = op[3];
	const int x = op[4];
	const int y = op[5];
	const int w = op[6];
	const int h = op[7];

	const int startPage = screeny >> 3;
	const int numPages = h >> 3;
	// printk("Blitting %d,%d,%dx%d to %d,%d\n", x, y, w, h, screenx, screeny);

	// Assumes data is already in the correct format and y and h are 8px aligned
	for (int pageIdx = 0; pageIdx < numPages; pageIdx++) {
		write(CMD_SET_PAGE | (startPage + pageIdx));
		// The way the screen is wired up on Tilda, the pixels appear to go from
		// columns 4-131 rather than 0-127.
		int colAddr = screenx + 4;
		write(CMD_SET_COLUMN_HI | ((colAddr >> 4) & 0xF));
		write(CMD_SET_COLUMN_LO | (colAddr & 0xF));
		gpio_set(GPIO_LCD_A0, 1);
		int srcPage = (y >> 3) + pageIdx;
		uint32 flags = (pageIdx == numPages-1) ? KSpiFlagLastXfer : 0;
		spi_readwrite_poll((uint8*)data + (srcPage * bwidth) + x, w, flags);
	}
}

// Generated with ./build/tilda/xbm2colpacked.lua < modules/bitmap/skullxbones.xbm
#define skullxbones_width 32
#define skullxbones_npages 4
static const unsigned char skullxbones[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x8f, 0x67, 0xf7, 0xfb, 0xfb,
	0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfb, 0xfb, 0xf7, 0x67,
	0x8f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xf0, 0xef, 0x00, 0xff, 0x1f, 0x0f, 0x0f, 0x0f, 0xcf, 0xff,
	0xff, 0xcf, 0x0f, 0x0f, 0x0f, 0x1f, 0xff, 0x00, 0xef, 0xf0, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xec, 0xee, 0xec, 0xe9, 0xdb, 0xd9,
	0xb2, 0xb7, 0x07, 0x4e, 0x1e, 0xbf, 0x31, 0xb0, 0xb0, 0x31, 0xbf, 0x1e,
	0x4e, 0x07, 0xb7, 0xb2, 0xd9, 0xdb, 0xe9, 0xec, 0xee, 0xec, 0xf3, 0xff,
	0xff, 0xff, 0xff, 0xe3, 0x9b, 0xbb, 0xdb, 0xed, 0xed, 0xf6, 0xf8, 0xf3,
	0xe6, 0xed, 0xec, 0xed, 0xed, 0xec, 0xed, 0xe6, 0xf3, 0xf8, 0xf6, 0xed,
	0xed, 0xdb, 0xbb, 0x9b, 0xe3, 0xff, 0xff, 0xff,
};

void screen_drawCrashed() {
	// Invert the screen
	write(CMD_DISPLAY_REVERSE | 1);
	// And draw some nice encouraging graphics
	write(CMD_SET_DISP_START_LINE);
	int xstart = (WIDTH - skullxbones_width) / 2;
	int ystart = (HEIGHT/8 - skullxbones_npages) / 2;
	for (int p = 0; p < skullxbones_npages; p++) {
		write(CMD_SET_PAGE | (ystart + p));
		int col = xstart + 4;
		write(CMD_SET_COLUMN_LO | (col & 0xF));
		write(CMD_SET_COLUMN_HI | ((col >> 4) & 0xF));
		gpio_set(GPIO_LCD_A0, 1);
		spi_write_poll(&skullxbones[skullxbones_width * p], skullxbones_width);
	}
}

static DRIVER_FN(screen_handleSvc) {
	switch(arg1) {
		case KExecDriverScreenBlit:
			doBlit(arg2);
			return 0;
		default:
			ASSERT(false, arg1);
	}
}
