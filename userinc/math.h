#ifndef MATH_H
#define MATH_H

// We let l_mathop do our work for us, for the most part
#define floor(x) __builtin_floor(x)

//#define HUGE_VAL (__builtin_huge_val())

#define RAND_MAX 0x7fffffff
//TODO!
int rand();
void srand(unsigned seed);

#endif
