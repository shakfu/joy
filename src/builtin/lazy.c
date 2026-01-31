/*
 *  module  : lazy.c
 *  version : 1.0
 *  date    : 01/31/26
 *
 *  Lazy (potentially infinite) sequence builtins:
 *  iterate, repeat, cycle, lazy-range, force
 */
#include "globals.h"

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
    LazyData* lzy;
    Index temp, state, result;
    int n, i;

    TWOPARAMS("force");
    POSITIVEINDEX(env->stck, "force");

    n = nodevalue(env->stck).num;
    POP(env->stck);

    if (nodetype(env->stck) != LAZY_) {
        execerror(env, "lazy sequence", "force");
        return;
    }

    lzy = nodevalue(env->stck).lzy;

    /* Build result list using dump registers for GC protection */
    env->dump1 = LIST_NEWNODE(0, env->dump1);  /* head */
    env->dump2 = LIST_NEWNODE(0, env->dump2);  /* tail */
    env->dump3 = LIST_NEWNODE(lzy->state, env->dump3);  /* current state */

    for (i = 0; i < n; i++) {
        state = DMP3;

        /* Check if finite sequence exhausted */
        if (lzy->kind == LAZY_RANGE && lzy->limit >= 0) {
            if (nodetype(state) != INTEGER_ || nodevalue(state).num > lzy->limit)
                break;
        }
        if (lzy->kind == LAZY_CYCLE && !state)
            break;  /* Should not happen for valid cycle */

        /* Get current value */
        switch (lzy->kind) {
        case LAZY_ITERATE:
        case LAZY_REPEAT:
        case LAZY_RANGE:
            temp = newnode2(env, state, 0);
            break;
        case LAZY_CYCLE:
            temp = newnode2(env, state, 0);
            break;
        default:
            temp = 0;
            break;
        }

        /* Append to result list */
        if (!DMP1) {
            DMP1 = temp;
            DMP2 = temp;
        } else {
            nextnode1(DMP2) = temp;
            DMP2 = temp;
        }

        /* Advance state for next iteration */
        switch (lzy->kind) {
        case LAZY_ITERATE:
            /* Execute generator: push state, execute quotation, get result */
            {
                Index saved_stck = env->stck;
                env->stck = newnode2(env, state, 0);
                exec_term(env, lzy->generator);
                if (env->stck) {
                    DMP3 = newnode2(env, env->stck, 0);
                }
                env->stck = saved_stck;
            }
            break;
        case LAZY_REPEAT:
            /* State stays the same */
            break;
        case LAZY_CYCLE:
            /* Move to next element, wrap to beginning if needed */
            if (nextnode1(state))
                DMP3 = nextnode1(state);
            else
                DMP3 = lzy->generator;  /* Wrap to beginning */
            break;
        case LAZY_RANGE:
            /* Increment state */
            DMP3 = INTEGER_NEWNODE(nodevalue(state).num + 1, 0);
            break;
        }
    }

    result = DMP1;
    POP(env->dump3);
    POP(env->dump2);
    POP(env->dump1);

    UNARY(LIST_NEWNODE, result);
}

/*
 * Helper: Advance a lazy sequence by one step, returning new LazyData.
 * Used by rest_ for LAZY_ type.
 */
static LazyData* lazy_rest(pEnv env, LazyData* lzy)
{
    LazyData* new_lzy;
    Index new_state;

    switch (lzy->kind) {
    case LAZY_ITERATE:
        /* Execute generator to get next state */
        {
            Index saved_stck = env->stck;
            env->stck = newnode2(env, lzy->state, 0);
            exec_term(env, lzy->generator);
            if (env->stck) {
                new_state = newnode2(env, env->stck, 0);
            } else {
                new_state = 0;
            }
            env->stck = saved_stck;
        }
        new_lzy = make_lazy(env, LAZY_ITERATE, new_state, lzy->generator, -1);
        break;

    case LAZY_REPEAT:
        /* State stays the same, just copy */
        new_lzy = copy_lazy(env, lzy);
        break;

    case LAZY_CYCLE:
        /* Move to next element, wrap if needed */
        if (nextnode1(lzy->state))
            new_state = nextnode1(lzy->state);
        else
            new_state = lzy->generator;  /* Wrap to beginning */
        new_lzy = make_lazy(env, LAZY_CYCLE, new_state, lzy->generator, -1);
        break;

    case LAZY_RANGE:
        /* Increment state */
        new_state = INTEGER_NEWNODE(nodevalue(lzy->state).num + 1, 0);
        new_lzy = make_lazy(env, LAZY_RANGE, new_state, 0, lzy->limit);
        break;

    default:
        new_lzy = copy_lazy(env, lzy);
        break;
    }

    return new_lzy;
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
LazyData* lazy_rest_helper(pEnv env, LazyData* lzy)
{
    return lazy_rest(env, lzy);
}

Index lazy_first_helper(pEnv env, LazyData* lzy)
{
    return lazy_first(env, lzy);
}

int lazy_null_helper(pEnv env, LazyData* lzy)
{
    return lazy_null(env, lzy);
}
