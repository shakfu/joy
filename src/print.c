/*
 *  module  : print.c
 *  version : 1.2
 *  date    : 01/20/26
 */
#include "globals.h"

/*
 * print the stack according to the autoput settings.
 * Uses I/O abstraction for output when callbacks are set.
 */
void print(pEnv env)
{
    if (env->stck) {
        if (env->config.autoput == 2)
            writeterm(env, env->stck, stdout);
        else if (env->config.autoput == 1) {
            writefactor(env, env->stck, stdout);
            env->stck = nextnode1(env->stck);
        }
        if (env->config.autoput) {
            joy_putchar(env, '\n');
            joy_flush(env);
        }
    }
}
