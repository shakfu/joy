/*
 *  module  : sets.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped set builtins: and, has, in, not, or, xor
 */
#include "globals.h"

/* Include shared helper headers */
#include "andorxor.h"
#include "inhas.h"

/* Set operations */
/**
Q0  OK  1360  and  :  X Y  ->  Z
Z is the intersection of sets X and Y, logical conjunction for truth values.
*/
ANDORXOR(and_, "and", &, &&)

/**
Q0  OK  2290  has  :  A X  ->  B
Tests whether aggregate A has X as a member.
*/
INHAS(has_, "has", nextnode1(env->stck), env->stck)

/**
Q0  OK  2300  in  :  X A  ->  B
Tests whether X is a member of aggregate A.
*/
INHAS(in_, "in", env->stck, nextnode1(env->stck))

/**
Q0  OK  1370  not  :  X  ->  Y
Y is the complement of set X, logical negation for truth values.
*/
void not_(pEnv env)
{
    ONEPARAM("not");
    switch (nodetype(env->stck)) {
    case SET_:
        UNARY(SET_NEWNODE, ~nodevalue(env->stck).set);
        break;
    case BOOLEAN_:
    case CHAR_:
    case INTEGER_:
        UNARY(BOOLEAN_NEWNODE, !nodevalue(env->stck).num);
        break;
    default:
        BADDATA("not");
    }
}

/**
Q0  OK  1340  or  :  X Y  ->  Z
Z is the union of sets X and Y, logical disjunction for truth values.
*/
ANDORXOR(or_, "or", |, ||)

/**
Q0  OK  1350  xor  :  X Y  ->  Z
Z is the symmetric difference of sets X and Y,
logical exclusive disjunction for truth values.
*/
ANDORXOR(xor_, "xor", ^, !=)
