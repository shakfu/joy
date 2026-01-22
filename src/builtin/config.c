/*
 *  module  : config.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped config builtins: autoput, echo, maxint, setautoput, setecho,
 *  setsize, setundeferror, undeferror
 */
#include "globals.h"

/* Config operations */
/**
Q0  OK  1090  autoput  :  ->  I
[SETTINGS] Pushes current value of flag for automatic output, I = 0..2.
*/
PUSH(autoput_, INTEGER_NEWNODE, env->config.autoput)

/**
Q0  OK  1120  echo  :  ->  I
[SETTINGS] Pushes value of echo flag, I = 0..3.
*/
PUSH(echo_, INTEGER_NEWNODE, env->config.echoflag)

/**
Q0  IMMEDIATE  1020  maxint  :  ->  maxint
Pushes largest integer (platform dependent). Typically it is 32 bits.
*/
PUSH(maxint_, INTEGER_NEWNODE, MAXINT_)

/**
Q0  IGNORE_POP  2980  setautoput  :  I  ->
[IMPURE] Sets value of flag for automatic put to I (if I = 0, none;
if I = 1, put; if I = 2, stack).
*/
void setautoput_(pEnv env)
{
    ONEPARAM("setautoput");
    NUMERICTYPE("setautoput");
    if (!env->config.autoput_set)
        env->config.autoput = nodevalue(env->stck).num;
    POP(env->stck);
}

/**
Q0  IGNORE_POP  3000  setecho  :  I  ->
[IMPURE] Sets value of echo flag for listing.
I = 0: no echo, 1: echo, 2: with tab, 3: and linenumber.
*/
void setecho_(pEnv env)
{
    ONEPARAM("setecho");
    NUMERICTYPE("setecho");
    env->config.echoflag = nodevalue(env->stck).num;
    POP(env->stck);
}

/**
Q0  OK  1030  setsize  :  ->  setsize
Pushes the maximum number of elements in a set (platform dependent).
Typically it is 32, and set members are in the range 0..31.
*/
PUSH(setsize_, INTEGER_NEWNODE, SETSIZE)

/**
Q0  IGNORE_POP  2990  setundeferror  :  I  ->
[IMPURE] Sets flag that controls behavior of undefined functions
(0 = no error, 1 = error).
*/
void setundeferror_(pEnv env)
{
    ONEPARAM("setundeferror");
    NUMERICTYPE("setundeferror");
    if (!env->config.undeferror_set)
        env->config.undeferror = nodevalue(env->stck).num;
    POP(env->stck);
}

/**
Q0  OK  1100  undeferror  :  ->  I
[SETTINGS] Pushes current value of undefined-is-error flag.
*/
PUSH(undeferror_, INTEGER_NEWNODE, env->config.undeferror)

