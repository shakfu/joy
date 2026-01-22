/*
 *  module  : recursion.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped recursion builtins: binrec, condlinrec, condnestrec, genrec,
 *  genrecaux, linrec, primrec, tailrec, treegenrec, treegenrecaux, treerec,
 *  treerecaux, treestep
 */
#include "globals.h"

/* Include shared helper headers */

/* Forward declarations for mutually recursive aux functions */
void condnestrecaux(pEnv env);

/* Recursion operations */
/**
Q4  OK  2730  binrec  :  [P] [T] [R1] [R2]  ->  ...
Executes P. If that yields true, executes T.
Else uses R1 to produce two intermediates, recurses on both,
then executes R2 to combine their results.
*/
void binrecaux(pEnv env)
{
    int result;

    env->dump1 = LIST_NEWNODE(env->stck, env->dump1);
    exec_term(env, nodevalue(SAVED4).lis);
    result = get_boolean(env, env->stck);
    env->stck = DMP1;
    POP(env->dump1);
    if (result)
        exec_term(env, nodevalue(SAVED3).lis);
    else {
        exec_term(env, nodevalue(SAVED2).lis); /* split */
        env->dump2 = newnode2(env, env->stck, env->dump2);
        POP(env->stck);
        binrecaux(env); /* first */
        GNULLARY(env->dump2);
        POP(env->dump2);
        binrecaux(env); /* second */
        exec_term(env, nodevalue(SAVED1).lis);
    } /* combine */
}

void binrec_(pEnv env)
{
    FOURPARAMS("binrec");
    FOURQUOTES("binrec");
    SAVESTACK;
    env->stck = SAVED5;
    binrecaux(env);
    POP(env->dump);
}

/**
Q1  OK  2760  condlinrec  :  [ [C1] [C2] .. [D] ]  ->  ...
Each [Ci] is of the form [[B] [T]] or [[B] [R1] [R2]].
Tries each B. If that yields true and there is just a [T], executes T and exit.
If there are [R1] and [R2], executes R1, recurses, executes R2.
Subsequent case are ignored. If no B yields true, then [D] is used.
It is then of the form [[T]] or [[R1] [R2]]. For the former, executes T.
For the latter executes R1, recurses, executes R2.
*/
void condlinrec_(pEnv env)
{
    ONEPARAM("condlinrec");
    LIST("condlinrec");
    CHECKEMPTYLIST(nodevalue(env->stck).lis, "condlinrec");
    SAVESTACK;
    env->stck = SAVED2;
    condnestrecaux(env);
    POP(env->dump);
}

/**
Q1  OK  2750  condnestrec  :  [ [C1] [C2] .. [D] ]  ->  ...
A generalisation of condlinrec.
Each [Ci] is of the form [[B] [R1] [R2] .. [Rn]] and [D] is of the form
[[R1] [R2] .. [Rn]]. Tries each B, or if all fail, takes the default [D].
For the case taken, executes each [Ri] but recurses between any two
consecutive [Ri] (n > 3 would be exceptional.)
*/
void condnestrecaux(pEnv env)
{
    int result = 0;

    env->dump1 = LIST_NEWNODE(nodevalue(SAVED1).lis, env->dump1);
    env->dump2 = LIST_NEWNODE(env->stck, env->dump2);
    for (; DMP1 && nextnode1(DMP1); DMP1 = nextnode1(DMP1)) {
        env->stck = DMP2;
        exec_term(env, nodevalue(nodevalue(DMP1).lis).lis);
        result = get_boolean(env, env->stck);
        if (result)
            break;
    }
    env->stck = DMP2;
    env->dump3 = LIST_NEWNODE(
        (result ? nextnode1(nodevalue(DMP1).lis) : nodevalue(DMP1).lis),
        env->dump3);
    exec_term(env, nodevalue(DMP3).lis);
    for (DMP3 = nextnode1(DMP3); DMP3; DMP3 = nextnode1(DMP3)) {
        condnestrecaux(env);
        exec_term(env, nodevalue(DMP3).lis);
    }
    POP(env->dump3);
    POP(env->dump2);
    POP(env->dump1);
}

void condnestrec_(pEnv env)
{
    ONEPARAM("condnestrec");
    LIST("condnestrec");
    CHECKEMPTYLIST(nodevalue(env->stck).lis, "condnestrec");
    SAVESTACK;
    env->stck = SAVED2;
    condnestrecaux(env);
    POP(env->dump);
}

