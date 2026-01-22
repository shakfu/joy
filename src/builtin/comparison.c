/*
 *  module  : comparison.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped comparison builtins: compare, eql, equal, geql, greater, leql,
 *  less, neql, sametype
 */
#include "globals.h"

/* Include shared helper headers */
#include "compare.h"
#include "comprel.h"
#include "comprel2.h"

/* Comparison operations */
/**
Q0  OK  2050  compare  :  A B  ->  I
I (=-1,0,+1) is the comparison of aggregates A and B.
The values correspond to the predicates <=, =, >=.
*/
COMPREL2(compare_, "compare", INTEGER_NEWNODE, +)

/**
Q0  OK  2270  =\0eql  :  X Y  ->  B
Either both X and Y are numeric or both are strings or symbols.
Tests whether X equal to Y.  Also supports float.
*/
COMPREL2(eql_, "=", BOOLEAN_NEWNODE, ==)

/**
Q0  OK  2280  equal  :  T U  ->  B
(Recursively) tests whether trees T and U are identical.
*/
static int equal_aux(pEnv env, Index n1, Index n2); /* forward */

static int equal_list_aux(pEnv env, Index n1, Index n2)
{
    if (!n1 && !n2)
        return 1;
    if (!n1 || !n2)
        return 0;
    if (equal_aux(env, n1, n2))
        return equal_list_aux(env, nextnode1(n1), nextnode1(n2));
    else
        return 0;
}

static int equal_aux(pEnv env, Index n1, Index n2)
{
    if (nodetype(n1) == LIST_ && nodetype(n2) == LIST_)
        return equal_list_aux(env, nodevalue(n1).lis, nodevalue(n2).lis);
    return !Compare(env, n1, n2);
}

void equal_(pEnv env)
{
    TWOPARAMS("equal");
    BINARY(BOOLEAN_NEWNODE, equal_aux(env, env->stck, nextnode1(env->stck)));
}

/**
Q0  OK  2220  >=\0geql  :  X Y  ->  B
Either both X and Y are numeric or both are strings or symbols.
Tests whether X greater than or equal to Y.  Also supports float.
*/
COMPREL(geql_, ">=", BOOLEAN_NEWNODE, >=, !(j & ~i))

/**
Q0  OK  2230  >\0greater  :  X Y  ->  B
Either both X and Y are numeric or both are strings or symbols.
Tests whether X greater than Y.  Also supports float.
*/
COMPREL(greater_, ">", BOOLEAN_NEWNODE, >, i != j && !(j & ~i))

/**
Q0  OK  2240  <=\0leql  :  X Y  ->  B
Either both X and Y are numeric or both are strings or symbols.
Tests whether X less than or equal to Y.  Also supports float.
*/
COMPREL(leql_, "<=", BOOLEAN_NEWNODE, <=, !(i & ~j))

/**
Q0  OK  2250  <\0less  :  X Y  ->  B
Either both X and Y are numeric or both are strings or symbols.
Tests whether X less than Y.  Also supports float.
*/
COMPREL(less_, "<", BOOLEAN_NEWNODE, <, i != j && !(i & ~j))

/**
Q0  OK  2260  !=\0neql  :  X Y  ->  B
Either both X and Y are numeric or both are strings or symbols.
Tests whether X not equal to Y.  Also supports float.
*/
COMPREL2(neql_, "!=", BOOLEAN_NEWNODE, !=)

/**
Q0  OK  3210  sametype  :  X Y  ->  B
Tests whether X and Y have the same type.
*/
void sametype_(pEnv env)
{
    int op;

    TWOPARAMS("sametype");
    if ((op = nodetype(env->stck)) == ANON_FUNCT_)
        op = nodevalue(env->stck).proc == nodevalue(nextnode1(env->stck)).proc;
    else
        op = op == nodetype(nextnode1(env->stck));
    BINARY(BOOLEAN_NEWNODE, op);
}

