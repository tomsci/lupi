#include <stdlib.h>
#include <lupi/exec.h>
#include "bitmap.h"
#include "font.xbm" // Rockin' the retro!
#include "font_small.xbm"

#define BLACK 0
#define WHITE 0xFFFFu

int exec_getInt(ExecGettableValue val);

#define BMP_DEBUG(...)
// #define BMP_DEBUG printk
// void printk(const char* fmt, ...) ATTRIBUTE_PRINTF(1, 2);

#ifdef ONE_BPP_BITMAPS

#ifdef BITBAND
#define PixelType uint32
#define bitmap_getPixels(b) BITBAND(b->data)
#define datasize(w, h) (((w) * (h) + 7) / 8)
static inline void set_pixel(uint32* data, int bwidth, int x, int y, uint16 colour) {
	int page = y / 8;
	int byteidx = page * bwidth + x;
	int bit = y - (page * 8);
	int bitidx = byteidx * 8 + bit;
	data[bitidx] = (colour == BLACK);
}
#else
#error "Can't use ONE_BPP_BITMAPS without also supporting BITBAND"
#endif // BITBAND

#else // !ONE_BPP_BITMAPS

#define PixelType uint16
#define bitmap_getPixels(b) ((uint16*)(b->data))
#define datasize(w, h) ((w) * (h) * 2)
#define set_pixel(ptr, bwidth, x, y, col) (ptr)[(y)*(bwidth) + (x)] = (col)

#endif // ONE_BPP_BITMAPS

typedef enum Flags {
	AutoBlit = 1,
	SmallFont = 2,
} Flags;


