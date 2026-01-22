/*
 *  module  : combinators.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped combinator builtins: app1, app11, app12, app2, app3, app4, cleave,
 *  construct, dip, filter, fold, infra, map, step, times, while
 */
#include "globals.h"

/* Include shared helper headers */

/* Combinator operations */
/**
Q1  OK  2440  app1  :  X [P]  ->  R
Obsolescent.  Executes P, pushes result R on stack.
*/
void app1_(pEnv env)
{
    TWOPARAMS("app1");
    ONEQUOTE("app1");
    SAVESTACK;
    POP(env->stck);
    exec_term(env, nodevalue(SAVED1).lis);
    POP(env->dump);
}

/**
Q1  OK  2440  app11  :  X Y [P]  ->  R
Executes P, pushes result R on stack.
*/
void app11_(pEnv env)
{
    THREEPARAMS("app11");
    ONEQUOTE("app11");
    i_(env);
    popd_(env);
}

/**
Q1  OK  2460  app12  :  X Y1 Y2 [P]  ->  R1 R2
Executes P twice, with Y1 and Y2, returns R1 and R2.
*/
void app12_(pEnv env)
{
    /*   X  Y  Z  [P]  app12  */
    FOURPARAMS("app12");
    unary2_(env);
    rolldown_(env);
    pop_(env);
}

/**
Q1  OK  2530  app2  :  X1 X2 [P]  ->  R1 R2
Obsolescent.  ==  unary2
*/
void app2_(pEnv env) { unary2_(env); }

/**
Q1  OK  2540  app3  :  X1 X2 X3 [P]  ->  R1 R2 R3
Obsolescent.  == unary3
*/
void app3_(pEnv env) { unary3_(env); }

/**
Q1  OK  2550  app4  :  X1 X2 X3 X4 [P]  ->  R1 R2 R3 R4
Obsolescent.  == unary4
*/
void app4_(pEnv env) { unary4_(env); }

/**
Q2  OK  2580  cleave  :  X [P1] [P2]  ->  R1 R2
Executes P1 and P2, each with X on top, producing two results.
*/
void cleave_(pEnv env)
{ /*  X [P1] [P2] cleave ==>  X1 X2  */
    THREEPARAMS("cleave");
    TWOQUOTES("cleave");
    SAVESTACK;
    env->stck = SAVED3;
    exec_term(env, nodevalue(SAVED2).lis);               /* [P1] */
    env->dump1 = newnode2(env, env->stck, env->dump1); /* X1 */
    env->stck = SAVED3;
    exec_term(env, nodevalue(SAVED1).lis);               /* [P2] */
    env->dump1 = newnode2(env, env->stck, env->dump1); /* X2 */
    env->stck = env->dump1;
    env->dump1 = nextnode2(env->dump1);
    nextnode2(env->stck) = SAVED4;
    POP(env->dump);
}

/**
Q2  OK  2470  construct  :  [P] [[P1] [P2] ..]  ->  R1 R2 ..
Saves state of stack and then executes [P].
Then executes each [Pi] to give Ri pushed onto saved stack.
*/
void construct_(pEnv env)
{ /*  [P] [[P1] [P2] ..]  ->  X1 X2 ..  */
    TWOPARAMS("construct");
    TWOQUOTES("construct");
    SAVESTACK;
    env->stck = SAVED3;                                /* pop progs */
    env->dump1 = LIST_NEWNODE(env->dump2, env->dump1); /* save env->dump2 */
    env->dump2 = env->stck;                            /* save old stack */
    exec_term(env, nodevalue(SAVED2).lis);               /* [P] */
    env->dump3 = LIST_NEWNODE(env->stck, env->dump3);  /* save new stack */
    env->dump4 = LIST_NEWNODE(nodevalue(SAVED1).lis, env->dump4);
    for (; DMP4; DMP4 = nextnode1(DMP4)) { /* step [..] */
        exec_term(env, nodevalue(DMP4).lis);
        env->dump2 = newnode2(env, env->stck, env->dump2); /* result */
        env->stck = DMP3; /* restore new stack */
    }
    POP(env->dump4);
    POP(env->dump3);
    env->stck = env->dump2;
    env->dump2 = nodevalue(env->dump1).lis; /* restore old stack */
    POP(env->dump1);
    POP(env->dump);
}

