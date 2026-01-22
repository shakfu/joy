/*
 *  module  : env_types.h
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Extracted subsystem types from Env struct.
 *  Part of Phase 1.2 architecture improvements.
 */
#ifndef ENV_TYPES_H
#define ENV_TYPES_H

#include <stdio.h>  /* for FILE */

/* Forward declarations */
struct Env;
typedef struct Env* pEnv;

/*
 * EnvConfig - Configuration flags
 *
 * Groups all runtime configuration options that control interpreter behavior.
 * These are typically set at startup or via commands like 'setautoput'.
 */
typedef struct EnvConfig {
    unsigned char autoput;        /* auto-print top of stack after each line */
    unsigned char autoput_set;    /* whether autoput was explicitly set */
    unsigned char echoflag;       /* echo input lines */
    unsigned char tracegc;        /* trace garbage collection */
    unsigned char undeferror;     /* report undefined symbol errors */
    unsigned char undeferror_set; /* whether undeferror was explicitly set */
    unsigned char debugging;      /* debugging mode enabled */
    unsigned char ignore;         /* ignore errors during include */
    unsigned char overwrite;      /* warn on symbol redefinition */
    unsigned char printing;       /* currently printing output */
    unsigned char inlining;       /* inline expansion enabled */
    signed char bytecoding;       /* bytecode mode (BDW only) */
    signed char compiling;        /* compiling mode (BDW only) */
} EnvConfig;

/*
 * EnvStats - Runtime statistics
 *
 * Tracks memory usage, GC collections, and operation counts.
 * Used for debugging and performance analysis.
 */
typedef struct EnvStats {
    double nodes;    /* current node count */
    double avail;    /* available memory */
    double collect;  /* GC collection count */
    double calls;    /* function call count */
    double opers;    /* operation count */
} EnvStats;

/*
 * EnvScanner - Scanner/lexer state
 *
 * All state related to tokenizing input, including the input file stack,
 * line buffering, and current position tracking.
 */
typedef struct EnvScanner {
    FILE* srcfile;                        /* current input file */
    char* srcfilename;                    /* name of current input file */
    int linenum;                          /* current line number */
    int linepos;                          /* position in current line */
    char linebuf[256];                    /* buffered input line (INPLINEMAX + 1) */
    struct {
        FILE* fp;
        int line;
        char name[15];                    /* FILENAMEMAX + 1 */
    } infile[10];                         /* include file stack (INPSTACKMAX) */
    int ilevel;                           /* index in infile stack (-1 = empty) */
    int startnum;                         /* line number of token start */
    int startpos;                         /* position of token start */
    int endpos;                           /* position of token end */
} EnvScanner;

/*
 * EnvError - Error handling state
 *
 * Error recovery points and last error information.
 */
typedef struct EnvError {
    char message[256];  /* last error message */
    int line;           /* line number of error */
    int column;         /* column of error */
} EnvError;

#endif /* ENV_TYPES_H */
