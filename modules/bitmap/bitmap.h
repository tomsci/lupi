#ifndef BITMAP_H
#define BITMAP_H

#include <stddef.h>

// Everything is 16-bit 5-6-5 for the moment

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
	Rect ret; rect_set(&ret, x, y, w, h);
	return ret;
}

static inline bool rect_isEmpty(const Rect* r) {
	return r->w == 0 || r->h == 0;
}

static inline uint16 rect_getWidth(const Rect* r) { return r->w; }
static inline uint16 rect_getHeight(const Rect* r) { return r->h; }

void rect_union(Rect* r, const Rect* r2);

typedef struct Bitmap {
	Rect bounds;
	uint32 screenDriverHandle; // Doesn't really belong here, but hey
	uint16 colour; // Current pen colour
	uint16 bgcolour; // Background colour, when drawing text
	Rect dirtyRect;
	bool autoBlit; // Flush every draw operation straight to the screen (debug)
	uint16 data[1]; // Extends beyond the struct
} Bitmap;

Bitmap* bitmap_create(uint16 width, uint16 height);
void bitmap_destroy(Bitmap* b);

uint16 bitmap_getColour(Bitmap* b);
void bitmap_setColour(Bitmap* b, uint16 colour);

uint16 bitmap_getBackgroundColour(Bitmap* b);
void bitmap_setBackgroundColour(Bitmap* b, uint16 colour);

void bitmap_setPosition(Bitmap* b, uint16 x, uint16 y);
static inline uint16 bitmap_getWidth(Bitmap* b) { return rect_getWidth(&b->bounds); }
static inline uint16 bitmap_getHeight(Bitmap* b) { return rect_getHeight(&b->bounds); }

void bitmap_drawLine(Bitmap* b, uint16 x0, uint16 y0, uint16 x1, uint16 y1);
void bitmap_drawRect(Bitmap* b, const Rect* r);
void bitmap_drawText(Bitmap* b, uint16 x, uint16 y, const char* text);
void bitmap_blitToScreen(Bitmap* b, const Rect* r);
void bitmap_blitDirtyToScreen(Bitmap* b);

static inline void bitmap_setAutoBlit(Bitmap* b, bool flag) { b->autoBlit = flag; }
#endif