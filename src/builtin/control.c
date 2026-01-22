/*
 *  module  : control.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped control flow builtins: branch, case, choice, cond, i, ifte,
 *  opcase, x
 */
#include "globals.h"

/* Include shared helper headers */
#include "boolean.h"
#include "decode.h"

/* Control flow operations */
#include "individual/branch.c"
#include "individual/case.c"
#include "individual/choice.c"
#include "individual/cond.c"
#include "individual/i.c"
#include "individual/ifte.c"
#include "individual/opcase.c"
#include "individual/x.c"
