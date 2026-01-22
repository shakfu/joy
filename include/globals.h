/* FILE: globals.h */
/*
 *  module  : globals.h
 *  version : 1.121
 *  date    : 01/14/25
 */
#ifndef GLOBALS_H
#define GLOBALS_H

#ifdef MALLOC_DEBUG
#include "rmalloc.h"
#endif

/* #define USE_KHASHL */

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Under Linux overcommit_memory can be turned off (value 2), or ulimit can be
 * set and that way, malloc can return 0. The return value of GC_malloc is not
 * tested, because, although BDW can announce that it will return NULL, it
 * never does. This replaces the _MSC_VER-only checks with portable checking.
 */
#define TEST_MALLOC_RETURN

/*
 * A call to system() can be disabled for security. Set this to allow shell
 * escape commands. When not defined, system() calls are blocked.
 * Note: The fork already has command_is_safe() validation in scan.c which
 * provides additional protection even when this is enabled.
 */
#define ALLOW_SYSTEM_CALLS

/*
 * Certain compilers are likely to compile for the Windows platform and that
 * means that WINDOWS can be set. Other compilers need to set this explicitly,
 * if so desired.
 */
#if defined(_MSC_VER) || defined(__MINGW64_VERSION_MAJOR) || defined(__TINYC__)
#define WINDOWS
#endif

#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <io.h>      /* also import deprecated POSIX names */
#include <windows.h> /* pollute name space as much as possible */
#ifdef __TINYC__
#define strtoll _strtoi64 /* tcc 0.9.27 lacks strtoll */
#endif
#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4996)
#define kh_packed /* forget about __attribute__ ((packed)) */
#endif
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifndef NOBDW
#ifdef _MSC_VER
#include "gc-8.2.8/include/gc.h"
#else
#include <gc.h>
#endif
#else
#include "gc.h"
#endif
#include "kvec.h"
#ifdef USE_KHASHL
#include "khashl.h"
#else
#include "khash.h"
#endif

/*
 * Node access macros (legacy interface)
 *
 * IMPORTANT: These macros require 'pEnv env' to be in scope (NOBDW mode).
 *
 * These macros provide access to node fields. For new code, prefer the
 * inline accessor functions (node_type, node_value, node_next) defined
 * later in this file, which have explicit env parameters.
 *
 * @requires: env in scope (NOBDW mode only)
 * @param n/p: Index (node pointer/index) to access
 *
 * Macros:
 *   nodetype(n)  - Get node's type/opcode (Operator)
 *   nodevalue(n) - Get node's value union (Types)
 *   nodeleng(n)  - Get string length (NOBDW only)
 *   nextnode1(n) - Get next pointer (1 hop)
 *   nextnode2-5  - Get next pointer (2-5 hops)
 */
#ifdef NOBDW
#define nodetype(n) env->memory[n].op
#define nodeleng(n) env->memory[n].len
#define nodevalue(n) env->memory[n].u
#define nextnode1(n) env->memory[n].next
#define nextnode2(n) env->memory[nextnode1(n)].next
#define nextnode3(n) env->memory[nextnode2(n)].next
#define nextnode4(n) env->memory[nextnode3(n)].next
#define nextnode5(n) env->memory[nextnode4(n)].next
#else
#define nodetype(p) (p)->op
#define nodevalue(p) (p)->u
#define nextnode1(p) (p)->next
#define nextnode2(p) (nextnode1(p))->next
#define nextnode3(p) (nextnode2(p))->next
#define nextnode4(p) (nextnode3(p))->next
#define nextnode5(p) (nextnode4(p))->next
#ifdef TRACEGC
#undef TRACEGC
#endif
#endif

#include "macros.h"

/* settings for cflags */
#define IS_ACTIVE 1  /* prevent recursion */
#define IS_USED 2    /* multiple inlining */
#define IS_PRINTED 4 /* print of contents */

