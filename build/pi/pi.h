#ifndef LUPI_BUILD_PI_H
#define LUPI_BUILD_PI_H

#include <stddef.h>

#define ARM
#define ARMV6
#define ARM1176
#define BCM2835

#define KPeripheralPhys		0x20000000
#define KPeripheralSize		0x00300000
//#define KSuperPagePhys	0x00004000

#define KPhysicalRamBase	0x00000000
#define KPhysicalRamSize	0x20000000 // 512MB - we're assuming model B atm

#define KSystemClockFreq	250000000 // 250 MHz

//#define KPeripheralBase	KPeripheralPhys
#define KPeripheralBase		0xF2000000
//#define KTimerBase		0xF2003000

#ifdef KLUA
#define KLuaHeapBase		0x00200000
#endif

#endif // LUPI_BUILD_PI_H
