/*
 *  module  : sets.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped set builtins: and, has, in, not, or, set, xor
 */
#include "globals.h"

/* Include shared helper headers */
#include "andorxor.h"
#include "inhas.h"

/* Set operations */
#include "individual/and.c"
#include "individual/has.c"
#include "individual/in.c"
#include "individual/not.c"
#include "individual/or.c"
#include "individual/xor.c"

/* Type predicate */
#include "individual/set.c"
