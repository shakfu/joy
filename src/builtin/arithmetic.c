/*
 *  module  : arithmetic.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped arithmetic builtins: abs, ceil, div, divide, floor, frexp, ldexp,
 *  max, min, minus, modf, mul, neg, plus, pred, rem, round, sign, succ, trunc
 */
#include "globals.h"

/* Include shared helper headers */
#include "plusminus.h"
#include "predsucc.h"
#include "maxmin.h"
#include "ufloat.h"
#include "bfloat.h"

/* Arithmetic operations */
/**
Q0  OK  1480  abs  :  N1  ->  N2
Integer N2 is the absolute value (0,1,2..) of integer N1,
or float N2 is the absolute value (0.0 ..) of float N1.
*/
void abs_(pEnv env)
{
    ONEPARAM("abs");
    /* start new */
    FLOAT("abs");
    if (nodetype(env->stck) == INTEGER_) {
        if (nodevalue(env->stck).num < 0)
            UNARY(INTEGER_NEWNODE, -nodevalue(env->stck).num);
        return;
    }
    /* end new */
    FLOAT_U(fabs);
}

/**
Q0  OK  1530  ceil  :  F  ->  G
G is the float ceiling of F.
*/
UFLOAT(ceil_, "ceil", ceil)

/**
Q0  OK  1430  div  :  I J  ->  K L
Integers K and L are the quotient and remainder of dividing I by J.
*/
void div_(pEnv env)
{
    int64_t quotient, remainder;

    TWOPARAMS("div");
    INTEGERS2("div");
    CHECKZERO("div");
    quotient = nodevalue(nextnode1(env->stck)).num / nodevalue(env->stck).num;
    remainder = nodevalue(nextnode1(env->stck)).num % nodevalue(env->stck).num;
    BINARY(INTEGER_NEWNODE, quotient);
    NULLARY(INTEGER_NEWNODE, remainder);
}

/**
Q0  OK  1410  /\0divide  :  I J  ->  K
Integer K is the (rounded) ratio of integers I and J.  Also supports float.
*/
void divide_(pEnv env)
{
    TWOPARAMS("divide");
    CHECKDIVISOR("divide");
    FLOAT_I(/);
    INTEGERS2("divide");
    BINARY(INTEGER_NEWNODE,
           nodevalue(nextnode1(env->stck)).num / nodevalue(env->stck).num);
}

/**
Q0  OK  1570  floor  :  F  ->  G
G is the floor of F.
*/
UFLOAT(floor_, "floor", floor)

/**
Q0  OK  1580  frexp  :  F  ->  G I
G is the mantissa and I is the exponent of F.
Unless F = 0, 0.5 <= abs(G) < 1.0.
*/
void frexp_(pEnv env)
{
    int exp;

    ONEPARAM("frexp");
    FLOAT("frexp");
    UNARY(FLOAT_NEWNODE, frexp(FLOATVAL, &exp));
    NULLARY(INTEGER_NEWNODE, exp);
}

/**
Q0  OK  1590  ldexp  :  F I  ->  G
G is F times 2 to the Ith power.
*/
void ldexp_(pEnv env)
{
    int exp;

    TWOPARAMS("ldexp");
    INTEGER("ldexp");
    exp = nodevalue(env->stck).num;
    POP(env->stck);
    FLOAT("ldexp");
    UNARY(FLOAT_NEWNODE, ldexp(FLOATVAL, exp));
}

/**
Q0  OK  1810  max  :  N1 N2  ->  N
N is the maximum of numeric values N1 and N2.  Also supports float.
*/
MAXMIN(max_, "max", <)

/**
Q0  OK  1820  min  :  N1 N2  ->  N
N is the minimum of numeric values N1 and N2.  Also supports float.
*/
MAXMIN(min_, "min", >)

/**
Q0  OK  1390  -\0minus  :  M I  ->  N
Numeric N is the result of subtracting integer I from numeric M.
Also supports float.
*/
PLUSMINUS(minus_, "-", -)

/**
Q0  OK  1620  modf  :  F  ->  G H
G is the fractional part and H is the integer part
(but expressed as a float) of F.
*/
void modf_(pEnv env)
{
    double exp;

    ONEPARAM("modf");
    FLOAT("modf");
    UNARY(FLOAT_NEWNODE, modf(FLOATVAL, &exp));
    NULLARY(FLOAT_NEWNODE, exp);
}

/**
Q0  OK  1400  *\0mul  :  I J  ->  K
Integer K is the product of integers I and J.  Also supports float.
*/
void mul_(pEnv env)
{
    TWOPARAMS("*");
    FLOAT_I(*);
    INTEGERS2("*");
    BINARY(INTEGER_NEWNODE,
           nodevalue(nextnode1(env->stck)).num * nodevalue(env->stck).num);
}

/**
Q0  OK  1450  neg  :  I  ->  J
Integer J is the negative of integer I.  Also supports float.
*/
void neg_(pEnv env)
{
    ONEPARAM("neg");
    /* start new */
    FLOAT("neg");
    if (nodetype(env->stck) == INTEGER_) {
        if (nodevalue(env->stck).num)
            UNARY(INTEGER_NEWNODE, -nodevalue(env->stck).num);
        return;
    }
    /* end new */
    FLOAT_U(-);
}

/**
Q0  OK  1380  +\0plus  :  M I  ->  N
Numeric N is the result of adding integer I to numeric M.
Also supports float.
*/
PLUSMINUS(plus_, "+", +)

/**
Q0  OK  1790  pred  :  M  ->  N
Numeric N is the predecessor of numeric M.
*/
PREDSUCC(pred_, "pred", -)

/**
Q0  OK  1420  rem  :  I J  ->  K
Integer K is the remainder of dividing I by J.  Also supports float.
*/
void rem_(pEnv env)
{
    TWOPARAMS("rem");
    FLOAT_P(fmod);
    INTEGERS2("rem");
    CHECKZERO("rem");
    BINARY(INTEGER_NEWNODE,
           nodevalue(nextnode1(env->stck)).num % nodevalue(env->stck).num);
}

/**
Q0  OK  3200  round  :  F  ->  G
G is F rounded to the nearest integer.
*/
UFLOAT(round_, "round", round)

/**
Q0  OK  1440  sign  :  N1  ->  N2
Integer N2 is the sign (-1 or 0 or +1) of integer N1,
or float N2 is the sign (-1.0 or 0.0 or 1.0) of float N1.
*/
void sign_(pEnv env)
{
    double dbl;

    ONEPARAM("sign");
    /* start new */
    FLOAT("sign");
    if (nodetype(env->stck) == INTEGER_) {
        if (nodevalue(env->stck).num != 0 && nodevalue(env->stck).num != 1)
            UNARY(INTEGER_NEWNODE, nodevalue(env->stck).num > 0 ? 1 : -1);
        return;
    }
    /* end new */
    if (FLOATABLE) {
        dbl = FLOATVAL;
        if (dbl > 0.0)
            dbl = 1.0;
        else if (dbl < 0.0)
            dbl = -1.0;
        else
            dbl = 0.0;
        UNARY(FLOAT_NEWNODE, dbl);
    }
}

/**
Q0  OK  1800  succ  :  M  ->  N
Numeric N is the successor of numeric M.
*/
PREDSUCC(succ_, "succ", +)

/**
Q0  OK  1690  trunc  :  F  ->  I
I is an integer equal to the float F truncated toward zero.
*/
void trunc_(pEnv env)
{
    ONEPARAM("trunc");
    FLOAT("trunc");
    UNARY(INTEGER_NEWNODE, (int64_t)FLOATVAL);
}

