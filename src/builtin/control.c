/*
 *  module  : control.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped control flow builtins: branch, case, choice, cond, i, ifte,
 *  opcase, x
 */
#include "globals.h"

/* Include shared helper headers */

/* Control flow operations */
/**
Q2  OK  2590  branch  :  B [T] [F]  ->  ...
If B is true, then executes T else executes F.
*/
void branch_(pEnv env)
{
    int result;

    THREEPARAMS("branch");
    TWOQUOTES("branch");
    SAVESTACK;
    env->stck = SAVED4;
    result = get_boolean(env, SAVED3);
    exec_term(env, result ? nodevalue(SAVED2).lis : nodevalue(SAVED1).lis);
    POP(env->dump);
}

/**
Q1  OK  2100  case  :  X [..[X Y]..]  ->  Y i
Indexing on the value of X, execute the matching Y.
*/
void case_(pEnv env)
{
    Index n;

    TWOPARAMS("case");
    LIST("case");
    n = nodevalue(env->stck).lis;
    CHECKEMPTYLIST(n, "case");
    while (nextnode1(n) && nodetype(n) == LIST_) {
        if (!Compare(env, nodevalue(n).lis, nextnode1(env->stck)))
            break;
        n = nextnode1(n);
    }
    CHECKLIST(nodetype(n), "case");
    if (nextnode1(n)) {
        env->stck = nextnode2(env->stck);
        exec_term(env, nextnode1(nodevalue(n).lis));
    } else {
        env->stck = nextnode1(env->stck);
        exec_term(env, nodevalue(n).lis);
    }
}

/**
Q0  OK  1330  choice  :  B T F  ->  X
If B is true, then X = T else X = F.
*/
void choice_(pEnv env)
{
    int result;

    THREEPARAMS("choice");
    result = get_boolean(env, nextnode2(env->stck));
    if (result)
        GTERNARY(nextnode1(env->stck));
    else
        GTERNARY(env->stck);
}

/**
Q1  OK  2690  cond  :  [..[[Bi] Ti]..[D]]  ->  ...
Tries each Bi. If that yields true, then executes Ti and exits.
If no Bi yields true, executes default D.
*/
void cond_(pEnv env)
{
    Index my_dump;
    int result = 0;

    ONEPARAM("cond");
    LIST("cond");
    CHECKEMPTYLIST(nodevalue(env->stck).lis, "cond");
    /* must check for QUOTES in list */
    for (my_dump = nodevalue(env->stck).lis; nextnode1(my_dump);
         my_dump = nextnode1(my_dump))
        CHECKLIST(nodetype(nodevalue(my_dump).lis), "cond");
    SAVESTACK;
    env->dump1 = LIST_NEWNODE(nodevalue(env->stck).lis, env->dump1);
    for (; DMP1 && nextnode1(DMP1); DMP1 = nextnode1(DMP1)) {
        env->stck = SAVED2;
        exec_term(env, nodevalue(nodevalue(DMP1).lis).lis);
        result = get_boolean(env, env->stck);
        if (result)
            break;
    }
    env->stck = SAVED2;
    if (result)
        exec_term(env, nextnode1(nodevalue(DMP1).lis));
    else
        exec_term(env, nodevalue(DMP1).lis); /* default */
    POP(env->dump1);
    POP(env->dump);
}

/**
Q1  OK  2410  i  :  [P]  ->  ...
Executes P. So, [P] i  ==  P.
*/
void i_(pEnv env)
{
    ONEPARAM("i");
    ONEQUOTE("i");
    SAVESTACK;
    POP(env->stck);
    exec_term(env, nodevalue(SAVED1).lis);
    POP(env->dump);
}

/**
Q3  OK  2600  ifte  :  [B] [T] [F]  ->  ...
Executes B. If that yields true, then executes T else executes F.
*/
void ifte_(pEnv env)
{
    int result;

    THREEPARAMS("ifte");
    THREEQUOTES("ifte");
    SAVESTACK;
    env->stck = SAVED4;
    exec_term(env, nodevalue(SAVED3).lis);
    result = get_boolean(env, env->stck);
    env->stck = SAVED4;
    exec_term(env, result ? nodevalue(SAVED2).lis : nodevalue(SAVED1).lis);
    POP(env->dump);
}

/**
Q0  OK  2090  opcase  :  X [..[X Xs]..]  ->  X [Xs]
Indexing on type of X, returns the list [Xs].
*/
void opcase_(pEnv env)
{
    int op;
    proc_t proc;
    Index n, temp;

    TWOPARAMS("opcase");
    LIST("opcase");
    n = nodevalue(env->stck).lis;
    CHECKEMPTYLIST(n, "opcase");
    if ((op = nodetype(nextnode1(env->stck))) == ANON_FUNCT_)
        proc = nodevalue(nextnode1(env->stck)).proc;
    while (nextnode1(n) && nodetype(n) == LIST_) {
        temp = nodevalue(n).lis;
        if (op == nodetype(temp)) {
            if (op != ANON_FUNCT_ || proc == nodevalue(temp).proc)
                break;
        }
        n = nextnode1(n);
    }
    CHECKLIST(nodetype(n), "opcase");
    UNARY(LIST_NEWNODE,
          nextnode1(n) ? nextnode1(nodevalue(n).lis) : nodevalue(n).lis);
}

/**
Q1  OK  2410  x  :  [P] x  ->  ...
Executes P without popping [P]. So, [P] x  ==  [P] P.
*/
void x_(pEnv env)
{
    ONEPARAM("x");
    ONEQUOTE("x");
    exec_term(env, nodevalue(env->stck).lis);
}

