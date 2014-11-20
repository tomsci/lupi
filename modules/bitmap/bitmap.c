#include <stdlib.h>
#include <string.h>
#include <lupi/exec.h>
#include "bitmap.h"
#include "font.xbm" // Rockin' the retro!
#include "font_small.xbm"

#define BLACK 0
#define WHITE 0xFFFFu

int exec_getInt(ExecGettableValue val);

#define BMP_DEBUG(...)
// #define BMP_DEBUG printf

#ifdef ONE_BPP_BITMAPS

#ifdef BITBAND
#define PixelType uint32
#define bitmap_getPixels(b) BITBAND(b->data)
#define datasize(w, h) (((w) * (h) + 7) / 8)
#else
#error "Can't use ONE_BPP_BITMAPS without also supporting BITBAND"
#endif // BITBAND

#else // !ONE_BPP_BITMAPS

#define PixelType uint16
#define bitmap_getPixels(b) ((uint16*)(b->data))
#define datasize(w, h) ((w) * (h) * 2)

#endif // ONE_BPP_BITMAPS

typedef enum Flags {
	AutoBlit = 1,
	SmallFont = 2,
	TransformSet = 4,
} Flags;

struct DrawContext;
typedef void (*PixelFn)(const struct DrawContext*, int, int, uint16);

// Contains precalculated values that are needed when drawing
typedef struct DrawContext {
	PixelType* data;
	int bwidth;
	int bheight;
	int drawWidth;
	int drawHeight;
	const AffineTransform pixelTransform;
	PixelFn setPixelFn;
} DrawContext;

static const AffineTransform IdentityTransform = { .a = 1, .b = 0, .c = 0, .d = 1, .tx = 0, .ty = 0 };
#define transform_x(t, x, y) ((int)(x) * (t).a + (int)(y) * (t).b + (t).tx)
#define transform_y(t, x, y) ((int)(x) * (t).c + (int)(y) * (t).d + (t).ty)

#define DECLARE_CONTEXT(b) \
	const DrawContext context = { \
		.data = bitmap_getPixels(b), \
		.bwidth = bitmap_getWidth(b), \
		.bheight = bitmap_getHeight(b), /* Only needed for debugging? */ \
		.drawWidth = b->transform.b == 0 ? b->bounds.w : b->bounds.h, \
		.drawHeight = b->transform.b == 0 ? b->bounds.h : b->bounds.w, \
		.pixelTransform = { \
			.a = b->transform.a, .b = b->transform.b, \
			.c = b->transform.c, .d = b->transform.d, \
			/* The -1 here is because the pixel "rects" in the case of when */ \
			/* we use a non-zero translate have a width/height of -1 not 1  */ \
			/* which we otherwise fail to account for, and this is the      */ \
			/* simplest way to fix it. Will have to revisit this if we ever */ \
			/* use the transform for anything other that 90-degree rotations*/ \
			.tx = b->transform.tx ? b->transform.tx - 1 : 0, \
			.ty = b->transform.ty ? b->transform.ty - 1 : 0 \
		}, \
		.setPixelFn = (b->flags & TransformSet) ? setPixelTransformed : setPixelRaw \
	}

#define set_pixel(x, y, col) context.setPixelFn(&context, x, y, col)

#if defined(ONE_BPP_BITMAPS)

static inline void setPixelRaw(const DrawContext* context, int x, int y, uint16 colour) {
	int page = y / 8;
	int byteidx = page * context->bwidth + x;
	int bit = y - (page * 8);
	int bitidx = byteidx * 8 + bit;
	context->data[bitidx] = (colour == BLACK);
}

#else

static inline void setPixelRaw(const DrawContext* context, int x, int y, uint16 col) {
	context->data[y * context->bwidth + x] = col;
}

#endif // ONE_BPP_BITMAPS

