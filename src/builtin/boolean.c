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
#include "individual/all.c"
#include "individual/false.c"
#include "individual/some.c"
#include "individual/true.c"
