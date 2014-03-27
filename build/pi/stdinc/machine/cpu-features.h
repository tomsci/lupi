#ifndef MACHINE_CPU_FEATURES_H
#define MACHINE_CPU_FEATURES_H

#define __ARM_ARCH__ 6

#define __ARM_HAVE_PLD 1

#if __ARM_HAVE_PLD
#  define  PLD(reg,offset)    pld    [reg, offset]
#else
#  define  PLD(reg,offset)    /* nothing */
#endif

#endif
