/*
 *  module  : aggregate.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped aggregate/list builtins: assign, at, concat, cons, drop, enconcat,
 *  first, list, null, of, rest, size, small, split, swons, take, unassign,
 *  uncons, unswons
 */
#include "globals.h"

/* Include shared helper headers */
#include "cons_swons.h"
#include "of_at.h"

/* List operations */
#include "individual/assign.c"
#include "individual/at.c"
#include "individual/concat.c"
#include "individual/cons.c"
#include "individual/drop.c"
#include "individual/enconcat.c"
#include "individual/first.c"
#include "individual/null.c"
#include "individual/of.c"
#include "individual/rest.c"
#include "individual/size.c"
#include "individual/small.c"
#include "individual/split.c"
#include "individual/swons.c"
#include "individual/take.c"
#include "individual/unassign.c"
#include "individual/uncons.c"
#include "individual/unswons.c"

/* Type predicate */
#include "individual/list.c"
