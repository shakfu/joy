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

/* I/O operations */
/**
Q0  OK  1830  fclose  :  S  ->
[FOREIGN] Stream S is closed and removed from the stack.
*/
void fclose_(pEnv env)
{
    ONEPARAM("fclose");
    ISFILE("fclose");
    fclose(nodevalue(env->stck).fil);
    POP(env->stck);
}

/**
Q0  OK  1840  feof  :  S  ->  S B
[FOREIGN] B is the end-of-file status of stream S.
*/
FILEGET(feof_, "feof", BOOLEAN_NEWNODE, feof(nodevalue(env->stck).fil))

/**
Q0  OK  1850  ferror  :  S  ->  S B
[FOREIGN] B is the error status of stream S.
*/
FILEGET(ferror_, "ferror", BOOLEAN_NEWNODE, ferror(nodevalue(env->stck).fil))

/**
Q0  OK  1860  fflush  :  S  ->  S
[FOREIGN] Flush stream S, forcing all buffered output to be written.
*/
void fflush_(pEnv env)
{
    ONEPARAM("fflush");
    ISFILE("fflush");
    fflush(nodevalue(env->stck).fil);
}

/**
Q0  OK  1870  fgetch  :  S  ->  S C
[FOREIGN] C is the next available character from stream S.
*/
FILEGET(fgetch_, "fgetch", CHAR_NEWNODE, getc(nodevalue(env->stck).fil))

/**
Q0  OK  1880  fgets  :  S  ->  S L
[FOREIGN] L is the next available line (as a string) from stream S.
*/
void fgets_(pEnv env)
{
    char* buf;
#ifdef NOBDW
    char* tmp;
#endif
    size_t leng, size = INPLINEMAX;

    ONEPARAM("fgets");
    ISFILE("fgets");
#ifdef NOBDW
    buf = malloc(size);
#else
    buf = GC_malloc_atomic(size);
#endif
    buf[leng = 0] = 0;
    while (fgets(buf + leng, size - leng, nodevalue(env->stck).fil)) {
        if ((leng = strlen(buf)) > 0 && buf[leng - 1] == '\n')
            break;
#ifdef NOBDW
        if ((tmp = realloc(buf, size <<= 1)) == 0)
            break; /* LCOV_EXCL_LINE */
        buf = tmp;
#else
        buf = GC_realloc(buf, size <<= 1);
#endif
    }
    NULLARY(STRING_NEWNODE, buf);
#ifdef NOBDW
    free(buf);
#endif
}

/**
Q0  OK  3170  finclude  :  S  ->  F ...
[FOREIGN] Reads Joy source code from stream S and pushes it onto stack.
*/
void finclude_(pEnv env)
{
    char* str;

    ONEPARAM("finclude");
    STRING("finclude");
    str = GETSTRING(env->stck); /* read file name */
    POP(env->stck);             /* remove file name from stack */
    if (include(env, str))      /* include new file */
        return;
    env->finclude_busy = 1; /* tell scanner about finclude */
    if (setjmp(env->finclude))
        env->finclude_busy = 0; /* done with finclude */
    else
        while (1) /* read all factors from file */
            get_(env);
}

/**
Q0  OK  1890  fopen  :  P M  ->  S
[FOREIGN] The file system object with pathname P is opened with mode M
(r, w, a, etc.) and stream object S is pushed; if the open fails, file:NULL
is pushed.
*/
void fopen_(pEnv env)
{
    char *path, *mode;

    TWOPARAMS("fopen");
    STRING("fopen");
    STRING2("fopen");
    path = GETSTRING(nextnode1(env->stck));
    mode = GETSTRING(env->stck);
    BINARY(FILE_NEWNODE, fopen(path, mode));
}

/**
Q0  OK  1760  format  :  N C I J  ->  S
S is the formatted version of N in mode C
('d or 'i = decimal, 'o = octal, 'x or
'X = hex with lower or upper case letters)
with maximum width I and minimum width J.
*/
void format_(pEnv env)
{
    size_t leng;
    int width, prec;
    char spec, format[MAXNUM], *result;

    FOURPARAMS("format");
    INTEGERS2("format");
    prec = nodevalue(env->stck).num;
    POP(env->stck);
    width = nodevalue(env->stck).num;
    POP(env->stck);
    CHARACTER("format");
    spec = (char)nodevalue(env->stck).num;
    POP(env->stck);
    CHECKFORMAT(spec, "format");
    strcpy(format, "%*.*lld");
    format[6] = spec;
    NUMERICTYPE("format");
    leng = snprintf(0, 0, format, width, prec, nodevalue(env->stck).num) + 1;
#ifdef NOBDW
    result = malloc(leng);
#else
    result = GC_malloc_atomic(leng);
#endif
    snprintf(result, leng, format, width, prec, nodevalue(env->stck).num);
    UNARY(STRING_NEWNODE, result);
#ifdef NOBDW
    free(result);
#endif
}