/**
Q1  OK  2430  dip  :  X [P]  ->  ...  X
Saves X, executes P, pushes X back.
*/
void dip_(pEnv env)
{
    TWOPARAMS("dip");
    ONEQUOTE("dip");
    SAVESTACK;
    env->stck = nextnode2(env->stck);
    exec_term(env, nodevalue(SAVED1).lis);
    GNULLARY(SAVED2);
    POP(env->dump);
}

/**
Q1  OK  2830  filter  :  A [B]  ->  A1
Uses test B to filter aggregate A producing sametype aggregate A1.
*/
void filter_(pEnv env)
{
    char* str;
    Index temp;
    uint64_t set;
    int i = 0, j = 0, result = 0;

    TWOPARAMS("filter");
    ONEQUOTE("filter");
    SAVESTACK;
    switch (nodetype(SAVED2)) {
    case SET_:
        for (set = 0; i < SETSIZE; i++)
            if (nodevalue(SAVED2).set & ((int64_t)1 << i)) {
                env->stck = INTEGER_NEWNODE(i, SAVED3);
                exec_term(env, nodevalue(SAVED1).lis);
                CHECKSTACK("filter");
                result = get_boolean(env, env->stck);
                if (result)
                    set |= ((int64_t)1 << i);
            }
        env->stck = SET_NEWNODE(set, SAVED3);
        break;
    case STRING_:
        for (str = strdup((char*)&nodevalue(SAVED2)); str[i]; i++) {
            env->stck = CHAR_NEWNODE(str[i], SAVED3);
            exec_term(env, nodevalue(SAVED1).lis);
            CHECKSTACK("filter");
            result = get_boolean(env, env->stck);
            if (result)
                str[j++] = str[i];
        }
        str[j] = 0;
        env->stck = STRING_NEWNODE(str, SAVED3);
        free(str);
        break;
    case LIST_:
        env->dump1 = LIST_NEWNODE(nodevalue(SAVED2).lis, env->dump1);
        env->dump2 = LIST_NEWNODE(0, env->dump2); /* head new */
        env->dump3 = LIST_NEWNODE(0, env->dump3); /* last new */
        for (; DMP1; DMP1 = nextnode1(DMP1)) {
            env->stck = newnode2(env, DMP1, SAVED3);
            exec_term(env, nodevalue(SAVED1).lis);
            CHECKSTACK("filter");
            result = get_boolean(env, env->stck);
            if (result) { /* test */
                temp = newnode2(env, DMP1, 0);
                if (!DMP2) { /* first */
                    DMP2 = temp;
                    DMP3 = DMP2;
                } else { /* further */
                    nextnode1(DMP3) = temp;
                    DMP3 = nextnode1(DMP3);
                }
            }
        }
        env->stck = LIST_NEWNODE(DMP2, SAVED3);
        POP(env->dump3);
        POP(env->dump2);
        POP(env->dump1);
        break;
    default:
        BADAGGREGATE("filter");
    }
    POP(env->dump);
}

/**
Q1  OK  2780  fold  :  A V0 [P]  ->  V
Starting with value V0, sequentially pushes members of aggregate A
and combines with binary operator P to produce value V.
*/
void fold_(pEnv env)
{
    THREEPARAMS("fold");
    swapd_(env);
    step_(env);
}

/**
Q1  OK  2810  infra  :  L1 [P]  ->  L2
Using list L1 as stack, executes P and returns a new list L2.
The first element of L1 is used as the top of stack,
and after execution of P the top of stack becomes the first element of L2.
*/
void infra_(pEnv env)
{
    TWOPARAMS("infra");
    ONEQUOTE("infra");
    LIST2("infra");
    SAVESTACK;
    env->stck = nodevalue(SAVED2).lis;
    exec_term(env, nodevalue(SAVED1).lis);
    env->stck = LIST_NEWNODE(env->stck, SAVED3);
    POP(env->dump);
}

