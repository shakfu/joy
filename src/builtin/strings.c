/*
 *  module  : strings.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped string builtins: chr, ord, strtod, strtol
 */
#include "globals.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>

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

/* True if end points past the digits to only trailing whitespace */
static int converted_all(const char* str, const char* end)
{
    if (end == str)
        return 0;
    while (isspace((unsigned char)*end))
        end++;
    return *end == '\0';
}

/**
Q0  OK  1750  strtod  :  S  ->  R
String S is converted to the float R.
S must be a whole number in range; trailing whitespace is allowed.
*/
void strtod_(pEnv env)
{
    char* str;
    char* end;
    double num;

    ONEPARAM("strtod");
    STRING("strtod");
    str = GETSTRING(env->stck);
    errno = 0;
    num = strtod(str, &end);
    if (!converted_all(str, end)) {
        execerror(env, "numeric string", "strtod");
        return;
    }
    /* ERANGE also flags underflow, which returns a usable denormal or 0 */
    if (errno == ERANGE && (num == HUGE_VAL || num == -HUGE_VAL)) {
        execerror(env, "float in range", "strtod");
        return;
    }
    UNARY(FLOAT_NEWNODE, num);
}

/**
Q0  OK  1740  strtol  :  S I  ->  J
String S is converted to the integer J using base I.
If I = 0, assumes base 10,
but leading "0" means base 8 and leading "0x" means base 16.
S must be a whole number in range; trailing whitespace is allowed.
*/
void strtol_(pEnv env)
{
    int64_t base;
    char* str;
    char* end;
    long long num;

    TWOPARAMS("strtol");
    INTEGER("strtol");
    base = nodevalue(env->stck).num;
    if (base != 0 && (base < 2 || base > 36)) {
        execerror(env, "base 0 or 2..36", "strtol");
        return;
    }
    STRING2("strtol");
    str = GETSTRING(nextnode1(env->stck));
    errno = 0;
    num = strtoll(str, &end, (int)base);
    if (!converted_all(str, end)) {
        execerror(env, "numeric string", "strtol");
        return;
    }
    if (errno == ERANGE) {
        execerror(env, "integer in range", "strtol");
        return;
    }
    POP(env->stck);
    UNARY(INTEGER_NEWNODE, num);
}
