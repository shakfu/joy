/*
 *  module  : stacks.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped stack builtins: dup, dupd, id, over, pick, pop, popd, rolldown,
 *  rolldownd, rollup, rollupd, rotate, rotated, stack, swap, swapd, unstack
 */
#include "globals.h"

/* Include shared helper headers */
#include "dipped.h"

/* Stack operations */
/**
Q0  OK  1210  dup  :  X  ->  X X
Pushes an extra copy of X onto stack.
*/
void dup_(pEnv env)
{
    ONEPARAM("dup");
    GNULLARY(env->stck);
}

/**
Q0  OK  1270  dupd  :  Y Z  ->  Y Y Z
As if defined by:   dupd  ==  [dup] dip
*/
DIPPED(dupd_, "dupd", TWOPARAMS, dup_)

/**
Q0  OK  1200  id  :  ->
Identity function, does nothing.
Any program of the form  P id Q  is equivalent to just  P Q.
*/
void id_(pEnv env) { /* do nothing */ }

/**
Q0  OK  3180  over  :  X Y  ->  X Y X
Pushes an extra copy of the second item X on top of the stack.
*/
void over_(pEnv env)
{
    TWOPARAMS("over");
    GNULLARY(nextnode1(env->stck));
}

/**
Q0  OK  3190  pick  :  X Y Z 2  ->  X Y Z X
Pushes an extra copy of nth (e.g. 2) item X on top of the stack.
*/
void pick_(pEnv env)
{
    Index item;
    int i, size;

    FOURPARAMS("pick"); /* minimum of four: 0 pick == dup; 1 pick == over */
    INTEGER("pick");
    size = nodevalue(env->stck).num; /* pick up the number */
    POP(env->stck);                  /* remove top of stack */
    item = env->stck; /* if the stack is too small, the last item is selected
                       */
    for (i = 0; i < size && nextnode1(item); i++) /* top of stack was popped */
        item = nextnode1(
            item); /* possibly select the last item on the stack */
    GNULLARY(item);
}

/**
Q0  OK  1320  pop  :  X  ->
Removes X from top of the stack.
*/
void pop_(pEnv env)
{
    ONEPARAM("pop");
    POP(env->stck);
}

/**
Q0  OK  1260  popd  :  Y Z  ->  Z
As if defined by:   popd  ==  [pop] dip
*/
DIPPED(popd_, "popd", TWOPARAMS, pop_)

/**
Q0  OK  1240  rolldown  :  X Y Z  ->  Y Z X
Moves Y and Z down, moves X up.
*/
void rolldown_(pEnv env)
{
    THREEPARAMS("rolldown");
    SAVESTACK;
    GTERNARY(SAVED2);
    GNULLARY(SAVED1);
    GNULLARY(SAVED3);
    POP(env->dump);
}

/**
Q0  OK  1300  rolldownd  :  X Y Z W  ->  Y Z X W
As if defined by:   rolldownd  ==  [rolldown] dip
*/
DIPPED(rolldownd_, "rolldownd", FOURPARAMS, rolldown_)

/**
Q0  OK  1230  rollup  :  X Y Z  ->  Z X Y
Moves X and Y up, moves Z down.
*/
void rollup_(pEnv env)
{
    THREEPARAMS("rollup");
    SAVESTACK;
    GTERNARY(SAVED1);
    GNULLARY(SAVED3);
    GNULLARY(SAVED2);
    POP(env->dump);
}

/**
Q0  OK  1290  rollupd  :  X Y Z W  ->  Z X Y W
As if defined by:   rollupd  ==  [rollup] dip
*/
DIPPED(rollupd_, "rollupd", FOURPARAMS, rollup_)

/**
Q0  OK  1250  rotate  :  X Y Z  ->  Z Y X
Interchanges X and Z.
*/
void rotate_(pEnv env)
{
    THREEPARAMS("rotate");
    SAVESTACK;
    GTERNARY(SAVED1);
    GNULLARY(SAVED2);
    GNULLARY(SAVED3);
    POP(env->dump);
}

/**
Q0  OK  1310  rotated  :  X Y Z W  ->  Z Y X W
As if defined by:   rotated  ==  [rotate] dip
*/
DIPPED(rotated_, "rotated", FOURPARAMS, rotate_)

/**
Q0  OK  1040  stack  :  .. X Y Z  ->  .. X Y Z [Z Y X ..]
[RUNTIME] Pushes the stack as a list.
*/
void stack_(pEnv env) { NULLARY(LIST_NEWNODE, env->stck); }

/**
Q0  OK  1220  swap  :  X Y  ->  Y X
Interchanges X and Y on top of the stack.
*/
void swap_(pEnv env)
{
    TWOPARAMS("swap");
    SAVESTACK;
    GBINARY(SAVED1);
    GNULLARY(SAVED2);
    POP(env->dump);
}

/**
Q0  OK  1280  swapd  :  X Y Z  ->  Y X Z
As if defined by:   swapd  ==  [swap] dip
*/
DIPPED(swapd_, "swapd", THREEPARAMS, swap_)

/**
Q0  OK  2000  unstack  :  [X Y ..]  ->  ..Y X
The list [X Y ..] becomes the new stack.
*/
void unstack_(pEnv env)
{
    ONEPARAM("unstack");
    LIST("unstack");
    env->stck = nodevalue(env->stck).lis;
}