Bitmap* bitmap_create(uint16 width, uint16 height) {
	if (!width) width = exec_getInt(EValScreenWidth);
	const int screenHeight = exec_getInt(EValScreenHeight);
	if (!height) height = screenHeight;
	ScreenBufferFormat format = (ScreenBufferFormat)exec_getInt(EValScreenFormat);
	int bufSize = datasize(width, height);
#ifndef ONE_BPP_BITMAPS
	if (format == EOneBitColumnPacked) {
		// We need an extra buf to transform to the packed format before blitting
		bufSize += width * ((height + 7) >> 3);
	}
#endif
	void* mem = malloc(offsetof(Bitmap, data) + bufSize);
	if (!mem) return NULL;
	Bitmap* b = (Bitmap*)mem;
	rect_set(&b->bounds, 0, 0, width, height);
	b->screenDriverHandle = 0;
	b->colour = BLACK;
	b->bgcolour = WHITE;
	b->flags = 0;
	if (screenHeight < 200) {
		b->flags |= SmallFont;
	}
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

void bitmap_clipToBounds(Bitmap* b, Rect* r) {
	// For the purposes of drawing inside the bitmap, the bitmap's bounds should
	// always be considered to be (0,0). The fact that we use the x and y to
	// indicate position of the bitmap relative to the screen is not relevant
	// here
	Rect bounds = rect_make(0, 0, b->bounds.w, b->bounds.h);
	rect_clip(r, &bounds);
}

void rect_clip(Rect* r, const Rect* clipr) {
	int dx = (int)clipr->x - (int)r->x;
	if (dx > 0) { r->x += dx; r->w -= dx; }

	dx = (int)r->x + (int)r->w - (int)clipr->x - (int)clipr->w;
	if (dx > 0) r->w -= dx;

	int dy = (int)clipr->y - (int)r->y;
	if (dy > 0) { r->y += dy; r->h -= dy; }

	dy = (int)r->y + (int)r->h - (int)clipr->y - (int)clipr->h;
	if (dy > 0) r->h -= dy;
}

static void updateDirtyRect(Bitmap* b, Rect* r) {
	BMP_DEBUG("drawnRect = %d,%d,%dx%d\n", r.x, r.y, r.w, r.h);
	bitmap_clipToBounds(b, r);
	BMP_DEBUG("clipped drawnRect = %d,%d,%dx%d\n", r.x, r.y, r.w, r.h);
	rect_union(&b->dirtyRect, r);
	if (b->flags & AutoBlit) bitmap_blitDirtyToScreen(b);
}

void bitmap_drawRect(Bitmap* b, const Rect* r) {
	// Line by line, by the numbers
	Rect bounds = *r;
	bitmap_clipToBounds(b, &bounds);
	const uint16 xend = bounds.x + bounds.w;
	const uint16 yend = bounds.y + bounds.h;
	const uint16 col = b->colour;
	const uint16 bwidth = bitmap_getWidth(b);
	PixelType* data = bitmap_getPixels(b);
	for (int yidx = bounds.y; yidx < yend; yidx++) {
		for (int xidx = bounds.x; xidx < xend; xidx++) {
			set_pixel(data, bwidth, xidx, yidx, col);
		}
	}
	updateDirtyRect(b, &bounds);
}

// All font bitmaps must have 8 chars per line, and 12 rows
#define CHAR_WIDTH(fontname) (fontname ## _width / 8)
#define CHAR_HEIGHT(fontname) (fontname ## _height / 12)

static inline bool getBit(const uint8* packedData, int bitIdx) {
	uint8 byte = packedData[bitIdx >> 3];
	return byte & (1<<(bitIdx & 0x7));
}

static inline int drawch(Bitmap* b, uint16 x, uint16 y, int chIdx) {
	int charw, charh;
	if (b->flags & SmallFont) {
		charw = CHAR_WIDTH(font_small);
		charh = CHAR_HEIGHT(font_small);
	} else {
		charw = CHAR_WIDTH(font);
		charh = CHAR_HEIGHT(font);
	}

	int chx = (chIdx & 7) * charw; // x pixel pos of start of character
	int chy = (chIdx >> 3) * charh; // y pixel pos of start of character

	Rect r = rect_make(chx, chy, charw, charh);
	if (b->flags & SmallFont) {
		bitmap_drawXbm(b, x, y, &r, font_small);
	} else {
		bitmap_drawXbm(b, x, y, &r, font);
	}
	return charw;
}

void bitmap_drawText(Bitmap* b, uint16 x, uint16 y, const char* text) {
	const char* chptr = text;
	for (;;) {
		char ch = *chptr++;
		if (ch == 0) break;
		if (ch < ' ' || ch >= 0x80) ch = 0x7F;
		int charIdx = ch - ' ';
		x += drawch(b, x, y, charIdx);
	}
}

void bitmap_getTextRect(Bitmap* b, int numChars, Rect* result) {
	if (b->flags & SmallFont) {
		result->h = CHAR_HEIGHT(font_small);
		result->w = numChars * CHAR_WIDTH(font_small);
	} else {
		result->h = CHAR_HEIGHT(font);
		result->w = numChars * CHAR_WIDTH(font);
	}
}

void bitmap_drawXbmData(Bitmap* b, uint16 x, uint16 y, const Rect* r, const uint8* xbm, uint16 xbm_width) {
	 // xbm drawing is allowed to continue outside the bitmap, but must start inside
	const uint16 bwidth = bitmap_getWidth(b);
	const uint16 bheight = bitmap_getHeight(b);
	if (x >= bwidth || y >= bheight) return;
	const int xbm_stride = (xbm_width + 7) & ~7;

	PixelType* data = bitmap_getPixels(b);
	for (int yidx = 0; yidx < r->h; yidx++) {
		if (y+yidx >= bheight) break; // rest of xbm will be outside bitmap
		for (int xidx = 0; xidx < r->w; xidx++) {
			if (x+xidx >= bwidth) break; // rest of xbm is outside bitmap
			int bitIdx = (r->y + yidx) * xbm_stride + r->x + xidx;
			uint16 colour = getBit(xbm, bitIdx) ? b->colour : b->bgcolour;
			set_pixel(data, bwidth, x + xidx, y + yidx, colour);
		}
	}

	Rect drawnRect = rect_make(x, y, r->w, r->h);
	updateDirtyRect(b, &drawnRect);
}

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
	const int bwidth = bitmap_getWidth(b);
	PixelType* data = bitmap_getPixels(b);

	int D = TwoDinc - dscan;
	set_pixel(data, bwidth, x0, y0, colour);
	set_pixel(data, bwidth, x1, y1, colour);

	for (scan = scanStart; scan != scanEnd; scan += scanIncr) {
		BMP_DEBUG("scan=%d inc=%d D=%d", (int)scan, (int)inc, D);
		if (D > 0) {
			inc = inc + incr;
			set_pixel(data, bwidth, *x, *y, colour);
			D = D + TwoDincMinusTwoDscan;
		} else {
			set_pixel(data, bwidth, *x, *y, colour);
			D = D + TwoDinc;
		}
	}

	Rect r = rect_make(min(x0,x1), min(y0,y1), abs(dx) + 1, abs(dy) + 1);
	updateDirtyRect(b, &r);
}