/**
Q1  OK  2790  map  :  A [P]  ->  B
Executes P on each member of aggregate A,
collects results in sametype aggregate B.
*/
void map_(pEnv env)
{
    char* str;
    Index temp;
    uint64_t set;
    int i = 0, j = 0;

    TWOPARAMS("map");
    ONEQUOTE("map");
    SAVESTACK;
    switch (nodetype(SAVED2)) {
    case LIST_:
        env->dump1 = LIST_NEWNODE(nodevalue(SAVED2).lis, env->dump1);
        env->dump2 = LIST_NEWNODE(0, env->dump2); /* head new */
        env->dump3 = LIST_NEWNODE(0, env->dump3); /* last new */
        for (; DMP1; DMP1 = nextnode1(DMP1)) {
            env->stck = newnode2(env, DMP1, SAVED3);
            exec_term(env, nodevalue(SAVED1).lis);
            CHECKSTACK("map");
            temp = newnode2(env, env->stck, 0);
            if (!DMP2) { /* first */
                DMP2 = temp;
                DMP3 = DMP2;
            } else { /* further */
                nextnode1(DMP3) = temp;
                DMP3 = nextnode1(DMP3);
            }
        }
        env->stck = LIST_NEWNODE(DMP2, SAVED3);
        POP(env->dump3);
        POP(env->dump2);
        POP(env->dump1);
        break;
    case STRING_:
        for (str = strdup((char*)&nodevalue(SAVED2)); str[i]; i++) {
            env->stck = CHAR_NEWNODE(str[i], SAVED3);
            exec_term(env, nodevalue(SAVED1).lis);
            CHECKSTACK("map");
            str[j++] = nodevalue(env->stck).num;
        }
        str[j] = 0;
        env->stck = STRING_NEWNODE(str, SAVED3);
        free(str);
        break;
    case SET_:
        for (set = 0; i < SETSIZE; i++)
            if (nodevalue(SAVED2).set & ((int64_t)1 << i)) {
                env->stck = INTEGER_NEWNODE(i, SAVED3);
                exec_term(env, nodevalue(SAVED1).lis);
                CHECKSTACK("map");
                set |= ((int64_t)1 << nodevalue(env->stck).num);
            }
        env->stck = SET_NEWNODE(set, SAVED3);
        break;
    default:
        BADAGGREGATE("map");
    }
    POP(env->dump);
}

/**
Q1  OK  2770  step  :  A [P]  ->  ...
Sequentially putting members of aggregate A onto stack,
executes P for each member of A.
*/
void step_(pEnv env)
{
    int i = 0;
    char* str;

    TWOPARAMS("step");
    ONEQUOTE("step");
    SAVESTACK;
    env->stck = nextnode2(env->stck);
    switch (nodetype(SAVED2)) {
    case LIST_:
        env->dump1 = LIST_NEWNODE(nodevalue(SAVED2).lis, env->dump1);
        for (; DMP1; DMP1 = nextnode1(DMP1)) {
            GNULLARY(DMP1);
            exec_term(env, nodevalue(SAVED1).lis);
        }
        POP(env->dump1);
        break;
    case STRING_:
        for (str = strdup((char*)&nodevalue(SAVED2)); str[i]; i++) {
            NULLARY(CHAR_NEWNODE, str[i]);
            exec_term(env, nodevalue(SAVED1).lis);
        }
        free(str);
        break;
    case SET_:
        for (; i < SETSIZE; i++)
            if (nodevalue(SAVED2).set & ((int64_t)1 << i)) {
                NULLARY(INTEGER_NEWNODE, i);
                exec_term(env, nodevalue(SAVED1).lis);
            }
        break;
    default:
        BADAGGREGATE("step");
    }
    POP(env->dump);
}

/**
Q1  OK  2800  times  :  N [P]  ->  ...
N times executes P.
*/
void times_(pEnv env)
{
    int64_t i, n;

    TWOPARAMS("times");
    ONEQUOTE("times");
    INTEGER2("times");
    SAVESTACK;
    env->stck = nextnode2(env->stck);
    n = nodevalue(SAVED2).num;
    for (i = 0; i < n; i++)
        exec_term(env, nodevalue(SAVED1).lis);
    POP(env->dump);
}

