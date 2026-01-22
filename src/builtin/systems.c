/*
 *  module  : systems.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped system builtins: abort, argc, argv, body, clock, conts, filetime,
 *  gc, getenv, gmtime, include, intern, localtime, mktime, name, quit, rand,
 *  srand, strftime, system, time, undefs
 */
#include "globals.h"

/* Include shared helper headers */
#include "unmktime.h"

/* System operations */
#include "individual/abort.c"
#include "individual/argc.c"
#include "individual/argv.c"
#include "individual/body.c"
#include "individual/clock.c"
#include "individual/conts.c"
#include "individual/filetime.c"
#include "individual/gc.c"
#include "individual/getenv.c"
#include "individual/gmtime.c"
#include "individual/include.c"
#include "individual/intern.c"
#include "individual/localtime.c"
#include "individual/mktime.c"
#include "individual/name.c"
#include "individual/quit.c"
#include "individual/rand.c"
#include "individual/srand.c"
#include "individual/strftime.c"
#include "individual/system.c"
#include "individual/time.c"
#include "individual/undefs.c"
