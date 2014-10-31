#ifndef LUPI_BUILD_TILDAUSER_H
#define LUPI_BUILD_TILDAUSER_H

#define THUMB
#define THUMB2
#define ARMV7_M
#define CORTEX_M3

#define ONE_BPP_BITMAPS
#define BITBAND(addr) ((uint32*)(0x22000000 + ((uintptr)(addr) - 0x20000000) * 32))

#endif