static void setPixelTransformed(const DrawContext* context, int x, int y, uint16 col) {
	AffineTransform const*const t = &context->pixelTransform;
	int xx = transform_x(*t, x, y);
	int yy = transform_y(*t, x, y);
	if (xx < 0 || xx >= context->bwidth || yy < 0 || yy >= context->bheight) {
		BMP_DEBUG("Transform fail (%d, %d) -> (%d, %d) in %dx%d bmp!\n", x, y, xx, yy, context->bwidth, context->bheight);
		return;
	}
	setPixelRaw(context, xx, yy, col);
}

// Width and height must not be zero
int bitmap_getAllocSize(uint16 width, uint16 height) {
	int bufSize = datasize(width, height);
#ifndef ONE_BPP_BITMAPS
	if (exec_getInt(EValScreenFormat) == EOneBitColumnPacked) {
		// We need an extra buf to transform to the packed format before blitting
		bufSize += width * ((height + 7) >> 3);
	}
#endif
	return offsetof(Bitmap, data) + bufSize;
}

Bitmap* bitmap_construct(void* mem, uint16 width, uint16 height) {
	Bitmap* b = (Bitmap*)mem;
	rect_set(&b->bounds, 0, 0, width, height);
	b->screenDriverHandle = 0;
	b->colour = BLACK;
	b->bgcolour = WHITE;
	b->flags = 0;
	const int screenHeight = exec_getInt(EValScreenHeight);
	if (screenHeight < 200) {
		b->flags |= SmallFont;
	}
	b->format = (uint8)exec_getInt(EValScreenFormat);
	rect_zero(&b->dirtyRect);
	b->transform = IdentityTransform;
	memset(b->data, 0, datasize(width, height));
	return b;
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

uint16 bitmap_getColour(const Bitmap* b) {
	return frombe(b->colour);
}

void bitmap_setBackgroundColour(Bitmap* b, uint16 colour) {
	b->bgcolour = tobe(colour);
}

uint16 bitmap_getBackgroundColour(const Bitmap* b) {
	return frombe(b->bgcolour);
}

void rect_union(Rect* r, const Rect* r2) {
	if (rect_isEmpty(r)) *r = *r2;
	else {
		int xmin = min(r->x, r2->x);
		int xmax = max(r->x + r->w, r2->x + r2->w);
		int ymin = min(r->y, r2->y);
		int ymax = max(r->y + r->h, r2->y + r2->h);
		r->x = xmin;
		r->y = ymin;
		r->w = xmax - xmin;
		r->h = ymax - ymin;
	}
}

void rect_transform(Rect* r, const AffineTransform* t) {
	int newx = transform_x(*t, r->x, r->y);
	int newy = transform_y(*t, r->x, r->y);
	int w = transform_x(*t, r->x + r->w, r->y + r->h) - newx;
	int h = transform_y(*t, r->x + r->w, r->y + r->h) - newy;
	BMP_DEBUG("Raw rect_transform %d,%d,%dx%d -> %d,%d,%dx%d\n", r->x, r->y, r->w, r->h, newx, newy, w, h);
	// And make sure to normalise
	if (w < 0) {
		r->x = newx + w;
		r->w = -w;
	} else {
		r->x = newx;
		r->w = w;
	}
	if (h < 0) {
		r->y = newy + h;
		r->h = -h;
	} else {
		r->y = newy;
		r->h = h;
	}
}

void rect_invert(Rect* r, const AffineTransform* t) {
	// Apply the inverse of t to r
	AffineTransform inv = {
		.a = t->a,
		.b = -t->b,
		.c = -t->c,
		.d = t->d,
		.tx = -t->tx * t->a + t->ty * t->b,
		.ty = -t->ty * t->d + t->tx * t->c
	};
	rect_transform(r, &inv);
}

void bitmap_clipToBounds(const Bitmap* b, Rect* r) {
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
	BMP_DEBUG("drawnRect = %d,%d,%dx%d\n", r->x, r->y, r->w, r->h);
	if (b->flags & TransformSet) {
		rect_transform(r, &b->transform);
		BMP_DEBUG("transformedRect = %d,%d,%dx%d\n", r->x, r->y, r->w, r->h);
	}
	bitmap_clipToBounds(b, r);
	BMP_DEBUG("clipped drawnRect = %d,%d,%dx%d\n", r->x, r->y, r->w, r->h);
	rect_union(&b->dirtyRect, r);
	BMP_DEBUG("dirtyRect = %d,%d,%dx%d\n", b->dirtyRect.x, b->dirtyRect.y, b->dirtyRect.w, b->dirtyRect.h);
	if (b->flags & AutoBlit) bitmap_blitDirtyToScreen(b);
}

void bitmap_drawRect(Bitmap* b, const Rect* rect) {
	// Line by line, by the numbers
	DECLARE_CONTEXT(b);
	Rect r = *rect;
	const Rect bounds = rect_make(0, 0, context.drawWidth, context.drawHeight);
	rect_clip(&r, &bounds);
	const uint16 xend = r.x + r.w;
	const uint16 yend = r.y + r.h;

	const uint16 col = b->colour;
	for (int yidx = r.y; yidx < yend; yidx++) {
		for (int xidx = r.x; xidx < xend; xidx++) {
			set_pixel(xidx, yidx, col);
		}
	}
	updateDirtyRect(b, &r);
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
	Rect r = rect_make(0, 0, 0, 0);
	if (b->flags & SmallFont) {
		r.h = CHAR_HEIGHT(font_small);
		r.w = numChars * CHAR_WIDTH(font_small);
	} else {
		r.h = CHAR_HEIGHT(font);
		r.w = numChars * CHAR_WIDTH(font);
	}
	result->w = r.w;
	result->h = r.h;
}

void bitmap_drawXbmData(Bitmap* b, uint16 x, uint16 y, const Rect* r, const uint8* xbm, uint16 xbm_width) {
	DECLARE_CONTEXT(b);

	 // xbm drawing is allowed to continue outside the bitmap, but must start inside
	if (x >= context.drawWidth || y >= context.drawHeight) {
		BMP_DEBUG("(%d,%d) outside of %dx%d!\n", x, y, context.drawWidth, context.drawHeight);
		return;
	}
	const int xbm_stride = (xbm_width + 7) & ~7;

	for (int yidx = 0; yidx < r->h; yidx++) {
		if (y+yidx >= context.drawHeight) break; // rest of xbm will be outside bitmap
		for (int xidx = 0; xidx < r->w; xidx++) {
			if (x+xidx >= context.drawWidth) break; // rest of xbm is outside bitmap
			int bitIdx = (r->y + yidx) * xbm_stride + r->x + xidx;
			uint16 colour = getBit(xbm, bitIdx) ? b->colour : b->bgcolour;
			set_pixel(x + xidx, y + yidx, colour);
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
	DECLARE_CONTEXT(b);

	int D = TwoDinc - dscan;
	set_pixel(x0, y0, colour);
	set_pixel(x1, y1, colour);

	for (scan = scanStart; scan != scanEnd; scan += scanIncr) {
		BMP_DEBUG("scan=%d inc=%d D=%d", (int)scan, (int)inc, D);
		if (D > 0) {
			inc = inc + incr;
			set_pixel(*x, *y, colour);
			D = D + TwoDincMinusTwoDscan;
		} else {
			set_pixel(*x, *y, colour);
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

void bitmap_setTransform(Bitmap* b, const AffineTransform* t) {
	if (!t) t = &IdentityTransform;
	b->transform = *t;

	if (t->a == 1 && t->b == 0 && t->c == 0 && t->d == 1 && t->tx == 0 && t->ty == 0) {
		// Identity transform, clear TransformSet for performance reasons
		b->flags &= ~TransformSet;
	} else {
		b->flags |= TransformSet;
	}
}
