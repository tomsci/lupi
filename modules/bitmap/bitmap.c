#include <stdlib.h>
#include <lupi/exec.h>
#include "bitmap.h"
#include "font.xbm" // Rockin' the retro!

#define BLACK 0
#define WHITE 0xFFFFu

int exec_getInt(ExecGettableValue val);

Bitmap* bitmap_create(int width, int height) {
	if (!width) width = exec_getInt(EValScreenWidth);
	if (!height) height = exec_getInt(EValScreenHeight);

	void* mem = malloc(offsetof(Bitmap, data) + width * height * 2);
	if (!mem) return NULL;
	Bitmap* b = (Bitmap*)mem;
	b->width = width;
	b->height = height;
	b->screenDriverHandle = 0;
	b->colour = BLACK;
	b->bgcolour = WHITE;
	return b;
}

void bitmap_destroy(Bitmap* b) {
	free(b);
}

static inline uint16 tobe(uint16 val) {
	union { uint16 hw; uint8 bb[2]; } u;
	u.bb[0] = val >> 8;
	u.bb[1] = val & 0xFF;
	return u.hw;
}

static inline uint16 frombe(uint16 val) {
	union { uint16 hw; uint8 bb[2]; } u;
	u.hw = val;
	return (((uint16)u.bb[0]) << 8) | u.bb[1];
}

void bitmap_setColour(Bitmap* b, uint16 colour) {
	// The panel expects the colour data to be big-endian, so we need to fix up
	b->colour = tobe(colour);
}

uint16 bitmap_getColour(Bitmap* b) {
	return frombe(b->colour);
}

void bitmap_setBackgroundColour(Bitmap* b, uint16 colour) {
	b->bgcolour = tobe(colour);
}

uint16 bitmap_getBackgroundColour(Bitmap* b) {
	return frombe(b->bgcolour);
}

#define BOUNDS_CLIP(b,x,y,w,h) \
	do { \
		if (x < 0) { x = 0; } \
		else if (x > b->width) { x = b->width; } \
		if (y < 0) { y = 0; } \
		else if (y > b->height) { y = b->height; } \
		if (w < 0) { w = 0; } \
		if (h < 0) { h = 0; } \
		if (x + w > b->width) { w = b->width - x; } \
		if (y + h > b->height) { h = b->height - y; } \
	} while(0)

#define BOUNDS_VALID(b,x,y,w,h) (x >= 0 && y >=0 && w >= 0 && h >=0 && x+w <= b->width && y+h <= b->height)

void bitmap_drawRect(Bitmap* b, int x, int y, int w, int h) {
	// Line by line, by the numbers
	if (!BOUNDS_VALID(b,x,y,w,h)) return; // Because screw 'em
	//BOUNDS_CLIP(b,x,y,w,h);
	const int yend = y + h;
	const int xend = x + w;
	const int col = b->colour;
	for (int yidx = y; yidx < yend; yidx++) {
		for (int xidx = x; xidx < xend; xidx++) {
			b->data[yidx * b->width + xidx] = col;
		}
	}
}

#define CHAR_WIDTH (font_width/8) // 8 chars per line. Currently = 7
#define CHAR_HEIGHT (font_height/12) // 12 rows. Currently = 13

static inline bool getBit(const uint8* packedData, int bitIdx) {
	uint8 byte = packedData[bitIdx >> 3];
	return byte & (1<<(bitIdx & 0x7));
}

static inline void drawch(Bitmap* b, int x, int y, int chIdx) {
	// 8 chars per line
	int chx = (chIdx & 7) * CHAR_WIDTH; // x pixel pos of start of character
	int chy = (chIdx >> 3) * CHAR_HEIGHT; // y pixel pos of start of character

	for (int yidx = 0; yidx < CHAR_HEIGHT; yidx++) {
		if (y+yidx >= b->height) break; // rest of char is outside bitmap
		for (int xidx = 0; xidx < CHAR_WIDTH; xidx++) {
			if (x+xidx >= b->width) break; // rest of char is outside bitmap
			int bitIdx = (chy + yidx) * font_width + chx + xidx;
			uint16 colour = getBit(font_bits, bitIdx) ? b->colour : b->bgcolour;
			b->data[(y+yidx) * b->width + x + xidx] = colour;
		}
	}
}

void bitmap_drawText(Bitmap* b, int x, int y, const char* text) {
	if (!BOUNDS_VALID(b, x, y, 0, 0)) return; // Text is allowed to go offscreen, for now
	for (;;) {
		char ch = *text++;
		if (ch == 0) break;
		if (ch < ' ' || ch >= 0x80) ch = 0x7F;
		int charIdx = ch - ' ';
		drawch(b, x, y, charIdx);
		x += CHAR_WIDTH;
	}
}

int exec_driverConnect(uint32 driverId);
int exec_driverCmd(uint32 driverHandle, uint32 arg1, uint32 arg2);

void bitmap_blitToScreen(Bitmap* b, int x, int y, int w, int h) {
	if (!b->screenDriverHandle) {
		b->screenDriverHandle = exec_driverConnect(FOURCC("pTFT"));
	}
	// It is assumed for now that the bitmap size matches the screensize, hence
	// that screenx==x and screeny==y
	uint32 op[] = { (uint32)&b->data, b->width, x, y, x, y, w, h };
	exec_driverCmd(b->screenDriverHandle, KExecDriverTftBlit, (uint32)&op);
}
