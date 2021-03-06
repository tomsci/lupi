#ifndef BITMAP_H
#define BITMAP_H

#include <stddef.h>

struct AffineTransform;

typedef struct Rect {
	uint16 x, y, w, h;
} Rect;

static inline void rect_zero(Rect* r) {
	r->x = 0; r->y = 0; r->w = 0; r->h = 0;
}

static inline void rect_set(Rect* r, uint16 x, uint16 y, uint16 w, uint16 h) {
	r->x = x; r->y = y; r->w = w; r->h = h;
}

static inline Rect rect_make(uint16 x, uint16 y, uint16 w, uint16 h) {
	return (Rect){ .x = x, .y = y, .w = w, .h = h };
}

static inline bool rect_isEmpty(const Rect* r) {
	return r->w == 0 || r->h == 0;
}

static inline uint16 rect_getWidth(const Rect* r) { return r->w; }
static inline uint16 rect_getHeight(const Rect* r) { return r->h; }

void rect_union(Rect* r, const Rect* r2);
void rect_clip(Rect* r, const Rect* clipRect);
void rect_transform(Rect* r, const struct AffineTransform* t);
void rect_invert(Rect* r, const struct AffineTransform* t);

typedef struct AffineTransform {
	int8 a, b, c, d;
	int16 tx, ty;
} AffineTransform;

typedef struct Bitmap {
	Rect bounds;
	uint32 screenDriverHandle; // Doesn't really belong here, but hey
	uint16 colour; // Current pen colour
	uint16 bgcolour; // Background colour, when drawing text or XBMs
	Rect dirtyRect;
	AffineTransform transform;
	uint8 flags;
	uint8 format; // a ScreenBufferFormat
	uint16 data[1]; // Extends beyond the struct
} Bitmap;

int bitmap_getAllocSize(uint16 width, uint16 height);
Bitmap* bitmap_construct(void* mem, uint16 width, uint16 height);

uint16 bitmap_getColour(const Bitmap* b);
void bitmap_setColour(Bitmap* b, uint16 colour);

uint16 bitmap_getBackgroundColour(const Bitmap* b);
void bitmap_setBackgroundColour(Bitmap* b, uint16 colour);

void bitmap_setPosition(Bitmap* b, uint16 x, uint16 y);
static inline uint16 bitmap_getWidth(const Bitmap* b) { return rect_getWidth(&b->bounds); }
static inline uint16 bitmap_getHeight(const Bitmap* b) { return rect_getHeight(&b->bounds); }

void bitmap_drawLine(Bitmap* b, uint16 x0, uint16 y0, uint16 x1, uint16 y1);
void bitmap_drawRect(Bitmap* b, const Rect* r);
void bitmap_drawText(Bitmap* b, uint16 x, uint16 y, const char* text);
void bitmap_drawXbmData(Bitmap* b, uint16 x, uint16 y, const Rect* r, const uint8* xbm, uint16 xbm_width);
void bitmap_getTextRect(Bitmap* b, int numChars, Rect* result);
void bitmap_blitToScreen(Bitmap* b, const Rect* r);
void bitmap_blitDirtyToScreen(Bitmap* b);

void bitmap_setAutoBlit(Bitmap* b, bool flag);
void bitmap_clipToBounds(const Bitmap* b, Rect* r);

#define bitmap_drawXbm(b, x, y, r, xbmName) \
	bitmap_drawXbmData(b, x, y, r, xbmName ## _bits, xbmName ## _width)

void bitmap_setTransform(Bitmap* b, const AffineTransform* transform);

#ifdef lua_h

Bitmap* bitmap_check(lua_State* L, int idx);

#endif

#endif
