/*
    module  : rand.c
    version : 1.7
    date    : 09/17/24
*/
#ifndef RAND_C
#define RAND_C

#include <stdlib.h>

static int joy_random(void)
{
#if defined(__APPLE__)
    return (int)arc4random();
#else
    return rand();
#endif
}

/**
Q0  IGNORE_PUSH  1150  rand  :  ->  I
[IMPURE] I is a random integer.
*/
PUSH(rand_, INTEGER_NEWNODE, joy_random())


#endif
