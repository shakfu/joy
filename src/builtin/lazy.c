/*
 *  module  : lazy.c
 *  version : 1.0
 *  date    : 01/31/26
 *
 *  Lazy (potentially infinite) sequence builtins:
 *  iterate, repeat, cycle, lazy-range, force
 */
#include "globals.h"

static int lazy_null(pEnv env, LazyData* lzy);
void lazy_step(pEnv env);
void lazy_take(pEnv env, int64_t n);

/*
 * Helper: Allocate and initialize a LazyData structure.
 */
static LazyData* make_lazy(pEnv env, int kind, Index state, Index generator, int64_t limit)
{
    LazyData* lzy = GC_CTX_MALLOC(env, sizeof(LazyData));
    lzy->kind = kind;
    lzy->state = state;
    lzy->generator = generator;
    lzy->limit = limit;
    return lzy;
}

/*
 * Helper: Deep copy a LazyData structure (for GC and rest operations).
 */
static LazyData* copy_lazy(pEnv env, LazyData* src)
{
    LazyData* lzy = GC_CTX_MALLOC(env, sizeof(LazyData));
    *lzy = *src;
    return lzy;
}

/*
 * Nodes that newnode2 of node n allocates. LazyData holds indices where a GC
 * cannot find them until a node refers to it, so each builtin reserves all of
 * its nodes before it reads an index. See ensure_capacity.
 */
static int nodes_for(pEnv env, Index n)
{
    if (nodetype(n) == STRING_ || nodetype(n) == BIGNUM_)
        return (int)(strlen(GETSTRING(n)) / sizeof(Node)) + 2;
    return 1;
}

/**
Q0  OK  4000  iterate  :  X [F]  ->  L
Creates a lazy sequence: X, F(X), F(F(X)), ...
The sequence is infinite. Use take or force to materialize elements.
*/
void iterate_(pEnv env)
{
    LazyData* lzy;
    Index state, generator;

    TWOPARAMS("iterate");
    ONEQUOTE("iterate");
    ensure_capacity(env, nodes_for(env, nextnode1(env->stck)) + 1);

    generator = nodevalue(env->stck).lis;
    POP(env->stck);
    /* Copy state node to protect from stack changes */
    state = newnode2(env, env->stck, 0);

    lzy = make_lazy(env, LAZY_ITERATE, state, generator, -1);
    UNARY(LAZY_NEWNODE, lzy);
}

/**
Q0  OK  4010  replicate  :  X  ->  L
Creates an infinite lazy sequence of X: X, X, X, ...
Use take or force to materialize elements.
*/
void replicate_(pEnv env)
{
    LazyData* lzy;
    Index state;

    ONEPARAM("repeat");
    ensure_capacity(env, nodes_for(env, env->stck) + 1);

    /* Copy state node to protect from stack changes */
    state = newnode2(env, env->stck, 0);

    lzy = make_lazy(env, LAZY_REPEAT, state, 0, -1);
    UNARY(LAZY_NEWNODE, lzy);
}

/**
Q0  OK  4020  cycle  :  [list]  ->  L
Creates an infinite lazy sequence cycling through the list elements.
[1 2 3] cycle produces: 1, 2, 3, 1, 2, 3, 1, ...
Error if list is empty.
*/
void cycle_(pEnv env)
{
    LazyData* lzy;
    Index list;

    ONEPARAM("cycle");
    LIST("cycle");
    ensure_capacity(env, 1);

    list = nodevalue(env->stck).lis;
    CHECKEMPTYLIST(list, "cycle");

    /* state = current position in list, generator = original list for cycling back */
    lzy = make_lazy(env, LAZY_CYCLE, list, list, -1);
    UNARY(LAZY_NEWNODE, lzy);
}

/**
Q0  OK  4030  lazy-range\0lazy_range  :  N M  ->  L  OR  N  ->  L
Creates a lazy range from N to M inclusive (finite), or from N to infinity (single argument).
When called with two arguments: lazy sequence from N to M.
When called with one argument: infinite lazy sequence starting from N.
*/
void lazy_range_(pEnv env)
{
    LazyData* lzy;
    Index state;
    int64_t start, limit;

    ONEPARAM("lazy-range");

    if (nodetype(env->stck) != INTEGER_) {
        execerror(env, "integer", "lazy-range");
        return;
    }

    ensure_capacity(env, 2);

    /* Check if we have two arguments (both integers) */
    if (nextnode1(env->stck) && nodetype(nextnode1(env->stck)) == INTEGER_) {
        /* Two argument form: N M -> range from N to M */
        limit = nodevalue(env->stck).num;
        POP(env->stck);
        start = nodevalue(env->stck).num;
        state = INTEGER_NEWNODE(start, 0);
        lzy = make_lazy(env, LAZY_RANGE, state, 0, limit);
        UNARY(LAZY_NEWNODE, lzy);
    } else {
        /* One argument form: N -> infinite range from N */
        start = nodevalue(env->stck).num;
        state = INTEGER_NEWNODE(start, 0);
        lzy = make_lazy(env, LAZY_RANGE, state, 0, -1);
        UNARY(LAZY_NEWNODE, lzy);
    }
}

