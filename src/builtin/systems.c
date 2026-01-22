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

#include <sys/stat.h>
#include <stdlib.h>

/* Include shared helper headers */

/* Helper for rand_ */
static int joy_random(void)
{
#if defined(__APPLE__)
    return (int)arc4random();
#else
    return rand();
#endif
}

/* System operations */
/**
Q0  IGNORE_OK  3120  abort  :  ->
[IMPURE] Aborts execution of current Joy program, returns to Joy main cycle.
*/
void abort_(pEnv env) { abortexecution_(env, ABORT_RETRY); } /* LCOV_EXCL_LINE */

/**
Q0  OK  3050  argc  :  ->  I
[RUNTIME] Pushes the number of command line arguments.
This is equivalent to 'argv size'.
*/
PUSH(argc_, INTEGER_NEWNODE, env->g_argc)

/**
Q0  OK  3040  argv  :  ->  A
Creates an aggregate A containing the interpreter's command line arguments.
*/
void argv_(pEnv env)
{
    int i;
    Index temp;

    env->dump1 = LIST_NEWNODE(0, env->dump1);
    for (i = env->g_argc - 1; i >= 0; i--) {
        temp = STRING_NEWNODE(env->g_argv[i], DMP1);
        DMP1 = temp;
    }
    NULLARY(LIST_NEWNODE, DMP1);
    POP(env->dump1);
}

/**
Q0  OK  2190  body  :  U  ->  [P]
[SYMBOLS] Quotation [P] is the body of user-defined symbol U.
*/
void body_(pEnv env)
{
    ONEPARAM("body");
    USERDEF("body");
    UNARY(LIST_NEWNODE, vec_at(env->symtab, nodevalue(env->stck).ent).u.body);
}

/**
Q0  IGNORE_PUSH  1130  clock  :  ->  I
[IMPURE] Pushes the integer value of current CPU usage in milliseconds.
*/
PUSH(clock_, INTEGER_NEWNODE,
     ((clock() - env->startclock) * 1000 / CLOCKS_PER_SEC))

/**
Q0  OK  1080  conts  :  ->  [[P] [Q] ..]
Pushes current continuations. Buggy, do not use.
*/
void conts_(pEnv env)
{
    Index temp;

    env->bucket.lis = nextnode1(nodevalue(env->conts).lis);
    temp = newnode(env, LIST_, env->bucket, nextnode1(env->conts));
    NULLARY(LIST_NEWNODE, temp);
    /*
      PUSH(conts_, LIST_NEWNODE,
        LIST_NEWNODE(nextnode1(nodevalue(env->conts).lis),
      nextnode1(env->conts)))

      Code has unspecified behavior. Order of evaluation of function parameters
      or subexpressions is not defined, so if a value is used and modified in
      different places not separated by a sequence point constraining
      evaluation order, then the result of the expression is unspecified.
    */
}

/**
Q0  OK  3160  filetime  :  F  ->  T
[FOREIGN] T is the modification time of file F.
*/
void filetime_(pEnv env)
{
    FILE* fp;
    char* str;
    time_t mtime;     /* modification time */
    struct stat* buf; /* struct stat is big */

    ONEPARAM("filetime");
    STRING("filetime");
    str = GETSTRING(env->stck);
    mtime = 0;
    if ((fp = fopen(str, "r")) != 0) {
#ifdef NOBDW
        buf = malloc(sizeof(struct stat));
#else
        buf = GC_malloc_atomic(sizeof(struct stat));
#endif
        if (fstat(fileno(fp), buf) >= 0)
            mtime = buf->st_mtime;
#ifdef NOBDW
        free(buf);
#endif
        fclose(fp);
    }
    UNARY(INTEGER_NEWNODE, mtime);
}

/**
Q0  OK  3010  gc  :  ->
[IMPURE] Initiates garbage collection.
*/
void gc_(pEnv env) { gc_collect(env); }

/**
Q0  OK  3030  getenv  :  "variable"  ->  "value"
[RUNTIME] Retrieves the value of the environment variable "variable".
*/
void getenv_(pEnv env)
{
    char* str;

    ONEPARAM("getenv");
    STRING("getenv");
    str = GETSTRING(env->stck);
    if ((str = getenv(str)) == 0)
        str = "";
    UNARY(STRING_NEWNODE, str);
}

/**
Q0  OK  1710  gmtime  :  I  ->  T
Converts a time I into a list T representing universal time:
[year month day hour minute second isdst yearday weekday].
Month is 1 = January ... 12 = December;
isdst is false; weekday is 1 = Monday ... 7 = Sunday.
*/
UNMKTIME(gmtime_, "gmtime", gmtime)

/**
Q0  OK  3110  include  :  "filnam.ext"  ->
[SYMBOLS] Transfers input to file whose name is "filnam.ext".
On end-of-file returns to previous input file.
*/
void include_(pEnv env)
{
    char* str;

    ONEPARAM("include");
    STRING("include");
    str = GETSTRING(env->stck);
    if (include(env, str))
        execerror(env, "valid file name", "include");
    POP(env->stck);
}

