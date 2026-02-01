/*
 *  module  : debugger.c
 *  version : 1.0
 *  date    : 02/01/26
 *
 *  Debugger builtins: debug-trace, debug-step, breakpoint,
 *  clear-breakpoints, show-breakpoints
 */
#include "globals.h"

/**
Q1  OK  3900  debug-trace\0debug_trace  :  [P]  ->  ...
Executes P with full execution tracing enabled.
*/
void debug_trace_(pEnv env)
{
    unsigned char saved;
    ONEPARAM("debug-trace");
    ONEQUOTE("debug-trace");
    SAVESTACK;
    POP(env->stck);
    saved = env->config.debugging;
    env->config.debugging = 2;
    exec_term(env, nodevalue(SAVED1).lis);
    env->config.debugging = saved;
    POP(env->dump);
}

/**
Q1  OK  3910  debug-step\0debug_step  :  [P]  ->  ...
Executes P in interactive stepping mode.
Commands: s=step, c=continue, q=quit
*/
void debug_step_(pEnv env)
{
    unsigned char saved;
    ONEPARAM("debug-step");
    ONEQUOTE("debug-step");
    SAVESTACK;
    POP(env->stck);
    saved = env->config.stepping;
    env->config.stepping = 1;
    exec_term(env, nodevalue(SAVED1).lis);
    env->config.stepping = saved;
    POP(env->dump);
}

/**
Q0  OK  3920  breakpoint  :  "name"  ->
Sets a breakpoint on the named symbol.
*/
void breakpoint_(pEnv env)
{
    int idx;
    char* name;
    ONEPARAM("breakpoint");
    STRING("breakpoint");
    name = GETSTRING(env->stck);
    idx = lookup(env, GC_strdup(name));
    if (idx == 0) {
        execerror(env, "defined symbol", "breakpoint");
        return;
    }
    if (!env->config.breakpoints)
        vec_init(env->config.breakpoints);
    /* Check not already set */
    {
        size_t bp_count = vec_size(env->config.breakpoints);
        for (size_t i = 0; i < bp_count; i++)
            if (vec_at(env->config.breakpoints, i) == idx) {
                POP(env->stck);
                return;
            }
    }
    vec_push(env->config.breakpoints, idx);
    POP(env->stck);
}

/**
Q0  OK  3930  clear-breakpoints\0clear_breakpoints  :  ->
Clears all breakpoints.
*/
void clear_breakpoints_(pEnv env)
{
    if (env->config.breakpoints)
        vec_setsize(env->config.breakpoints, 0);
}

/**
Q0  OK  3940  show-breakpoints\0show_breakpoints  :  ->  L
Pushes a list of breakpoint symbol names.
*/
void show_breakpoints_(pEnv env)
{
    Index list = 0;
    if (env->config.breakpoints) {
        for (int i = (int)vec_size(env->config.breakpoints) - 1; i >= 0; i--) {
            int idx = vec_at(env->config.breakpoints, i);
            Entry ent = vec_at(env->symtab, idx);
            list = STRING_NEWNODE(GC_strdup(ent.name), list);
        }
    }
    NULLARY(LIST_NEWNODE, list);
}