/**
Q2  OK  2700  while  :  [B] [D]  ->  ...
While executing B yields true executes D.
*/
void while_(pEnv env)
{
    int result;

    TWOPARAMS("while");
    TWOQUOTES("while");
    SAVESTACK;
    while (1) {
        env->stck = SAVED3;
        exec_term(env, nodevalue(SAVED2).lis); /* TEST */
        CHECKSTACK("while");
        result = get_boolean(env, env->stck);
        if (!result)
            break;
        env->stck = SAVED3;
        exec_term(env, nodevalue(SAVED1).lis); /* DO */
        SAVED3 = env->stck;
    }
    env->stck = SAVED3;
    POP(env->dump);
}

/**
Q2  OK  2390  let  :  X1 X2 ... Xn [name1 name2 ... namen] [body]  ->  result
Binds n values from the stack to names, executes body, restores original bindings.
Names are bound in reverse order: name1 gets X1 (deepest), namen gets Xn (top).

Example:
  10 20 [a b] [a b + a b - *] let  =>  300
  (* a=10, b=20, computes (a+b)*(a-b) = 30*(-10) = -300 *)
*/
void let_(pEnv env)
{
    Index names, body, cur;
    int num_names, i;
    Entry* saved_entries;
    int* indices;
    Index* bodies;

    TWOPARAMS("let");
    TWOQUOTES("let");

    /* Get body and names quotations */
    body = nodevalue(env->stck).lis;
    names = nodevalue(nextnode1(env->stck)).lis;
    env->stck = nextnode2(env->stck);  /* Pop both quotations */

    /* Count names and validate they are user symbols (not builtins) */
    num_names = 0;
    for (cur = names; cur; cur = nextnode1(cur)) {
        Operator op = nodetype(cur);
        if (op == ANON_FUNCT_) {
            /* Builtins won't work because the body already has the function pointer */
            execerror(env, "let", "cannot bind builtin names; use fresh identifiers");
            return;
        }
        if (op != USR_) {
            execerror(env, "let", "names must be symbols (got literal value)");
            return;
        }
        num_names++;
    }

    if (num_names == 0) {
        /* No bindings, just execute body */
        exec_term(env, body);
        return;
    }

    /* Allocate arrays to save state */
    saved_entries = (Entry*)malloc(num_names * sizeof(Entry));
    indices = (int*)malloc(num_names * sizeof(int));
    bodies = (Index*)malloc(num_names * sizeof(Index));

    if (!saved_entries || !indices || !bodies) {
        free(saved_entries);
        free(indices);
        free(bodies);
        execerror(env, "let", "memory allocation failed");
        return;
    }

    /* Collect symbol table indices (all must be USR_ at this point) */
    i = 0;
    for (cur = names; cur; cur = nextnode1(cur)) {
        indices[i++] = nodevalue(cur).ent;
    }

    /* Bind values (reverse order: pop values for namen first) */
    for (i = num_names - 1; i >= 0; i--) {
        Index value;
        Entry ent;
        int index = indices[i];

        /* Check we have enough values on stack */
        if (!env->stck) {
            /* Restore already-bound entries */
            for (int j = num_names - 1; j > i; j--) {
                vec_at(env->symtab, indices[j]) = saved_entries[j];
            }
            free(saved_entries);
            free(indices);
            free(bodies);
            execerror(env, "let", "not enough values for names");
            return;
        }

        /* Pop value from stack */
        value = env->stck;
        env->stck = nextnode1(env->stck);

        /* Save original entry */
        saved_entries[i] = vec_at(env->symtab, index);

        /* Create a body that pushes the value */
        bodies[i] = newnode2(env, value, 0);

        /* Update symbol table entry */
        ent = saved_entries[i];
        ent.is_user = 1;
        ent.u.body = bodies[i];
        vec_at(env->symtab, index) = ent;
    }

    /* Execute body */
    exec_term(env, body);

    /* Restore original entries */
    for (i = 0; i < num_names; i++) {
        vec_at(env->symtab, indices[i]) = saved_entries[i];
    }

    free(saved_entries);
    free(indices);
    free(bodies);
}