/**
Q0  OK  2180  intern  :  "sym"  ->  sym
[SYMBOLS] Pushes the item whose name is "sym".
*/
void intern_(pEnv env)
{
    int index;
    Entry ent;
    char *ptr, *str;

    ONEPARAM("intern");
    STRING("intern");
    ptr = str = GETSTRING(env->stck);
    if (!strchr("\"#'().0123456789;[]{}", *ptr)) {
        if (*ptr == '-' && isdigit((int)ptr[1]))
            ;
        else
            for (++ptr; *ptr; ptr++)
                if (!isalnum((int)*ptr) && !strchr("-=_", *ptr))
                    break;
    }
    CHECKNAME(ptr, "intern");
    index = lookup(env, GC_CTX_STRDUP(env, str));
    ent = vec_at(env->symtab, index);
    if (ent.is_user)
        UNARY(USR_NEWNODE, index);
    else
        UNARY(ANON_FUNCT_NEWNODE, ent.u.proc);
}

/**
Q0  OK  1700  localtime  :  I  ->  T
Converts a time I into a list T representing local time:
[year month day hour minute second isdst yearday weekday].
Month is 1 = January ... 12 = December;
isdst is a Boolean flagging daylight savings/summer time;
weekday is 1 = Monday ... 7 = Sunday.
*/
UNMKTIME(localtime_, "localtime", localtime)

/**
Q0  OK  1720  mktime  :  T  ->  I
Converts a list T representing local time into a time I.
T is in the format generated by localtime.
*/
void mktime_(pEnv env)
{
    struct tm t;

    ONEPARAM("mktime");
    LIST("mktime");
    decode_time(env, &t);
    UNARY(INTEGER_NEWNODE, (int64_t)mktime(&t));
}

/**
Q0  OK  2170  name  :  sym  ->  "sym"
For operators and combinators, the string "sym" is the name of item sym,
for literals sym the result string is its type.
*/
void name_(pEnv env)
{
    int op;
    char* str;

    ONEPARAM("name");
    if ((op = nodetype(env->stck)) == USR_)
        str = vec_at(env->symtab, nodevalue(env->stck).ent).name;
    else if (op == ANON_FUNCT_)
        str = nickname(operindex(env, nodevalue(env->stck).proc));
    else
        str = opername(op);
    UNARY(STRING_NEWNODE, str);
}

/**
Q0  IGNORE_OK  3130  quit  :  ->
[IMPURE] Exit from Joy.
*/
void quit_(pEnv env) { abortexecution_(env, ABORT_QUIT); } /* LCOV_EXCL_LINE */

/**
Q0  IGNORE_PUSH  1150  rand  :  ->  I
[IMPURE] I is a random integer.
*/
PUSH(rand_, INTEGER_NEWNODE, joy_random())

/**
Q0  IGNORE_POP  1780  srand  :  I  ->
[IMPURE] Sets the random integer seed to integer I.
*/
USETOP(srand_, "srand", INTEGER, srand((unsigned)nodevalue(env->stck).num))

/**
Q0  OK  1730  strftime  :  T S1  ->  S2
Formats a list T in the format of localtime or gmtime
using string S1 and pushes the result S2.
*/
void strftime_(pEnv env)
{
    struct tm t;
    size_t leng;
    char *fmt, *result;

    TWOPARAMS("strftime");
    STRING("strftime");
    fmt = GETSTRING(env->stck);
    POP(env->stck);
    LIST("strftime");
    decode_time(env, &t);
    leng = BUFFERMAX;
#ifdef NOBDW
    result = malloc(leng + 1);
#else
    result = GC_malloc_atomic(leng + 1);
#endif
    strftime(result, leng, fmt, &t);
    UNARY(STRING_NEWNODE, result);
#ifdef NOBDW
    free(result);
#endif
}

/**
Q0  IGNORE_POP  3020  system  :  "command"  ->
[IMPURE] Escapes to shell, executes string "command".
The string may cause execution of another program.
When that has finished, the process returns to Joy.
*/
void system_(pEnv env)
{
#ifndef WINDOWS_S
    char* str;
#endif
    ONEPARAM("system");
    STRING("system");
#ifndef WINDOWS_S
    str = GETSTRING(env->stck);
    (void)system(str);
#endif
    POP(env->stck);
}

/**
Q0  IGNORE_PUSH  1140  time  :  ->  I
[IMPURE] Pushes the current time (in seconds since the Epoch).
*/
PUSH(time_, INTEGER_NEWNODE, (int64_t)time(0))

/**
Q0  OK  1110  undefs  :  ->  [..]
[SYMBOLS] Push a list of all undefined symbols in the current symbol table.
*/
void undefs_(pEnv env)
{
    int i, j;
    Entry ent;
    Index* my_dump;

    NULLARY(LIST_NEWNODE, 0);
    my_dump = &nodevalue(env->stck).lis;
    for (i = 0, j = vec_size(env->symtab); i < j; i++) {
        ent = vec_at(env->symtab, i);
        if (ent.name[0] && ent.name[0] != '_' && !ent.u.body) {
            *my_dump = STRING_NEWNODE(ent.name, 0);
            my_dump = &nextnode1(*my_dump);
        }
    }
}

