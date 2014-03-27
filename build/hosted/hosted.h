#ifndef LUPI_BUILD_HOSTED_H
#define LUPI_BUILD_HOSTED_H

#define HOSTED

#include <stddef.h>

extern void* kernelMemory;

#define KPhysicalRamBase ((uintptr)kernelMemory)
#define KPhysicalRamSize (4 * 1024*1024)

//#define KSuperPageAddress (kernelMemory)

#endif
