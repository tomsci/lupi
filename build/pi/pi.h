#ifndef LUPI_BUILD_PI_H
#define LUPI_BUILD_PI_H

#include <stddef.h>

#define ARM
#define ARMV6
#define ARM1176
#define BCM2835

#define KPeripheralPhys		0x20000000
//#define KPeripheralLen		0x00FFFFFF
//#define KSuperPagePhys	0x00001000

#define KPhysicalRamBase	0x00000000
#define KPhysicalRamSize	0x20000000 // 512MB - we're assuming model B atm

#define MMU_DISABLED


#ifdef MMU_DISABLED

#define KPeripheralBase		0x20000000

//#define KSuperPageAddress	KSuperPagePhys

#else

// In keeping with the BCM2835 Linux kernel, we map peripheral addresses at 0xF2000000
#define KPeripheralBase		0xF2000000
//#define KPL011Base		0xF2201000
//#define KTimerBase		0xF2003000

//#define KSuperPageAddress	0xFF000000

#endif // MMU_DISABLED

#endif // LUPI_BUILD_PI_H