int exec_driverConnect(uint32 driverId);
int exec_driverCmd(uint32 driverHandle, uint32 arg1, uint32 arg2);

void bitmap_blitToScreen(Bitmap* b, const Rect* r) {
	if (!b->screenDriverHandle) {
		b->screenDriverHandle = exec_driverConnect(FOURCC("SCRN"));
	}
	if (b->format == EOneBitColumnPacked) {
		const int bwidth = bitmap_getWidth(b);
		const int starty = r->y & ~0x7; // Round down to 8px boundary
		const int endy = (r->y + r->h + 7) & ~0x7;
#ifdef ONE_BPP_BITMAPS
		uint8* buf = (uint8*)b->data;
#else
		// Need to transform into the packed format, copy that into the temp
		// buf past the end of the bitmap data, then send that to the driver
		//ASSERT((b->bounds.y & 7) == 0); // Otherwise the compositing is too nasty to contemplate
		uint8* buf = (uint8*)b->data + datasize(bwidth, bitmap_getHeight(b));
		PixelType* data = bitmap_getPixels(b);
		for (int y = starty; y < endy; y += 8) {
			for (int x = r->x; x < r->x + r->w; x++) {
				// Each byte has bits for col x, rows y thru y+7
				byte pageByte = 0;
				for (int i = 0; i < 8; i++) {
					if (data[(y+i)*bwidth + x] == BLACK) {
						pageByte |= 1 << i;
					}
				}
				int bufIdx = (y >> 3) * bwidth + x;
				buf[bufIdx] = pageByte;
			}
		}
#endif // ONE_BPP_BITMAPS
		// { dataPtr, bitmapWidth, screenx, screeny, x, y, w, h }
		uint32 op[] = {
			(uint32)buf, bwidth,
			b->bounds.x + r->x, b->bounds.y + starty,
			r->x, starty, r->w, endy-starty
		};
		exec_driverCmd(b->screenDriverHandle, KExecDriverScreenBlit, (uint32)&op);
		return;
	} else {
		// { dataPtr, bitmapWidth, screenx, screeny, x, y, w, h }
		uint32 op[] = {
			(uint32)&b->data, b->bounds.w,
			b->bounds.x + r->x, b->bounds.y + r->y,
			r->x, r->y, r->w, r->h
		};
		exec_driverCmd(b->screenDriverHandle, KExecDriverScreenBlit, (uint32)&op);
	}
}

void bitmap_blitDirtyToScreen(Bitmap* b) {
	bitmap_blitToScreen(b, &b->dirtyRect);
	rect_zero(&b->dirtyRect);
}

void bitmap_setAutoBlit(Bitmap* b, bool flag) {
	if (flag) b->flags |= AutoBlit;
	else b->flags &= ~AutoBlit;
}
