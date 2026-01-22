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
#include "n_ary.h"

/* N-ary operations */
#include "individual/binary.c"
#include "individual/nullary.c"
#include "individual/ternary.c"
#include "individual/unary.c"
#include "individual/unary2.c"
#include "individual/unary3.c"
#include "individual/unary4.c"
