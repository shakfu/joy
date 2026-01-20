/*
 *  module  : error.c
 *  version : 1.5
 *  date    : 01/20/26
 */
#include "globals.h"

/*
 * abort execution and restart reading from srcfile; the stack is not cleared.
 */
void abortexecution_(pEnv env, int num)
{
    fflush(stdin);
    longjmp(env->error_jmp, num);
}

/*
 * print a runtime error to stderr and abort the execution of current program.
 * Uses I/O abstraction for error output when callbacks are set.
 */
void execerror(pEnv env, char* message, char* op)
{
    int leng = 0;
    char *ptr, *str;
#ifdef COMPILER
    Entry ent;

    if (env->compiling > 0) {
        leng = lookup(env, op); /* locate in symbol table */
        if (leng < tablesize())
            op = nickname(leng);
        ent = vec_at(env->symtab, leng);
        ent.flags |= IS_USED;
        vec_at(env->symtab, leng) = ent;
        printstack(env);
        fprintf(env->outfp, "%s_(env);\n", op);
        return;
    }
#endif
    if ((ptr = strrchr(op, '/')) != 0)
        ptr++;
    else
        ptr = op;
    if ((str = strrchr(ptr, '.')) != 0 && str[1] == 'c')
        leng = str - ptr;
    else
        leng = strlen(ptr);

    /* Format error message */
    int needed = snprintf(NULL, 0,
                          "run time error: %s needed for %.*s\n", message,
                          leng, ptr);
    if (needed > 0) {
        size_t size = (size_t)needed + 1;
        char *buffer = malloc(size);
        if (buffer) {
            snprintf(buffer, size,
                     "run time error: %s needed for %.*s\n", message, leng,
                     ptr);
            /* Use I/O abstraction for error reporting */
            joy_report_error(env, ABORT_RETRY, buffer);
            free(buffer);
        }
    }
    abortexecution_(env, ABORT_RETRY);
} /* LCOV_EXCL_LINE */
