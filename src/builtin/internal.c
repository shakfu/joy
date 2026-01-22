/*
 *  module  : internal.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped internal/debug builtins: __dump, __html_manual, __latex_manual,
 *  __manual_list, __memoryindex, __memorymax, __settracegc, __symtabindex,
 *  __symtabmax, _help, help, helpdetail, manual
 */
#include "globals.h"

/* Include shared helper headers */
#include "help.h"
#include "manual.h"

/* Internal operations */
#include "individual/__dump.c"
#include "individual/__html_manual.c"
#include "individual/__latex_manual.c"
#include "individual/__manual_list.c"
#include "individual/__memoryindex.c"
#include "individual/__memorymax.c"
#include "individual/__settracegc.c"
#include "individual/__symtabindex.c"
#include "individual/__symtabmax.c"
#include "individual/_help.c"
#include "individual/help.c"
#include "individual/helpdetail.c"
#include "individual/manual.c"
