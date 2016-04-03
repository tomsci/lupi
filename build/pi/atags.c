#include <k.h>
#include <atags.h>

extern char *strstr(const char *s, const char *find);

#ifdef BCM2837
// Pi 3
#define BOARDREV_STR "bcm2709.boardrev="
#else
#define BOARDREV_STR "bcm2708.boardrev="
#endif

void parseAtags(uint32* ptr, AtagsParams* params) {
	// hexdump((char*)ptr, 0x200);

	// Grr there's a perfectly good atag for board rev (ATAG_REVISION) but the Pi
	// has to go dump it in a string we have to parse
	params->boardRev = 0;
	params->totalRam = 0;

	for (;;) {
		uint32 tagsize = ptr[0];
		uint32 tag = ptr[1];
		switch (tag) {
		case ATAG_NONE:
			return;
		case ATAG_CORE:
			// Pi doesn't put anything interesting here
			break;
		case ATAG_MEM:
			// There can in theory be multiple mem tags (although the Pi doesn't
			// do this)
			params->totalRam += ptr[2];
			break;
		case ATAG_CMDLINE: {
			// Pi puts it all in here...
			// Yuk, using user functions here. Sorry...
			char* cmdline = (char*)(ptr + 2);
			char* board = strstr(cmdline, BOARDREV_STR);
			if (board) {
				board = board + sizeof(BOARDREV_STR)-1;
				// Hackiest reimplementation of strtol follows...
				while (*board && *board != ' ') {
					char ch = *board;
					if (ch == 'x') { board++; continue; }
					int val = ch >= 'a' ? ch+0xA-'a' : ch >= 'A' ? ch+0xA-'A' : ch-'0';
					params->boardRev = (params->boardRev << 4) + val;
					board++;
				}
			}
			break;
		}
		default:
			break;
		}
		ptr += tagsize; // tagsize is in words, so this is the correct thing to do
	}
}