/**
Q4  OK  2740  genrec  :  [B] [T] [R1] [R2]  ->  ...
Executes B, if that yields true, executes T.
Else executes R1 and then [[[B] [T] [R1] R2] genrec] R2.
*/
void genrec_(pEnv env)
{
    FOURPARAMS("genrec");
    FOURQUOTES("genrec");
    cons_(env);
    cons_(env);
    cons_(env);
    genrecaux_(env);
}

/**
Q1  OK  3240  #genrec  :  [[B] [T] [R1] R2]  ->  ...
Executes B, if that yields true, executes T.
Else executes R1 and then [[[B] [T] [R1] R2] genrec] R2.
*/
void genrecaux_(pEnv env)
{
    Index temp;
    int result;

    SAVESTACK;
    POP(env->stck);
    exec_term(env, nodevalue(nodevalue(SAVED1).lis).lis); /* [B] */
    CHECKSTACK("genrecaux");
    result = get_boolean(env, env->stck);
    env->stck = SAVED2;
    if (result)
        exec_term(env,
                nodevalue(nextnode1(nodevalue(SAVED1).lis)).lis); /* [T] */
    else {
        exec_term(env,
                nodevalue(nextnode2(nodevalue(SAVED1).lis)).lis); /* [R1] */
        NULLARY(LIST_NEWNODE, nodevalue(SAVED1).lis);
        temp = ANON_FUNCT_NEWNODE(genrecaux_, 0);
        NULLARY(LIST_NEWNODE, temp);
        cons_(env);
        exec_term(env, nextnode3(nodevalue(SAVED1).lis)); /* [R2] */
    }
    POP(env->dump);
}

/**
Q4  OK  2710  linrec  :  [P] [T] [R1] [R2]  ->  ...
Executes P. If that yields true, executes T.
Else executes R1, recurses, executes R2.
*/
void linrecaux(pEnv env)
{
    int result;

    env->dump1 = LIST_NEWNODE(env->stck, env->dump1);
    exec_term(env, nodevalue(SAVED4).lis);
    CHECKSTACK("linrec");
    result = get_boolean(env, env->stck);
    env->stck = DMP1;
    POP(env->dump1);
    if (result)
        exec_term(env, nodevalue(SAVED3).lis);
    else {
        exec_term(env, nodevalue(SAVED2).lis);
        linrecaux(env);
        exec_term(env, nodevalue(SAVED1).lis);
    }
}

void linrec_(pEnv env)
{
    FOURPARAMS("linrec");
    FOURQUOTES("linrec");
    SAVESTACK;
    env->stck = SAVED5;
    linrecaux(env);
    POP(env->dump);
}

/**
Q2  OK  2820  primrec  :  X [I] [C]  ->  R
Executes I to obtain an initial value R0.
For integer X uses increasing positive integers to X, combines by C for new R.
For aggregate X uses successive members and combines by C for new R.
*/
void primrec_(pEnv env)
{
    char* str;
    uint64_t set;
    int i = 0, n = 0;

    THREEPARAMS("primrec");
    TWOQUOTES("primrec");
    SAVESTACK;
    env->stck = nextnode3(env->stck);
    switch (nodetype(SAVED3)) {
    case LIST_:
        env->dump1 = LIST_NEWNODE(nodevalue(SAVED3).lis, env->dump1);
        for (; DMP1; DMP1 = nextnode1(DMP1)) {
            GNULLARY(DMP1);
            n++;
        }
        POP(env->dump1);
        break;
    case STRING_:
        for (str = strdup((char*)&nodevalue(SAVED3)); str[i]; i++) {
            NULLARY(CHAR_NEWNODE, str[i]);
            n++;
        }
        free(str);
        break;
    case SET_:
        set = nodevalue(SAVED3).set;
        for (; i < SETSIZE; i++)
            if (set & ((int64_t)1 << i)) {
                NULLARY(INTEGER_NEWNODE, i);
                n++;
            }
        break;
    case INTEGER_:
        for (i = nodevalue(SAVED3).num; i > 0; i--) {
            NULLARY(INTEGER_NEWNODE, i);
            n++;
        }
        break;
    default:
        BADDATA("primrec");
    }
    exec_term(env, nodevalue(SAVED2).lis);
    for (i = 1; i <= n; i++)
        exec_term(env, nodevalue(SAVED1).lis);
    POP(env->dump);
}