/* configure			*/
#define SHELLESCAPE '$'
#define INPSTACKMAX 10
#define INPLINEMAX 255
#define BUFFERMAX 80 /* smaller buffer */
#define HELPLINEMAX 72
#define MAXNUM 40 /* even smaller buffer */
#define FILENAMEMAX 14
#define DISPLAYMAX 10 /* nesting in HIDE & MODULE */
#define INIECHOFLAG 0
#define INIAUTOPUT 1
#define INITRACEGC 1
#define INIUNDEFERROR 0
#define INIWARNING 1

/* installation dependent	*/
#define SETSIZE (int)(CHAR_BIT * sizeof(uint64_t)) /* from limits.h */
#define MAXINT_ INT64_MAX                          /* from stdint.h */

/* symbols from getsym		*/
enum {
    ILLEGAL_,
    COPIED_,
    USR_,
    ANON_FUNCT_,
    BOOLEAN_,
    CHAR_,
    INTEGER_,
    SET_,
    STRING_,
    LIST_,
    FLOAT_,
    FILE_,
    BIGNUM_,

    LIST_PRIME_,

    LIBRA,
    EQDEF,
    HIDE,
    IN__,
    MODULE_,
    PRIVATE,
    PUBLIC,
    CONST_
};

typedef enum {
    OK,
    IGNORE_OK,
    IGNORE_PUSH,
    IGNORE_POP,
    IMMEDIATE,
    POSTPONE
} Flags;

typedef enum { ABORT_NONE, ABORT_RETRY, ABORT_QUIT } Abort;

/* types			*/
typedef unsigned char Operator; /* opcode / datatype */

typedef struct Env* pEnv;

typedef void (*proc_t)(pEnv); /* procedure */

#ifdef NOBDW
typedef unsigned Index;
#else
typedef struct Node* Index;
#endif

typedef union {
    int64_t num;  /* USR, BOOLEAN, CHAR, INTEGER */
    proc_t proc;  /* ANON_FUNCT */
    uint64_t set; /* SET */
    char* str;    /* STRING */
    Index lis;    /* LIST */
    double dbl;   /* FLOAT */
    FILE* fil;    /* FILE */
    int ent;      /* SYMBOL */
} Types;

#ifdef NOBDW
typedef struct Node {
    unsigned op : 4, len : 28; /* length of string */
    Index next;
    Types u;
} Node;
#else
typedef struct Node {
    Operator op;
    Index next;
    Types u;
} Node;
#endif

typedef struct Token {
    Operator op;
    int x, y, pos;
    Types u;
} Token;

typedef struct Entry {
    char* name;
    unsigned char is_user, flags, is_ok, is_root, is_last, qcode, nofun,
        cflags;
    union {
        Index body;
        proc_t proc;
    } u;
} Entry;

#ifdef USE_KHASHL
KHASHL_MAP_INIT(KH_LOCAL, symtab_t, symtab, const char*, int, kh_hash_str,
                kh_eq_str)
KHASHL_MAP_INIT(KH_LOCAL, funtab_t, funtab, uint64_t, int, kh_hash_uint64,
                kh_eq_generic)
#else
KHASH_MAP_INIT_STR(Symtab, int)
KHASH_MAP_INIT_INT64(Funtab, int)
#endif

/*
 * Env subsystem types (Phase 1.2 architecture improvements)
 *
 * These structs group related fields from Env for better organization.
 */

/* EnvError - Error state */
typedef struct EnvError {
    char message[256];  /* last error message */
    int line;           /* line number of error */
    int column;         /* column of error */
} EnvError;

/* EnvStats - Runtime statistics */
typedef struct EnvStats {
    double nodes;    /* current node count */
    double avail;    /* available memory */
    double collect;  /* GC collection count */
    double calls;    /* function call count */
    double opers;    /* operation count */
} EnvStats;

/* EnvConfig - Configuration flags */
typedef struct EnvConfig {
    unsigned char autoput;        /* auto-print top of stack after each line */
    unsigned char autoput_set;    /* whether autoput was explicitly set */
    unsigned char echoflag;       /* echo input lines */
    unsigned char tracegc;        /* trace garbage collection */
    unsigned char undeferror;     /* report undefined symbol errors */
    unsigned char undeferror_set; /* whether undeferror was explicitly set */
    unsigned char debugging;      /* debugging mode enabled */
    unsigned char overwrite;      /* warn on symbol redefinition */
    unsigned char inlining;       /* inline expansion enabled */
} EnvConfig;