/**
Q0  OK  1770  formatf  :  F C I J  ->  S
S is the formatted version of F in mode C
('e or 'E = exponential, 'f = fractional,
'g or G = general with lower or upper case letters)
with maximum width I and precision J.
*/
void formatf_(pEnv env)
{
    size_t leng;
    int width, prec;
    char spec, format[MAXNUM], *result;

    FOURPARAMS("formatf");
    INTEGERS2("formatf");
    prec = nodevalue(env->stck).num;
    POP(env->stck);
    width = nodevalue(env->stck).num;
    POP(env->stck);
    CHARACTER("formatf");
    spec = (char)nodevalue(env->stck).num;
    POP(env->stck);
    CHECKFORMATF(spec, "formatf");
    strcpy(format, "%*.*g");
    format[4] = spec;
    FLOAT("formatf");
    leng = snprintf(0, 0, format, width, prec, nodevalue(env->stck).dbl) + 1;
#ifdef NOBDW
    result = malloc(leng);
#else
    result = GC_malloc_atomic(leng);
#endif
    snprintf(result, leng, format, width, prec, nodevalue(env->stck).dbl);
    UNARY(STRING_NEWNODE, result);
#ifdef NOBDW
    free(result);
#endif
}

/**
Q0  OK  1940  fput  :  S X  ->  S
[FOREIGN] Writes X to stream S, pops X off stack.
*/
void fput_(pEnv env)
{
    FILE* fp;
    Index node;

    TWOPARAMS("fput");
    node = env->stck;
    POP(env->stck);
    ISFILE("fput");
    fp = nodevalue(env->stck).fil;
    writefactor(env, node, fp);
}

/**
Q0  OK  1950  fputch  :  S C  ->  S
[FOREIGN] The character C is written to the current position of stream S.
*/
void fputch_(pEnv env)
{
    int ch;

    TWOPARAMS("fputch");
    NUMERICTYPE("fputch");
    ch = nodevalue(env->stck).num;
    POP(env->stck);
    ISFILE("fputch");
    putc(ch, nodevalue(env->stck).fil);
}

/**
Q0  OK  1960  fputchars  :  S "abc.."  ->  S
[FOREIGN] The string abc.. (no quotes) is written to the current position of
stream S.
*/
void fputchars_(pEnv env) /* suggested by Heiko Kuhrt, as "fputstring_" */
{
    FILE* fp;
    char* str;

    TWOPARAMS("fputchars");
    STRING("fputchars");
    str = GETSTRING(env->stck);
    POP(env->stck);
    ISFILE("fputchars");
    fp = nodevalue(env->stck).fil;
    fputs(str, fp);
}

/**
Q0  OK  1970  fputstring  :  S "abc.."  ->  S
[FOREIGN] == fputchars, as a temporary alternative.
*/
void fputstring_(pEnv env) { fputchars_(env); }

/**
Q0  OK  1900  fread  :  S I  ->  S L
[FOREIGN] I bytes are read from the current position of stream S
and returned as a list of I integers.
*/
void fread_(pEnv env)
{
    int count;
    unsigned char* buf;

    TWOPARAMS("fread");
    INTEGER("fread");
    count = nodevalue(env->stck).num;
    POP(env->stck);
    ISFILE("fread");
    buf = malloc(count);
    env->dump1 = LIST_NEWNODE(0, env->dump1);
    for (count = fread(buf, 1, count, nodevalue(env->stck).fil) - 1;
         count >= 0; count--)
        DMP1 = INTEGER_NEWNODE(buf[count], DMP1);
    NULLARY(LIST_NEWNODE, DMP1);
    POP(env->dump1);
    free(buf);
}

