#ifndef BITMAP_H
#define BITMAP_H

#include <stddef.h>

// Everything is 16-bit 5-6-5 for the moment

typedef struct Bitmap {
	int width;
	int height;
	uint32 screenDriverHandle; // Doesn't really belong here, but hey
	uint16 colour; // Current pen colour
	uint16 bgcolour; // Background colour, when drawing text
	uint16 data[1]; // Extends beyond the struct
} Bitmap;

Bitmap* bitmap_create(int width, int height);
void bitmap_destroy(Bitmap* b);

uint16 bitmap_getColour(Bitmap* b);
void bitmap_setColour(Bitmap* b, uint16 colour);

uint16 bitmap_getBackgroundColour(Bitmap* b);
void bitmap_setBackgroundColour(Bitmap* b, uint16 colour);

void bitmap_drawRect(Bitmap* b, int x, int y, int w, int h);
void bitmap_drawText(Bitmap* b, int x, int y, const char* text);
void bitmap_blitToScreen(Bitmap* b, int x, int y, int w, int h);

#endif
