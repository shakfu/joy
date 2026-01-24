/*
 *  module  : type.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped type builtins: casting, char, file, float, ifchar, iffile,
 *  iffloat, ifinteger, iflist, iflogical, ifset, ifstring, integer, leaf,
 *  list, logical, set, string, typeof, user
 */
#include "globals.h"

/* Include shared helper headers */

/* Type operations */
/**
Q0  OK  3150  casting  :  X Y  ->  Z
[EXT] Z takes the value from X and uses the value from Y as its type.
*/
void casting_(pEnv env)
{
    Node node;

    TWOPARAMS("casting");
    node.op = nodevalue(env->stck).num;
#ifdef JOY_NATIVE_TYPES
    /* VECTOR_ and MATRIX_ require special allocation, cannot be cast to */
    if (node.op == VECTOR_ || node.op == MATRIX_) {
        execerror(env, "non-native type for casting", "casting");
        return;
    }
#endif
    POP(env->stck);
    if (node.op == STRING_ || node.op == BIGNUM_)
        node.u.str = strdup((char*)&nodevalue(env->stck));
    else
        node.u = nodevalue(env->stck);
    env->stck = newnode(env, node.op, node.u, nextnode1(env->stck));
    if (node.op == STRING_ || node.op == BIGNUM_)
        free(node.u.str);
}

/**
Q0  OK  2320  char  :  X  ->  B
Tests whether X is a character.
*/
TYPE(char_, "char", ==, CHAR_)

/**
Q0  OK  2400  file  :  F  ->  B
[FOREIGN] Tests whether F is a file.
*/
TYPE(file_, "file", ==, FILE_)

/**
Q0  OK  2390  float  :  R  ->  B
Tests whether R is a float.
*/
TYPE(float_, "float", ==, FLOAT_)

/**
Q2  OK  2620  ifchar  :  X [T] [E]  ->  ...
If X is a character, executes T else executes E.
*/
IF_TYPE(ifchar_, "ifchar", CHAR_)

/**
Q2  OK  2680  iffile  :  X [T] [E]  ->  ...
[FOREIGN] If X is a file, executes T else executes E.
*/
IF_TYPE(iffile_, "iffile", FILE_)

/**
Q2  OK  2670  iffloat  :  X [T] [E]  ->  ...
If X is a float, executes T else executes E.
*/
IF_TYPE(iffloat_, "iffloat", FLOAT_)

/**
Q2  OK  2610  ifinteger  :  X [T] [E]  ->  ...
If X is an integer, executes T else executes E.
*/
IF_TYPE(ifinteger_, "ifinteger", INTEGER_)

/**
Q2  OK  2660  iflist  :  X [T] [E]  ->  ...
If X is a list, executes T else executes E.
*/
IF_TYPE(iflist_, "iflist", LIST_)

/**
Q2  OK  2630  iflogical  :  X [T] [E]  ->  ...
If X is a logical or truth value, executes T else executes E.
*/
IF_TYPE(iflogical_, "iflogical", BOOLEAN_)

/**
Q2  OK  2640  ifset  :  X [T] [E]  ->  ...
If X is a set, executes T else executes E.
*/
IF_TYPE(ifset_, "ifset", SET_)

/**
Q2  OK  2650  ifstring  :  X [T] [E]  ->  ...
If X is a string, executes T else executes E.
*/
IF_TYPE(ifstring_, "ifstring", STRING_)

/**
Q0  OK  2310  integer  :  X  ->  B
Tests whether X is an integer.
*/
TYPE(integer_, "integer", ==, INTEGER_)

/**
Q0  OK  2370  leaf  :  X  ->  B
Tests whether X is not a list.
*/
TYPE(leaf_, "leaf", !=, LIST_)

/**
Q0  OK  2360  list  :  X  ->  B
Tests whether X is a list.
*/
TYPE(list_, "list", ==, LIST_)

/**
Q0  OK  2330  logical  :  X  ->  B
Tests whether X is a logical.
*/
TYPE(logical_, "logical", ==, BOOLEAN_)

/**
Q0  OK  2340  set  :  X  ->  B
Tests whether X is a set.
*/
TYPE(set_, "set", ==, SET_)

/**
Q0  OK  2350  string  :  X  ->  B
Tests whether X is a string.
*/
TYPE(string_, "string", ==, STRING_)

/**
Q0  OK  3220  typeof  :  X  ->  I
Replace X by its type.
*/
void typeof_(pEnv env)
{
    ONEPARAM("typeof");
    UNARY(INTEGER_NEWNODE, nodetype(env->stck));
}

/**
Q0  OK  2380  user  :  X  ->  B
Tests whether X is a user-defined symbol.
*/
TYPE(user_, "user", ==, USR_)

