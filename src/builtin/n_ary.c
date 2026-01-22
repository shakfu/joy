/*
 *  module  : n_ary.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped n-ary builtins: binary, nullary, ternary, unary, unary2, unary3,
 *  unary4
 */
#include "globals.h"

/* Include shared helper headers */

/* N-ary operations */
/**
Q1  OK  2560  binary  :  X Y [P]  ->  R
Executes P, which leaves R on top of the stack.
No matter how many parameters this consumes,
exactly two are removed from the stack.
*/
N_ARY(binary_, "binary", THREEPARAMS, SAVED4)

/**
Q1  OK  2480  nullary  :  [P]  ->  R
Executes P, which leaves R on top of the stack.
No matter how many parameters this consumes, none are removed from the stack.
*/
N_ARY(nullary_, "nullary", ONEPARAM, SAVED2)

/**
Q1  OK  2570  ternary  :  X Y Z [P]  ->  R
Executes P, which leaves R on top of the stack.
No matter how many parameters this consumes,
exactly three are removed from the stack.
*/
N_ARY(ternary_, "ternary", FOURPARAMS, SAVED5)

/**
Q1  OK  2490  unary  :  X [P]  ->  R
Executes P, which leaves R on top of the stack.
No matter how many parameters this consumes,
exactly one is removed from the stack.
*/
N_ARY(unary_, "unary", TWOPARAMS, SAVED3)

/**
Q1  OK  2500  unary2  :  X1 X2 [P]  ->  R1 R2
Executes P twice, with X1 and X2 on top of the stack.
Returns the two values R1 and R2.
*/
void unary2_(pEnv env)
{ /*   Y  Z  [P]  unary2     ==>  Y'  Z'  */
    THREEPARAMS("unary2");
    ONEQUOTE("unary2");
    SAVESTACK;
    env->stck = nextnode1(SAVED2);                        /* just Y on top */
    exec_term(env, nodevalue(SAVED1).lis);                  /* execute P */
    env->dump1 = newnode2(env, env->stck, env->dump1);    /* save P(Y) */
    env->stck = newnode2(env, SAVED2, nextnode1(SAVED3)); /* just Z on top */
    exec_term(env, nodevalue(SAVED1).lis);                  /* execute P */
    env->dump1 = newnode2(env, env->stck, env->dump1);    /* save P(Z) */
    env->stck = env->dump1;
    env->dump1 = nextnode2(env->dump1);
    nextnode2(env->stck) = SAVED4;
    POP(env->dump);
}

/**
Q1  OK  2510  unary3  :  X1 X2 X3 [P]  ->  R1 R2 R3
Executes P three times, with Xi, returns Ri (i = 1..3).
*/
void unary3_(pEnv env)
{ /*  X Y Z [P]  unary3    ==>  X' Y' Z'  */
    FOURPARAMS("unary3");
    ONEQUOTE("unary3");
    SAVESTACK;
    env->stck = nextnode1(SAVED3);                        /* just X on top */
    exec_term(env, nodevalue(SAVED1).lis);                  /* execute P */
    env->dump1 = newnode2(env, env->stck, env->dump1);    /* save P(X) */
    env->stck = newnode2(env, SAVED3, nextnode1(SAVED4)); /* just Y on top */
    exec_term(env, nodevalue(SAVED1).lis);                  /* execute P */
    env->dump1 = newnode2(env, env->stck, env->dump1);    /* save P(Y) */
    env->stck = newnode2(env, SAVED2, nextnode1(SAVED4)); /* just Z on top */
    exec_term(env, nodevalue(SAVED1).lis);                  /* execute P */
    env->dump1 = newnode2(env, env->stck, env->dump1);    /* save P(Z) */
    env->stck = env->dump1;
    env->dump1 = nextnode3(env->dump1);
    nextnode3(env->stck) = SAVED5;
    POP(env->dump);
}

/**
Q1  OK  2520  unary4  :  X1 X2 X3 X4 [P]  ->  R1 R2 R3 R4
Executes P four times, with Xi, returns Ri (i = 1..4).
*/
void unary4_(pEnv env)
{ /*  X Y Z W [P]  unary4    ==>  X' Y' Z' W'  */
    FIVEPARAMS("unary4");
    ONEQUOTE("unary4");
    SAVESTACK;
    env->stck = nextnode1(SAVED4);                        /* just X on top */
    exec_term(env, nodevalue(SAVED1).lis);                  /* execute P */
    env->dump1 = newnode2(env, env->stck, env->dump1);    /* save p(X) */
    env->stck = newnode2(env, SAVED4, nextnode1(SAVED5)); /* just Y on top */
    exec_term(env, nodevalue(SAVED1).lis);                  /* execute P */
    env->dump1 = newnode2(env, env->stck, env->dump1);    /* save P(Y) */
    env->stck = newnode2(env, SAVED3, nextnode1(SAVED5)); /* just Z on top */
    exec_term(env, nodevalue(SAVED1).lis);                  /* execute P */
    env->dump1 = newnode2(env, env->stck, env->dump1);    /* save P(Z) */
    env->stck = newnode2(env, SAVED2, nextnode1(SAVED5)); /* just W on top */
    exec_term(env, nodevalue(SAVED1).lis);                  /* execute P */
    env->dump1 = newnode2(env, env->stck, env->dump1);    /* save P(W) */
    env->stck = env->dump1;
    env->dump1 = nextnode4(env->dump1);
    nextnode4(env->stck) = SAVED6;
    POP(env->dump);
}

