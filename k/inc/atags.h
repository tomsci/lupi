#ifndef ATAGS_H
#define ATAGS_H

typedef struct AtagsParams {
	uint32 totalRam;
	uint32 boardRev;
} AtagsParams;

// These are the only ones the Pi implements
#define ATAG_CORE		0x54410001
#define ATAG_NONE		0
#define ATAG_MEM		0x54410002
#define ATAG_CMDLINE	0x54410009

#endif