/**
Q3  OK  2720  tailrec  :  [P] [T] [R1]  ->  ...
Executes P. If that yields true, executes T.
Else executes R1, recurses.
*/
void tailrecaux(pEnv env)
{
    int result;

tailrec:
    env->dump1 = LIST_NEWNODE(env->stck, env->dump1);
    exec_term(env, nodevalue(SAVED3).lis);
    CHECKSTACK("tailrec");
    result = get_boolean(env, env->stck);
    env->stck = DMP1;
    POP(env->dump1);
    if (result)
        exec_term(env, nodevalue(SAVED2).lis);
    else {
        exec_term(env, nodevalue(SAVED1).lis);
        goto tailrec;
    }
}

void tailrec_(pEnv env)
{
    THREEPARAMS("tailrec");
    THREEQUOTES("tailrec");
    SAVESTACK;
    env->stck = SAVED4;
    tailrecaux(env);
    POP(env->dump);
}

/**
Q3  OK  2890  treegenrec  :  T [O1] [O2] [C]  ->  ...
T is a tree. If T is a leaf, executes O1.
Else executes O2 and then [[[O1] [O2] C] treegenrec] C.
*/
void treegenrec_(pEnv env)
{ /* T [O1] [O2] [C] */
    FOURPARAMS("treegenrec");
    THREEQUOTES("treegenrec");
    cons_(env);
    cons_(env);
    treegenrecaux_(env);
}

/**
Q1  OK  3250  #treegenrec  :  T [[O1] [O2] C]  ->  ...
T is a tree. If T is a leaf, executes O1.
Else executes O2 and then [[[O1] [O2] C] treegenrec] C.
*/
void treegenrecaux_(pEnv env)
{
    Index temp;

    if (nodetype(nextnode1(env->stck)) == LIST_) {
        SAVESTACK; /* begin DIP */
        POP(env->stck);
        exec_term(env,
                nodevalue(nextnode1(nodevalue(SAVED1).lis)).lis); /* [O2] */
        GNULLARY(SAVED1);
        POP(env->dump); /* end DIP */
        temp = ANON_FUNCT_NEWNODE(treegenrecaux_, 0);
        NULLARY(LIST_NEWNODE, temp);
        cons_(env);
        exec_term(env, nextnode2(nodevalue(nodevalue(env->stck).lis).lis));
    } else { /* [C] */
        env->dump1 = LIST_NEWNODE(nodevalue(env->stck).lis, env->dump1);
        POP(env->stck);
        exec_term(env, nodevalue(DMP1).lis);
        POP(env->dump1);
    }
}

/**
Q2  OK  2880  treerec  :  T [O] [C]  ->  ...
T is a tree. If T is a leaf, executes O. Else executes [[[O] C] treerec] C.
*/
void treerec_(pEnv env)
{
    THREEPARAMS("treerec");
    TWOQUOTES("treerec");
    cons_(env);
    treerecaux_(env);
}

/**
Q1  OK  3260  #treerec  :  T [[O] C]  ->  ...
T is a tree. If T is a leaf, executes O. Else executes [[[O] C] treerec] C.
*/
void treerecaux_(pEnv env)
{
    Index temp;

    if (nodetype(nextnode1(env->stck)) == LIST_) {
        temp = ANON_FUNCT_NEWNODE(treerecaux_, 0);
        NULLARY(LIST_NEWNODE, temp);
        cons_(env); /*  D  [[[O] C] ANON_FUNCT_]  */
        exec_term(env, nextnode1(nodevalue(nodevalue(env->stck).lis).lis));
    } else {
        env->dump1 = LIST_NEWNODE(nodevalue(env->stck).lis, env->dump1);
        POP(env->stck);
        exec_term(env, nodevalue(DMP1).lis);
        POP(env->dump1);
    }
}

/**
Q1  OK  2870  treestep  :  T [P]  ->  ...
Recursively traverses leaves of tree T, executes P for each leaf.
*/
void treestepaux(pEnv env, Index item)
{
    if (nodetype(item) != LIST_) {
        GNULLARY(item);
        exec_term(env, nodevalue(SAVED1).lis);
    } else {
        env->dump1 = LIST_NEWNODE(nodevalue(item).lis, env->dump1);
        for (; DMP1; DMP1 = nextnode1(DMP1))
            treestepaux(env, DMP1);
        POP(env->dump1);
    }
}

void treestep_(pEnv env)
{
    TWOPARAMS("treestep");
    ONEQUOTE("treestep");
    SAVESTACK;
    env->stck = SAVED3;
    treestepaux(env, SAVED2);
    POP(env->dump);
}

