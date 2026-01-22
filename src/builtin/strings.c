/*
 *  module  : strings.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped string builtins: chr, ord, string, strtod, strtol
 */
#include "globals.h"

/* Include shared helper headers */
#include "ordchr.h"

/* String operations */
#include "individual/chr.c"
#include "individual/ord.c"
#include "individual/strtod.c"
#include "individual/strtol.c"

/* Type predicate */
#include "individual/string.c"
