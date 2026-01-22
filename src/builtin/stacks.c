/*
 *  module  : stacks.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped stack builtins: dup, dupd, id, over, pick, pop, popd, rolldown,
 *  rolldownd, rollup, rollupd, rotate, rotated, stack, swap, swapd, unstack
 */
#include "globals.h"

/* Include shared helper headers */
#include "dipped.h"

/* Stack operations */
#include "individual/dup.c"
#include "individual/dupd.c"
#include "individual/id.c"
#include "individual/over.c"
#include "individual/pick.c"
#include "individual/pop.c"
#include "individual/popd.c"
#include "individual/rolldown.c"
#include "individual/rolldownd.c"
#include "individual/rollup.c"
#include "individual/rollupd.c"
#include "individual/rotate.c"
#include "individual/rotated.c"
#include "individual/stack.c"
#include "individual/swap.c"
#include "individual/swapd.c"
#include "individual/unstack.c"
