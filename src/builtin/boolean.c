/*
 *  module  : boolean.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped boolean builtins: all, false, some, true
 */
#include "globals.h"

/* Include shared helper headers */
#include "boolean.h"
#include "someall.h"

/* Boolean operations */
/**
Q1  OK  2860  all  :  A [B]  ->  X
Applies test B to members of aggregate A, X = true if all pass.
*/
SOMEALL(all_, "all", 1)

/**
Q0  IMMEDIATE  1000  false  :  ->  false
Pushes the value false.
*/
PUSH(false_, BOOLEAN_NEWNODE, 0)

/**
Q1  OK  2850  some  :  A [B]  ->  X
Applies test B to members of aggregate A, X = true if some pass.
*/
SOMEALL(some_, "some", 0)

/**
Q0  IMMEDIATE  1010  true  :  ->  true
Pushes the value true.
*/
PUSH(true_, BOOLEAN_NEWNODE, 1)

