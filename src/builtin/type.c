/*
 *  module  : type.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped type builtins: casting, char, file, float, ifchar, iffile,
 *  iffloat, ifinteger, iflist, iflogical, ifset, ifstring, integer, leaf,
 *  list, logical, set, string, typeof, user
 */
#include "globals.h"

/* Include shared helper headers */
#include "type.h"
#include "if_type.h"
#include "boolean.h"

/* Type operations */
#include "individual/casting.c"
#include "individual/char.c"
#include "individual/file.c"
#include "individual/float.c"
#include "individual/ifchar.c"
#include "individual/iffile.c"
#include "individual/iffloat.c"
#include "individual/ifinteger.c"
#include "individual/iflist.c"
#include "individual/iflogical.c"
#include "individual/ifset.c"
#include "individual/ifstring.c"
#include "individual/integer.c"
#include "individual/leaf.c"
#include "individual/list.c"
#include "individual/logical.c"
#include "individual/set.c"
#include "individual/string.c"
#include "individual/typeof.c"
#include "individual/user.c"