/**
Q0  OK  4040  force  :  L N  ->  [list]
Materializes the first N elements of lazy sequence L into a list.
Error if L is not a lazy sequence or N is negative.
*/
void force_(pEnv env)
{
    int n;

    TWOPARAMS("force");
    POSITIVEINDEX(env->stck, "force");

    n = nodevalue(env->stck).num;
    POP(env->stck);

    if (nodetype(env->stck) != LAZY_) {
        execerror(env, "lazy sequence", "force");
        return;
    }
    lazy_take(env, n);
}

/*
 * Replace the lazy sequence on top of the stack by its rest. A GC copies the
 * LazyData and moves the nodes it refers to, so nothing read from it is kept
 * across an allocation: the sequence stays on the stack, or on the dump while
 * the generator runs, and nodes are reserved before an index is held.
 */
void lazy_step(pEnv env)
{
    LazyData* lzy;
    Index state;

    if (nodevalue(env->stck).lzy->kind == LAZY_ITERATE) {
        /* The generator sees only the state; the stack waits on the dump */
        env->dump1 = LIST_NEWNODE(env->stck, env->dump1);
        env->stck = newnode2(env, nodevalue(DMP1).lzy->state, 0);
        exec_term(env, nodevalue(DMP1).lzy->generator);
        ensure_capacity(env, (env->stck ? nodes_for(env, env->stck) : 0) + 1);
        state = env->stck ? newnode2(env, env->stck, 0) : 0;
        lzy = make_lazy(env, LAZY_ITERATE, state, nodevalue(DMP1).lzy->generator, -1);
        env->stck = DMP1;
        POP(env->dump1);
        UNARY(LAZY_NEWNODE, lzy);
        return;
    }

    ensure_capacity(env, 2);
    lzy = nodevalue(env->stck).lzy;
    switch (lzy->kind) {
    case LAZY_CYCLE:
        /* Move to next element, wrap to the beginning */
        state = nextnode1(lzy->state) ? nextnode1(lzy->state) : lzy->generator;
        lzy = make_lazy(env, LAZY_CYCLE, state, lzy->generator, -1);
        break;
    case LAZY_RANGE:
        state = INTEGER_NEWNODE(WRAP_ADD(nodevalue(lzy->state).num, 1), 0);
        lzy = make_lazy(env, LAZY_RANGE, state, 0, lzy->limit);
        break;
    default: /* LAZY_REPEAT: the state does not change */
        lzy = copy_lazy(env, lzy);
        break;
    }
    UNARY(LAZY_NEWNODE, lzy);
}

/*
 * Replace the lazy sequence on top of the stack by a list of its first n
 * elements (fewer if it ends). The list is built on the dump.
 */
void lazy_take(pEnv env, int64_t n)
{
    Index temp, result;

    env->dump2 = LIST_NEWNODE(0, env->dump2); /* head */
    env->dump3 = LIST_NEWNODE(0, env->dump3); /* last */
    for (; n > 0 && !lazy_null(env, nodevalue(env->stck).lzy); n--) {
        temp = newnode2(env, nodevalue(env->stck).lzy->state, 0);
        if (!DMP2)
            DMP2 = DMP3 = temp;
        else {
            nextnode1(DMP3) = temp;
            DMP3 = temp;
        }
        if (n > 1)
            lazy_step(env);
    }
    result = DMP2;
    POP(env->dump3);
    POP(env->dump2);
    UNARY(LIST_NEWNODE, result);
}

/*
 * Helper: Get the current/first value of a lazy sequence.
 * Used by first_ for LAZY_ type.
 */
static Index lazy_first(pEnv env, LazyData* lzy)
{
    switch (lzy->kind) {
    case LAZY_ITERATE:
    case LAZY_REPEAT:
    case LAZY_RANGE:
        return lzy->state;
    case LAZY_CYCLE:
        return lzy->state;  /* state is the current position in the cycle */
    default:
        return 0;
    }
}

/*
 * Helper: Check if a lazy sequence is exhausted (for finite sequences).
 * Returns 1 if exhausted, 0 otherwise.
 */
static int lazy_null(pEnv env, LazyData* lzy)
{
    if (lzy->kind == LAZY_RANGE && lzy->limit >= 0) {
        if (nodetype(lzy->state) == INTEGER_ && nodevalue(lzy->state).num > lzy->limit)
            return 1;
    }
    /* Infinite sequences are never null */
    return 0;
}

/*
 * These helper functions are used by aggregate.c.
 * We declare them here and define the actual integration in aggregate.c.
 */

/* Export helper functions for use in aggregate.c */
Index lazy_first_helper(pEnv env, LazyData* lzy)
{
    return lazy_first(env, lzy);
}

int lazy_null_helper(pEnv env, LazyData* lzy)
{
    return lazy_null(env, lzy);
}
