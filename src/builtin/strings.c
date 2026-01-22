/*
 *  module  : strings.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped string builtins: chr, ord, strtod, strtol
 */
#include "globals.h"

/* Include shared helper headers */

/* String operations */
/**
Q0  OK  1470  chr  :  I  ->  C
C is the character whose Ascii value is integer I (or logical or character).
*/
ORDCHR(chr_, "chr", CHAR_NEWNODE)

/**
Q0  OK  1460  ord  :  C  ->  I
Integer I is the Ascii value of character C (or logical or integer).
*/
ORDCHR(ord_, "ord", INTEGER_NEWNODE)

/**
Q0  OK  1750  strtod  :  S  ->  R
String S is converted to the float R.
*/
void strtod_(pEnv env)
{
    char* str;

    ONEPARAM("strtod");
    STRING("strtod");
    str = GETSTRING(env->stck);
    UNARY(FLOAT_NEWNODE, strtod(str, 0));
}

/**
Q0  OK  1740  strtol  :  S I  ->  J
String S is converted to the integer J using base I.
If I = 0, assumes base 10,
but leading "0" means base 8 and leading "0x" means base 16.
*/
void strtol_(pEnv env)
{
    int base;
    char* str;

    TWOPARAMS("strtol");
    INTEGER("strtol");
    base = nodevalue(env->stck).num;
    POP(env->stck);
    STRING("strtol");
    str = GETSTRING(env->stck);
    UNARY(INTEGER_NEWNODE, strtoll(str, 0, base));
}
