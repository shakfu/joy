/*
 *  module  : internal.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped internal/debug builtins: __dump, __html_manual, __latex_manual,
 *  __manual_list, __memoryindex, __memorymax, __settracegc, __symtabindex,
 *  __symtabmax, _help, help, helpdetail, manual
 */
#include "globals.h"

/* Include shared helper headers */
#include "help.h"
#include "manual.h"

/* Internal operations */
/**
Q0  OK  1070  __dump  :  ->  [..]
[SYMBOLS] debugging only: pushes the dump as a list.
*/
PUSH(__dump_, INTEGER_NEWNODE, 0) /* variables */

/**
Q0  IGNORE_OK  2940  __html_manual  :  ->
[IMPURE] Writes this manual of all Joy primitives to output file in HTML style.
*/
void __html_manual_(pEnv env) { make_manual(1); }

/**
Q0  IGNORE_OK  2950  __latex_manual  :  ->
[IMPURE] Writes this manual of all Joy primitives to output file in Latex style
but without the head and tail.
*/
void __latex_manual_(pEnv env) { make_manual(2); }

/**
Q0  OK  2960  __manual_list  :  ->  L
Pushes a list L of lists (one per operator) of three documentation strings.
*/
void __manual_list_(pEnv env)
{
    int i;
    Index temp;

    i = sizeof(optable) / sizeof(optable[0]); /* find end */
    env->dump1 = LIST_NEWNODE(0, env->dump1);
    env->dump2 = LIST_NEWNODE(0, env->dump2);
    while (--i >= 0) { /* overshot */
        temp = STRING_NEWNODE(optable[i].messg2, 0);
        DMP1 = temp;
        temp = STRING_NEWNODE(optable[i].messg1, DMP1);
        DMP1 = temp;
        temp = STRING_NEWNODE(optable[i].name, DMP1);
        DMP1 = temp;
        temp = LIST_NEWNODE(DMP1, DMP2);
        DMP2 = temp;
    }
    env->stck = LIST_NEWNODE(DMP2, env->stck);
    POP(env->dump2);
    POP(env->dump1);
}

/**
Q0  IGNORE_PUSH  3060  __memoryindex  :  ->  I
[IMPURE] Pushes current value of memory.
*/
void __memoryindex_(pEnv env) { mem_index(env); }

/**
Q0  IGNORE_PUSH  1160  __memorymax  :  ->  I
[IMPURE] Pushes value of total size of memory.
*/
void __memorymax_(pEnv env) { mem_max(env); }

/**
Q0  OK  2970  __settracegc  :  I  ->
[IMPURE] Sets value of flag for tracing garbage collection to I (= 0..6).
*/
void __settracegc_(pEnv env)
{
    ONEPARAM("settracegc");
    NUMERICTYPE("settracegc");
    env->config.tracegc = nodevalue(env->stck).num;
#ifndef NOBDW
    if (env->config.tracegc) /* 0=enable bytecoding or compiling */
        ;
    /*
     * The flags are initially negative; when activating, they are made
     * positive.
     */
    else if (env->bytecoding)
        env->bytecoding = -env->bytecoding; /* LCOV_EXCL_LINE */
    else if (env->compiling)
        env->compiling = -env->compiling; /* LCOV_EXCL_LINE */
    else
        env->ignore = 0; /* disable ignore */
#endif
    POP(env->stck);
}

/**
Q0  OK  1060  __symtabindex  :  ->  I
[SYMBOLS] Pushes current size of the symbol table.
*/
PUSH(__symtabindex_, INTEGER_NEWNODE, vec_size(env->symtab))

/**
Q0  OK  1050  __symtabmax  :  ->  I
[SYMBOLS] Pushes value of maximum size of the symbol table.
*/
PUSH(__symtabmax_, INTEGER_NEWNODE, vec_max(env->symtab))

/**
Q0  IGNORE_OK  2910  _help  :  ->
[IMPURE] Lists all hidden symbols in library and then all hidden builtin
symbols.
*/
HELP(_help_, !=)

/**
Q0  IGNORE_OK  2900  help  :  ->
[IMPURE] Lists all defined symbols, including those from library files.
Then lists all primitives of raw Joy.
(There is a variant: "_help" which lists hidden symbols).
*/
HELP(help_, ==)

/**
Q0  IGNORE_POP  2920  helpdetail  :  [ S1 S2 .. ]  ->
[IMPURE] Gives brief help on each symbol S in the list.
*/
void helpdetail_(pEnv env)
{
    int op;
    Index n;
    Entry ent;

    ONEPARAM("HELP");
    LIST("HELP");
    printf("\n");
    for (n = nodevalue(env->stck).lis; n; n = nextnode1(n)) {
        switch (op = nodetype(n)) {
        case USR_:
            ent = vec_at(env->symtab, nodevalue(n).ent);
            printf("%s  ==\n    ", ent.name);
            writeterm(env, ent.u.body, stdout);
            printf("\n\n");
            continue;

        case ANON_FUNCT_:
            op = operindex(env, nodevalue(n).proc);
            break;

        case BOOLEAN_:
            op = nodevalue(n).num ? operindex(env, true_)
                                  : operindex(env, false_);
            break;

        case INTEGER_:
            if (nodevalue(n).num == MAXINT_)
                op = operindex(env, maxint_);
            break;

        case FILE_:
            if (nodevalue(n).fil == stdin)
                op = operindex(env, stdin_);
            else if (nodevalue(n).fil == stdout)
                op = operindex(env, stdout_);
            else if (nodevalue(n).fil == stderr)
                op = operindex(env, stderr_);
            break;
        }
        printf("%s\t:  %s.\n%s\n", optable[op].name, optable[op].messg1,
               optable[op].messg2);
        if (op <= BIGNUM_)
            printf("\n");
    }
    POP(env->stck);
}

/**
Q0  IGNORE_OK  2930  manual  :  ->
[IMPURE] Writes this manual of all Joy primitives to output file.
*/
void manual_(pEnv env) { make_manual(0); }

