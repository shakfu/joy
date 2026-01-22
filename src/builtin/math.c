/*
 *  module  : math.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped math builtins: acos, asin, atan, atan2, cos, cosh, exp, log,
 *  log10, pow, sin, sinh, sqrt, tan, tanh
 */
#include "globals.h"

/* Include shared helper headers */
#include "ufloat.h"
#include "bfloat.h"

/* Math operations */
/**
Q0  OK  1490  acos  :  F  ->  G
G is the arc cosine of F.
*/
UFLOAT(acos_, "acos", acos)

/**
Q0  OK  1500  asin  :  F  ->  G
G is the arc sine of F.
*/
UFLOAT(asin_, "asin", asin)

/**
Q0  OK  1510  atan  :  F  ->  G
G is the arc tangent of F.
*/
UFLOAT(atan_, "atan", atan)

/**
Q0  OK  1520  atan2  :  F G  ->  H
H is the arc tangent of F / G.
*/
BFLOAT(atan2_, "atan2", atan2)

/**
Q0  OK  1540  cos  :  F  ->  G
G is the cosine of F.
*/
UFLOAT(cos_, "cos", cos)

/**
Q0  OK  1550  cosh  :  F  ->  G
G is the hyperbolic cosine of F.
*/
UFLOAT(cosh_, "cosh", cosh)

/**
Q0  OK  1560  exp  :  F  ->  G
G is e (2.718281828...) raised to the Fth power.
*/
UFLOAT(exp_, "exp", exp)

/**
Q0  OK  1600  log  :  F  ->  G
G is the natural logarithm of F.
*/
UFLOAT(log_, "log", log)

/**
Q0  OK  1610  log10  :  F  ->  G
G is the common logarithm of F.
*/
UFLOAT(log10_, "log10", log10)

/**
Q0  OK  1630  pow  :  F G  ->  H
H is F raised to the Gth power.
*/
BFLOAT(pow_, "pow", pow)

/**
Q0  OK  1640  sin  :  F  ->  G
G is the sine of F.
*/
UFLOAT(sin_, "sin", sin)

/**
Q0  OK  1650  sinh  :  F  ->  G
G is the hyperbolic sine of F.
*/
UFLOAT(sinh_, "sinh", sinh)

/**
Q0  OK  1660  sqrt  :  F  ->  G
G is the square root of F.
*/
UFLOAT(sqrt_, "sqrt", sqrt)

/**
Q0  OK  1670  tan  :  F  ->  G
G is the tangent of F.
*/
UFLOAT(tan_, "tan", tan)

/**
Q0  OK  1680  tanh  :  F  ->  G
G is the hyperbolic tangent of F.
*/
UFLOAT(tanh_, "tanh", tanh)

