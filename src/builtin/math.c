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
#include "individual/acos.c"
#include "individual/asin.c"
#include "individual/atan.c"
#include "individual/atan2.c"
#include "individual/cos.c"
#include "individual/cosh.c"
#include "individual/exp.c"
#include "individual/log.c"
#include "individual/log10.c"
#include "individual/pow.c"
#include "individual/sin.c"
#include "individual/sinh.c"
#include "individual/sqrt.c"
#include "individual/tan.c"
#include "individual/tanh.c"
