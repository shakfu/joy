/*
 *  module  : error.c
 *  version : 1.3
 *  date    : 10/11/24
 */
#include "globals.h"

/*
 * print a runtime error to stderr and abort the execution of current program.
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
    fflush(stdout);
    {
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
                fwrite(buffer, 1, size - 1, stderr);
                free(buffer);
            }
        }
    }
    abortexecution_(ABORT_RETRY);
} /* LCOV_EXCL_LINE */