/* EnvScanner - Scanner/lexer state */
typedef struct EnvScanner {
    FILE* srcfile;                        /* current input file */
    char* srcfilename;                    /* name of current input file */
    int linenum;                          /* current line number */
    int linepos;                          /* position in current line */
    char linebuf[INPLINEMAX + 1];         /* buffered input line */
    struct {
        FILE* fp;
        int line;
        char name[FILENAMEMAX + 1];
    } infile[INPSTACKMAX];                /* include file stack */
    int ilevel;                           /* index in infile stack (-1 = empty) */
    int startnum;                         /* line number of token start */
    int startpos;                         /* position of token start */
    int endpos;                           /* position of token end */
    Operator sym;                         /* current symbol */
} EnvScanner;

typedef struct Env {
    jmp_buf error_jmp; /* error recovery point */
    jmp_buf finclude;  /* return point in finclude */
    EnvError error;    /* error state (message, line, column) */
    EnvStats stats;    /* runtime statistics */
    double dbl; /* numerics */
    int64_t num;
    char* str;                 /* string */
    clock_t startclock;        /* main */
    char** g_argv;             /* command line */
    char* filename;            /* first include file */
    char* homedir;             /* HOME or HOMEPATH */
    char* mod_name;            /* name of module */
    vector(char*) * pathnames; /* pathnames to be searched when including */
    vector(char) * string;     /* value */
    vector(char) * pushback;   /* push back buffer */
    vector(Token) * tokens;    /* read ahead table */
    vector(Entry) * symtab;    /* symbol table */
#ifdef USE_KHASHL
    symtab_t* hash; /* hash tables that index the symbol table */
    funtab_t* prim;
#else
    khash_t(Symtab) * hash;
    khash_t(Funtab) * prim;
#endif
    Types bucket; /* used by NEWNODE defines */
#ifdef NOBDW
    clock_t gc_clock;
    Node* memory;       /* dynamic memory */
    Node* old_memory;   /* backup during GC (was static in utils.c) */
    Node* parent_memory; /* parent's memory for symbol body lookup in parallel contexts */
    Index conts, dump, dump1, dump2, dump3, dump4, dump5, inits;
    Index mem_low;      /* start of definition space (was global in utils.c) */
    Index memoryindex;  /* next free node index (was global in utils.c) */
    size_t memorymax;   /* total capacity of memory array (was static in utils.c) */
    char* stack_bottom; /* bottom of C stack for this context (was global) */
    GC_Context* gc_ctx; /* per-context conservative GC (Phase 3) */
#endif
    Index prog, stck;
#ifdef COMPILER
    FILE *declfp, *outfp;
#endif
    int g_argc; /* command line */
    int hide_stack[DISPLAYMAX];
    struct {
        char* name;
        int hide;
    } module_stack[DISPLAYMAX];
    EnvConfig config;    /* configuration flags */
    EnvScanner scanner;  /* scanner/lexer state */
    /* Runtime state flags (not configuration) */
    unsigned char ignore;         /* ignore errors during include */
    unsigned char printing;       /* currently printing output */
    unsigned char finclude_busy;  /* finclude is active */
    unsigned char flibrary_busy;  /* loading library */
    unsigned char variable_busy;  /* variable lookup in progress */
    signed char bytecoding;       /* bytecode mode (BDW only) */
    signed char compiling;        /* compiling mode (BDW only) */
    /* I/O callbacks for embedding (Phase 4) */
    struct {
        void* user_data;
        int   (*read_char)(void* user_data);
        void  (*write_char)(void* user_data, int ch);
        void  (*write_string)(void* user_data, const char* s);
        void  (*on_error)(void* user_data, int code, const char* msg,
                          const char* filename, int line, int column);
    } io;
} Env;

