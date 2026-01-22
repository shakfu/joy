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
#include "individual/compare.c"
#include "individual/eql.c"
#include "individual/equal.c"
#include "individual/geql.c"
#include "individual/greater.c"
#include "individual/leql.c"
#include "individual/less.c"
#include "individual/neql.c"
#include "individual/sametype.c"
