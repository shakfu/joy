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
#include "individual/abs.c"
#include "individual/ceil.c"
#include "individual/div.c"
#include "individual/divide.c"
#include "individual/floor.c"
#include "individual/frexp.c"
#include "individual/ldexp.c"
#include "individual/max.c"
#include "individual/min.c"
#include "individual/minus.c"
#include "individual/modf.c"
#include "individual/mul.c"
#include "individual/neg.c"
#include "individual/plus.c"
#include "individual/pred.c"
#include "individual/rem.c"
#include "individual/round.c"
#include "individual/sign.c"
#include "individual/succ.c"
#include "individual/trunc.c"