/*
 * Inline node accessors
 *
 * Type-safe functions for accessing node fields with explicit env parameter.
 * These complement the legacy macros (nodetype, nodevalue, nextnode1, etc.)
 * which assume 'env' is in scope.
 *
 * Benefits over macros:
 * - Type checking at compile time
 * - Explicit env parameter (no hidden dependency)
 * - Better debugging (show in stack traces)
 * - IDE navigation and autocomplete work
 *
 * The macros are retained for backward compatibility and for use in
 * performance-critical code where the explicit parameter passing overhead
 * might matter (though compilers typically optimize this away).
 */

static inline Operator node_type(pEnv env, Index n)
{
#ifdef NOBDW
    return env->memory[n].op;
#else
    (void)env;
    return n->op;
#endif
}

static inline Types node_value(pEnv env, Index n)
{
#ifdef NOBDW
    return env->memory[n].u;
#else
    (void)env;
    return n->u;
#endif
}

static inline Index node_next(pEnv env, Index n)
{
#ifdef NOBDW
    return env->memory[n].next;
#else
    (void)env;
    return n->next;
#endif
}

#ifdef NOBDW
static inline unsigned node_leng(pEnv env, Index n)
{
    return env->memory[n].len;
}
#endif

/* Convenience accessor for getting the next node's next (2 hops) */
static inline Index node_next2(pEnv env, Index n)
{
    return node_next(env, node_next(env, n));
}

typedef struct table_t {
    proc_t proc;
    char* name;
} table_t;

/* GOOD REFS:
    005.133l H4732		A LISP interpreter in C
    Manna p139  recursive Ackermann SCHEMA

   OTHER DATA TYPES
    WORD = "ABCD" - up to four chars
    LIST of SETs of char [S0 S1 S2 S3]
        LISTS - binary tree [left right]
            " with info [info left right] "
    STRING of 32 chars = 32 * 8 bits = 256 bits = bigset
    CHAR = 2 HEX
    32 SET = 2 * 16SET
*/