/**
Q0  OK  1920  fremove  :  P  ->  B
[FOREIGN] The file system object with pathname P is removed from the file
system. B is a boolean indicating success or failure.
*/
void fremove_(pEnv env)
{
    char* str;

    ONEPARAM("fremove");
    STRING("fremove");
    str = GETSTRING(env->stck);
    UNARY(BOOLEAN_NEWNODE, !remove(str));
}

/**
Q0  OK  1930  frename  :  P1 P2  ->  B
[FOREIGN] The file system object with pathname P1 is renamed to P2.
B is a boolean indicating success or failure.
*/
void frename_(pEnv env)
{
    char *p1, *p2;

    TWOPARAMS("frename");
    STRING("frename");
    STRING2("frename");
    p1 = GETSTRING(nextnode1(env->stck));
    p2 = GETSTRING(env->stck);
    BINARY(BOOLEAN_NEWNODE, !rename(p1, p2));
}

/**
Q0  OK  1980  fseek  :  S P W  ->  S B
[FOREIGN] Stream S is repositioned to position P relative to whence-point W,
where W = 0, 1, 2 for beginning, current position, end respectively.
*/
void fseek_(pEnv env)
{
    int whence;
    int64_t pos;

    THREEPARAMS("fseek");
    INTEGERS2("fseek");
    whence = nodevalue(env->stck).num;
    POP(env->stck);
    pos = nodevalue(env->stck).num;
    POP(env->stck);
    ISFILE("fseek");
    NULLARY(BOOLEAN_NEWNODE, !!fseek(nodevalue(env->stck).fil, pos, whence));
}

/**
Q0  OK  1990  ftell  :  S  ->  S I
[FOREIGN] I is the current position of stream S.
*/
FILEGET(ftell_, "ftell", INTEGER_NEWNODE, ftell(nodevalue(env->stck).fil))

/**
Q0  OK  1910  fwrite  :  S L  ->  S
[FOREIGN] A list of integers are written as bytes to the current position of
stream S.
*/
void fwrite_(pEnv env)
{
    int i;
    Index n;
    unsigned char* buf;

    TWOPARAMS("fwrite");
    LIST("fwrite");
    /* check that each member of the list is numeric; also count the length */
    for (n = nodevalue(env->stck).lis, i = 0; n; n = nextnode1(n), i++)
        CHECKNUMERIC(n, "fwrite");
#ifdef NOBDW
    buf = malloc(i);
#else
    buf = GC_malloc_atomic(i);
#endif
    /* copy the list of integers to the character array; truncating integers */
    for (n = nodevalue(env->stck).lis, i = 0; n; n = nextnode1(n), i++)
        buf[i] = (unsigned char)nodevalue(n).num;
    POP(env->stck);
    ISFILE("fwrite");
    fwrite(buf, i, 1, nodevalue(env->stck).fil);
#ifdef NOBDW
    free(buf);
#endif
}

/**
Q0  POSTPONE  3070  get  :  ->  F
[IMPURE] Reads a factor from input and pushes it onto stack.
*/
void get_(pEnv env)
{
    int ch, rv;

    ch = getch(env);
    ch = getsym(env, ch);
    ch = readfactor(env, ch, &rv);
    ungetch(env, ch);
}

/**
Q0  IGNORE_POP  3080  put  :  X  ->
[IMPURE] Writes X to output, pops X off stack.
*/
USETOP(put_, "put", ONEPARAM, writefactor(env, env->stck, stdout))

/**
Q0  IGNORE_POP  3090  putch  :  N  ->
[IMPURE] N : numeric, writes character whose ASCII is N.
*/
USETOP(putch_, "putch", NUMERICTYPE,
       printf("%c", (int)nodevalue(env->stck).num))

/**
Q0  IGNORE_POP  3100  putchars  :  "abc.."  ->
[IMPURE] Writes abc.. (without quotes)
*/
USETOP(putchars_, "putchars", STRING,
       printf("%s", (char*)&nodevalue(env->stck)))

/**
Q0  IMMEDIATE  1190  stderr  :  ->  S
[FOREIGN] Pushes the standard error stream.
*/
PUSH(stderr_, FILE_NEWNODE, stderr)

/**
Q0  IMMEDIATE  1170  stdin  :  ->  S
[FOREIGN] Pushes the standard input stream.
*/
PUSH(stdin_, FILE_NEWNODE, stdin)

/**
Q0  IMMEDIATE  1180  stdout  :  ->  S
[FOREIGN] Pushes the standard output stream.
*/
PUSH(stdout_, FILE_NEWNODE, stdout)

