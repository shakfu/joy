/*
 *  module  : io.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped I/O builtins: fclose, feof, ferror, fflush, fgetch, fgets,
 *  finclude, fopen, format, formatf, fput, fputch, fputchars, fputstring,
 *  fread, fremove, frename, fseek, ftell, fwrite, get, put, putch, putchars,
 *  stderr, stdin, stdout
 */
#include "globals.h"

/* Include shared helper headers */
#include "fileget.h"
#include "push.h"
#include "usetop.h"

/* I/O operations */
#include "individual/fclose.c"
#include "individual/feof.c"
#include "individual/ferror.c"
#include "individual/fflush.c"
#include "individual/fgetch.c"
#include "individual/fgets.c"
#include "individual/finclude.c"
#include "individual/fopen.c"
#include "individual/format.c"
#include "individual/formatf.c"
#include "individual/fput.c"
#include "individual/fputch.c"
#include "individual/fputchars.c"
#include "individual/fputstring.c"
#include "individual/fread.c"
#include "individual/fremove.c"
#include "individual/frename.c"
#include "individual/fseek.c"
#include "individual/ftell.c"
#include "individual/fwrite.c"
#include "individual/get.c"
#include "individual/put.c"
#include "individual/putch.c"
#include "individual/putchars.c"
#include "individual/stderr.c"
#include "individual/stdin.c"
#include "individual/stdout.c"