/* Public procedures: */
/* main.c */
void fatal(char* str);
/* error.c */
void abortexecution_(pEnv env, int num);
/* interp.c */
void exec_term(pEnv env, Index n);
/* scan.c */
void inilinebuffer(pEnv env);
int getch(pEnv env);
void ungetch(pEnv env, int ch);
void error(pEnv env, char* str);
int include(pEnv env, char* name);
int getsym(pEnv env, int ch);
/* utils.c */
void set_push_int_env(pEnv env);
Index newnode(pEnv env, Operator o, Types u, Index r);
Index newnode2(pEnv env, Index n, Index r);
void mem_index(pEnv env);
void mem_max(pEnv env);
#ifdef NOBDW
void inimem1(pEnv env, int status);
void inimem2(pEnv env);
void printnode(pEnv env, Index p);
void gc_collect(pEnv env);
void ensure_capacity(pEnv env, int num);
char *check_strdup(char *str);
void *check_malloc(size_t leng);
#endif
/* error.c */
void execerror(pEnv env, char* message, char* op);
/* factor.c */
int readfactor(pEnv env, int ch, int* rv); /* read a JOY factor */
int readterm(pEnv env, int ch);
/* module.c */
void savemod(int* hide, int* modl, int* hcnt);
void undomod(int hide, int modl, int hcnt);
void initmod(pEnv env, char* name);
void initpriv(pEnv env);
void stoppriv(void);
void exitpriv(void);
void exitmod(void);
char* classify(pEnv env, char* name);
int qualify(pEnv env, char* name);
/* optable.c */
int tablesize(void);
char* nickname(int ch);
char* opername(int o);
int operindex(pEnv env, proc_t proc);
void inisymboltable(pEnv env); /* initialise */
void addsymbol(pEnv env, Entry ent, int index);
/* print.c */
void print(pEnv env);
/* repl.c */
void repl(pEnv env);
/* setraw.c */
void SetRaw(void);
void SetNormal(void);
/* symbol.c */
int lookup(pEnv env, char* name);
int enteratom(pEnv env, char* name);
int compound_def(pEnv env, int ch);
/* undefs.c */
void hide_inner_modules(pEnv env, int flag);
/* write.c */
void writefactor(pEnv env, Index n, FILE* fp);
void writeterm(pEnv env, Index n, FILE* fp);
#ifdef BYTECODE
/* bytecode.c */
void bytecode(pEnv env, Node* list);
void initbytes(pEnv env);
void exitbytes(pEnv env);
/* compeval.c */
void compeval(pEnv env, FILE* fp);
/* computil.c */
Node* reverse(Node* cur);
char* outputfile(char* inputfile, char* suffix);
/* dumpbyte.c */
void dumpbyte(pEnv env, FILE* fp);
/* optimize.c */
void optimize(pEnv env, FILE* fp);
/* readbyte.c */
void readbyte(pEnv env, FILE* fp);
unsigned char* readfile(FILE* fp);
#endif
#ifdef COMPILER
/* compiler.c */
void printnode(pEnv env, Node* node);
void printstack(pEnv env);
void compile(pEnv env, Node* node);
void initcompile(pEnv env);
void exitcompile(pEnv env);
/* computil.c */
Node* reverse(Node* cur);
char* outputfile(char* inputfile, char* suffix);
/* identify.c */
const char* identifier(const char* str);
const char* unidentify(const char* str);
/* instance.c */
int instance(pEnv env, char* name, int qcode);
/* outfiles.c */
void initout(void);
FILE* nextfile(void);
void closefile(FILE* fp);
void printout(FILE* fp);
void closeout(void);
/* readtemp.c */
int testtemp(char* file);
void readtemp(pEnv env, char* file, Node* nodes[], int found, int seqnr);
#endif
/* iolib.c - I/O abstraction layer */
int joy_getchar(pEnv env);
int joy_getc(pEnv env, FILE* fp);
void joy_putchar(pEnv env, int ch);
void joy_putc(pEnv env, int ch, FILE* fp);
void joy_fputs(pEnv env, const char* s, FILE* fp);
void joy_fwrite(pEnv env, const char* s, size_t len, FILE* fp);
void joy_fprintf(pEnv env, FILE* fp, const char* fmt, ...);
void joy_puts(pEnv env, const char* s);
void joy_printf(pEnv env, const char* fmt, ...);
void joy_flush(pEnv env);
void joy_report_error(pEnv env, int code, const char* msg);

/*
 * Context-aware GC allocation macros (Phase 3)
 *
 * These macros provide convenient access to per-context GC allocation.
 * In NOBDW mode, they use env->gc_ctx if available, otherwise fall back
 * to global GC. This allows code to work with both the public API
 * (joy_create) and the internal CLI which uses Env directly.
 *
 * Note: kvec.h macros still use global GC_malloc/GC_realloc directly.
 * Full context isolation for vectors requires additional work.
 */
#ifdef NOBDW
#define GC_CTX_MALLOC(env, size) \
    ((env)->gc_ctx ? gc_ctx_malloc((env)->gc_ctx, (size)) : GC_malloc(size))
#define GC_CTX_MALLOC_ATOMIC(env, size) \
    ((env)->gc_ctx ? gc_ctx_malloc_atomic((env)->gc_ctx, (size)) : GC_malloc_atomic(size))
#define GC_CTX_REALLOC(env, ptr, size) \
    ((env)->gc_ctx ? gc_ctx_realloc((env)->gc_ctx, (ptr), (size)) : GC_realloc((ptr), (size)))
#define GC_CTX_STRDUP(env, str) \
    ((env)->gc_ctx ? gc_ctx_strdup((env)->gc_ctx, (str)) : GC_strdup(str))
#else
#define GC_CTX_MALLOC(env, size)        GC_malloc(size)
#define GC_CTX_MALLOC_ATOMIC(env, size) GC_malloc_atomic(size)
#define GC_CTX_REALLOC(env, ptr, size)  GC_realloc((ptr), (size))
#define GC_CTX_STRDUP(env, str)         GC_strdup(str)
#endif

#endif
