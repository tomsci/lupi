#include <stdlib.h>
#include <lupi/exec.h>
#include "bitmap.h"
#include "font.xbm" // Rockin' the retro!

#define BLACK 0
#define WHITE 0xFFFFu

int exec_getInt(ExecGettableValue val);

Bitmap* bitmap_create(uint16 width, uint16 height) {
	if (!width) width = exec_getInt(EValScreenWidth);
	if (!height) height = exec_getInt(EValScreenHeight);

	int bufSize = width * height * 2;
	void* mem = malloc(offsetof(Bitmap, data) + bufSize);
	if (!mem) return NULL;
	Bitmap* b = (Bitmap*)mem;
	rect_set(&b->bounds, 0, 0, width, height);
	b->screenDriverHandle = 0;
	b->colour = BLACK;
	b->bgcolour = WHITE;
	b->autoBlit = false;
	b->format = (uint8)format;
	rect_zero(&b->dirtyRect);
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

void rect_union(Rect* r, const Rect* r2) {
	if (rect_isEmpty(r)) *r = *r2;
	else {
		if (r2->x < r->x) r->x = r2->x;
		if (r2->y < r->y) r->y = r2->y;
		if (r2->x + r2->w > r->x + r->w) r->w = r2->x + r2->w - r->x;
		if (r2->y + r2->h > r->y + r->h) r->h = r2->y + r2->h - r->y;
	}
}

void rect_clipToParent(Rect* r, const Rect* parent) {
	// Note, doesn't care about the parent's position, only its size
	uint16 parentw = rect_getWidth(parent);
	uint16 parenth = rect_getHeight(parent);
	if (r->x > parentw) r->x = parentw;
	if (r->y > parenth) r->y = parenth;
	if (r->x + r->w > parentw) r->w = parentw - r->x;
	if (r->y + r->h > parenth) r->h = parenth - r->h;
}

void bitmap_drawRect(Bitmap* b, const Rect* r) {
	// Line by line, by the numbers
	Rect bounds = *r;
	rect_clipToParent(&bounds, &b->bounds);
	const uint16 xend = bounds.x + bounds.w;
	const uint16 yend = bounds.y + bounds.h;
	const uint16 col = b->colour;
	const uint16 bwidth = bitmap_getWidth(b);
	uint16* data = b->data;
	for (int yidx = bounds.y; yidx < yend; yidx++) {
		for (int xidx = bounds.x; xidx < xend; xidx++) {
			data[yidx * bwidth + xidx] = col;
		}
	}
	rect_union(&b->dirtyRect, &bounds);
	if (b->autoBlit) bitmap_blitDirtyToScreen(b);
}

#define CHAR_WIDTH (font_width/8) // 8 chars per line. Currently = 7
#define CHAR_HEIGHT (font_height/12) // 12 rows. Currently = 13

static inline bool getBit(const uint8* packedData, int bitIdx) {
	uint8 byte = packedData[bitIdx >> 3];
	return byte & (1<<(bitIdx & 0x7));
}

static inline void drawch(Bitmap* b, uint16 x, uint16 y, int chIdx) {
	// 8 chars per line
	int chx = (chIdx & 7) * CHAR_WIDTH; // x pixel pos of start of character
	int chy = (chIdx >> 3) * CHAR_HEIGHT; // y pixel pos of start of character

	const uint16 bwidth = bitmap_getWidth(b);
	const uint16 bheight = bitmap_getHeight(b);
	for (int yidx = 0; yidx < CHAR_HEIGHT; yidx++) {
		if (y+yidx >= bheight) break; // rest of char is outside bitmap
		for (int xidx = 0; xidx < CHAR_WIDTH; xidx++) {
			if (x+xidx >= bwidth) break; // rest of char is outside bitmap
			int bitIdx = (chy + yidx) * font_width + chx + xidx;
			uint16 colour = getBit(font_bits, bitIdx) ? b->colour : b->bgcolour;
			b->data[(y+yidx) * bwidth + x + xidx] = colour;
		}
	}
}

void bitmap_drawText(Bitmap* b, uint16 x, uint16 y, const char* text) {
	 // Text is allowed to go offscreen, but must start onscreen
	if (x >= bitmap_getWidth(b) || y >= bitmap_getHeight(b)) return;
	const char* chptr = text;
	const int xstart = x;
	const uint16 bwidth = bitmap_getWidth(b);
	for (;;) {
		char ch = *chptr++;
		if (ch == 0 || x >= bwidth) break;
		if (ch < ' ' || ch >= 0x80) ch = 0x7F;
		int charIdx = ch - ' ';
		drawch(b, x, y, charIdx);
		x += CHAR_WIDTH;
	}
	Rect r = rect_make(xstart, y, (chptr - text - 1) * CHAR_WIDTH, CHAR_HEIGHT);
	rect_union(&b->dirtyRect, &r);
	if (b->autoBlit) bitmap_blitDirtyToScreen(b);
}

void bitmap_getTextRect(Bitmap* b, int numChars, Rect* result) {
	result->h = CHAR_HEIGHT;
	result->w = numChars * CHAR_WIDTH;
}

void bitmap_drawXbmData(Bitmap* b, uint16 x, uint16 y, const Rect* r, const uint8* xbm, uint16 xbm_width) {
	 // Bitmap is allowed to go offscreen, but must start onscreen
	if (x >= bitmap_getWidth(b) || y >= bitmap_getHeight(b)) return;
	const uint16 bwidth = bitmap_getWidth(b);
	const uint16 bheight = bitmap_getHeight(b);
	const int xbm_stride = (xbm_width + 7) & ~7;

	for (int yidx = 0; yidx < r->h; yidx++) {
		if (y+yidx >= bheight) break; // rest of xbm will be outside bitmap
		for (int xidx = 0; xidx < r->w; xidx++) {
			if (x+xidx >= bwidth) break; // rest of xbm is outside bitmap
			int bitIdx = (r->y + yidx) * xbm_stride + r->x + xidx;
			uint16 colour = getBit(xbm, bitIdx) ? b->colour : b->bgcolour;
			b->data[(y+yidx) * bwidth + x + xidx] = colour;
		}
	}

	Rect drawnRect = rect_make(x, y, r->w, r->h);
	rect_union(&b->dirtyRect, &drawnRect);
	if (b->autoBlit) bitmap_blitDirtyToScreen(b);
}

#define plot(ptr,col,x,y) ptr[y*b->bounds.w + x] = col

void bitmap_drawLine(Bitmap* b, uint16 x0, uint16 y0, uint16 x1, uint16 y1) {
	// Prof Bresenham, we salute you
	// Ok I'm too stupid, this is copied from the internets
	const int dx = (int)x1 - (int)x0;
	const int dy = (int)y1 - (int)y0;
	// "scan" is the axis we iterate over (x in quadrant 0)
	// "inc" is the axis we conditionally add to (y in quadrant 0)
	int inc, incr;
	int scan, scanStart, scanEnd, scanIncr;
	int* x; int* y;
	int dscan, dinc;
	if (abs(dx) > abs(dy)) {
		x = &scan;
		y = &inc;
		dscan = dx;
		dinc = dy;
		inc = y0;
		scanStart = x0 + 1;
		scanEnd = x1;
	} else {
		x = &inc;
		y = &scan;
		dscan = dy;
		dinc = dx;
		inc = x0;
		scanStart = y0 + 1;
		scanEnd = y1;
	}

	if (dinc < 0) {
		incr = -1;
		dinc = -dinc;
	} else {
		incr = 1;
	}
	if (dscan < 0) {
		scanIncr = -1;
		dscan = -dscan;
	} else {
		scanIncr = 1;
	}
	// Hoist these as they're constants
	const int TwoDinc = 2 * dinc;
	const int TwoDincMinusTwoDscan = 2 * dinc - 2 * dscan;
	const uint16 colour = b->colour;
	uint16 *const data = &b->data[0];

	int D = TwoDinc - dscan;
	plot(data, colour, x0, y0);
	plot(data, colour, x1, y1);

	for (scan = scanStart; scan != scanEnd; scan += scanIncr) {
		//PRINTL("scan=%d inc=%d D=%d", (int)scan, (int)inc, D);
		if (D > 0) {
			inc = inc + incr;
			plot(data, colour, *x, *y);
			D = D + TwoDincMinusTwoDscan;
		} else {
			plot(data, colour, *x, *y);
			D = D + TwoDinc;
		}
	}

	Rect r = rect_make(min(x0,x1), min(y0,y1), abs(dx) + 1, abs(dy) + 1);
	//rect_clipToParent(&r, &b->bounds);
	rect_union(&b->dirtyRect, &r);
	if (b->autoBlit) bitmap_blitDirtyToScreen(b);
}

int exec_driverConnect(uint32 driverId);
int exec_driverCmd(uint32 driverHandle, uint32 arg1, uint32 arg2);

void bitmap_blitToScreen(Bitmap* b, const Rect* r) {
	if (!b->screenDriverHandle) {
		b->screenDriverHandle = exec_driverConnect(FOURCC("SCRN"));
	}
	if (b->format == EOneBitColumnPacked) {
	} else {
		// { dataPtr, bitmapWidth, screenx, screeny, x, y, w, h }
		uint32 op[] = { (uint32)&b->data, b->bounds.w, b->bounds.x + r->x,
			b->bounds.y + r->y, r->x, r->y, r->w, r->h };
		exec_driverCmd(b->screenDriverHandle, KExecDriverScreenBlit, (uint32)&op);
	}
}

void bitmap_blitDirtyToScreen(Bitmap* b) {
	bitmap_blitToScreen(b, &b->dirtyRect);
	rect_zero(&b->dirtyRect);
}
