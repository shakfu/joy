/*
 *  module  : parallel.c
 *  version : 2.0
 *  date    : 11/01/26
 *
 *  Grouped parallel builtins: pfork, pmap, pfilter, preduce
 */
#include "globals.h"
#include "parallel.h"

/* Strings are inline: bytes past the NUL still hold part of a stale pointer. */
static int pfilter_keeps(Node* memory, Index node)
{
    Operator op = memory[node].op;

    if (op == STRING_ || op == BIGNUM_)
        return *(char*)&memory[node].u != 0;
    return memory[node].u.num != 0;
}

#ifdef JOY_PARALLEL
/*
 * Each *_parallel function returns 1 with the combinator finished, or 0 with
 * the stack and dump untouched, so that the sequential code runs instead. A
 * task that errors or needs a shared side effect makes it return 0: results,
 * errors and output then come from one sequential run. See doc/parallel.md.
 *
 * Index values into the parent's memory stay valid only while the tasks run.
 * Allocating in the parent afterwards may move everything not rooted.
 */
enum { PARALLEL_MIN_ITEMS = 4 }; /* below this, thread overhead dominates */

static void silent_error(void* user_data, int code, const char* msg,
                         const char* filename, int line, int column)
{
    (void)user_data, (void)code, (void)msg;
    (void)filename, (void)line, (void)column;
}

static void task_init(ParallelTask* task, pEnv parent)
{
    pEnv child = &task->child_env;
    Node* root = parent->parent_memory ? parent->parent_memory : parent->memory;

    task->parent_env = parent;
    memset(child, 0, sizeof(Env));
    child->config.autoput = parent->config.autoput;
    child->config.echoflag = parent->config.echoflag;
    child->config.tracegc = parent->config.tracegc;
    child->config.undeferror = parent->config.undeferror;
    child->config.overwrite = parent->config.overwrite;
    child->ignore = parent->ignore;
    child->symtab = parent->symtab; /* read-only: see PARALLEL_UNSAFE */
    child->hash = parent->hash;
    child->prim = parent->prim;
    child->g_argc = parent->g_argc;
    child->g_argv = parent->g_argv;
    child->parent_memory = root;
    child->io.on_error = silent_error;
    child->gc_ctx = gc_ctx_create();
    if (child->gc_ctx) {
        inimem1(child, 0);
        inimem2(child);
    }
}

static void task_destroy(ParallelTask* task)
{
    free(task->child_env.memory);
    gc_ctx_destroy(task->child_env.gc_ctx);
}

static void tasks_destroy(ParallelTask* tasks, int count)
{
    for (int i = 0; i < count; i++)
        task_destroy(&tasks[i]);
    free(tasks);
}

/* Combinators build lists from results, so check them while a task can stop. */
static int is_portable(pEnv child, Index node)
{
    switch (child->memory[node].op) {
    case FILE_:
    case LAZY_:
    case DICT_:
        return 0;
    case LIST_:
        for (Index n = child->memory[node].u.lis; n; n = child->memory[n].next)
            if (!is_portable(child, n))
                return 0;
        return 1;
    default:
        return 1;
    }
}

static void task_body(ParallelTask* task)
{
    pEnv child = &task->child_env;
    Node* src = task->parent_env->memory;

    if (task->whole_stack) {
        child->stck = parallel_copy_chain(child, src, task->inputs[0]);
    } else {
        for (int i = 0; i < task->ninputs; i++) {
            Index value = parallel_copy_value(child, src, task->inputs[i]);
            child->memory[value].next = child->stck;
            child->stck = value;
        }
    }
    exec_term(child, parallel_copy_chain(child, src, task->quotation));
    if (!child->stck || child->error.count
        || !is_portable(child, child->stck))
        abortexecution_(child, ABORT_RETRY);
    task->result = child->stck;
}

static void task_run(ParallelTask* task)
{
    pEnv child = &task->child_env;
    char stack_bottom;
    GC_Context* previous = gc_set_task_context(child->gc_ctx);

    child->gc_ctx->stack_bottom = &stack_bottom;
    if (setjmp(child->error_jmp) == 0)
        task_body(task);
    else
        task->failed = 1;
    gc_set_task_context(previous);
}

/*
 * Run all tasks. On any failure, destroy them and return 0. On success the
 * caller owns them.
 */
static int tasks_run(ParallelTask* tasks, int count)
{
    int failed = 0;

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < count; i++)
        task_run(&tasks[i]);
    for (int i = 0; i < count; i++)
        failed |= tasks[i].failed;
    if (failed)
        tasks_destroy(tasks, count);
    return !failed;
}

/* calloc and clone count tasks, or return NULL. */
static ParallelTask* tasks_create(pEnv env, int count)
{
    ParallelTask* tasks = calloc(count, sizeof(ParallelTask));

    if (!tasks)
        return NULL;
    for (int i = 0; i < count; i++) {
        task_init(&tasks[i], env);
        if (!tasks[i].child_env.memory) {
            tasks_destroy(tasks, i + 1);
            return NULL;
        }
    }
    return tasks;
}

/* Debuggers and profilers observe one execution order, in one environment. */
static int can_parallelize(pEnv env)
{
    return !env->config.debugging && !env->config.stepping
        && !env->config.breakpoints && !env->profiler.enabled;
}

static int list_length(pEnv env, Index n)
{
    int count = 0;

    for (; n; n = nextnode1(n))
        count++;
    return count;
}

static int pfork_parallel(pEnv env)
{
    ParallelTask* tasks;
    Index value;

    if (!can_parallelize(env) || !(tasks = tasks_create(env, 2)))
        return 0;
    for (int i = 0; i < 2; i++) {
        tasks[i].quotation = nodevalue(i ? SAVED1 : SAVED2).lis;
        tasks[i].inputs[0] = SAVED3; /* X and everything below it */
        tasks[i].whole_stack = 1;
    }
    if (!tasks_run(tasks, 2))
        return 0;
    env->stck = SAVED4;
    for (int i = 0; i < 2; i++) {
        value = parallel_copy_value(env, tasks[i].child_env.memory,
                                    tasks[i].result);
        nextnode1(value) = env->stck;
        env->stck = value;
    }
    tasks_destroy(tasks, 2);
    POP(env->dump);
    return 1;
}

static ParallelTask* tasks_over_list(pEnv env, int count)
{
    ParallelTask* tasks = tasks_create(env, count);
    Index n = nodevalue(SAVED2).lis;

    if (!tasks)
        return NULL;
    for (int i = 0; i < count; i++, n = nextnode1(n)) {
        tasks[i].quotation = nodevalue(SAVED1).lis;
        tasks[i].inputs[0] = n;
        tasks[i].ninputs = 1;
    }
    return tasks;
}

static int pmap_parallel(pEnv env)
{
    int count = list_length(env, nodevalue(SAVED2).lis);
    ParallelTask* tasks;
    Index value;

    if (count < PARALLEL_MIN_ITEMS || !can_parallelize(env)
        || !(tasks = tasks_over_list(env, count)) || !tasks_run(tasks, count))
        return 0;
    env->dump1 = LIST_NEWNODE(0, env->dump1);
    for (int i = count - 1; i >= 0; i--) {
        value = parallel_copy_value(env, tasks[i].child_env.memory,
                                    tasks[i].result);
        nextnode1(value) = DMP1;
        DMP1 = value;
    }
    env->stck = LIST_NEWNODE(DMP1, SAVED3);
    POP(env->dump1);
    tasks_destroy(tasks, count);
    POP(env->dump);
    return 1;
}

static int pfilter_parallel(pEnv env)
{
    int count = list_length(env, nodevalue(SAVED2).lis);
    ParallelTask* tasks;
    char* keep;
    Index temp;
    int i = 0;

    if (count < PARALLEL_MIN_ITEMS || !can_parallelize(env)
        || !(keep = malloc(count)))
        return 0;
    if (!(tasks = tasks_over_list(env, count)) || !tasks_run(tasks, count)) {
        free(keep);
        return 0;
    }
    for (i = 0; i < count; i++)
        keep[i] = pfilter_keeps(tasks[i].child_env.memory, tasks[i].result);
    tasks_destroy(tasks, count);

    env->dump1 = LIST_NEWNODE(nodevalue(SAVED2).lis, env->dump1);
    env->dump2 = LIST_NEWNODE(0, env->dump2); /* head of new list */
    env->dump3 = LIST_NEWNODE(0, env->dump3); /* tail of new list */
    for (i = 0; DMP1; DMP1 = nextnode1(DMP1), i++) {
        if (!keep[i])
            continue;
        temp = newnode2(env, DMP1, 0);
        if (DMP2)
            nextnode1(DMP3) = temp;
        else
            DMP2 = temp;
        DMP3 = temp;
    }
    free(keep);
    env->stck = LIST_NEWNODE(DMP2, SAVED3);
    POP(env->dump3);
    POP(env->dump2);
    POP(env->dump1);
    POP(env->dump);
    return 1;
}

/* One tree level: DMP1 holds the values, and receives the reduced values. */
static int preduce_level(pEnv env, int count)
{
    int pairs = count / 2;
    ParallelTask* tasks = tasks_create(env, pairs);
    Index n = DMP1, value;

    if (!tasks)
        return 0;
    for (int i = 0; i < pairs; i++) {
        tasks[i].quotation = nodevalue(SAVED1).lis;
        tasks[i].ninputs = 2;
        tasks[i].inputs[0] = n; /* the right operand ends on top, as in fold */
        n = nextnode1(n);
        tasks[i].inputs[1] = n;
        n = nextnode1(n);
    }
    if (!tasks_run(tasks, pairs))
        return 0;
    env->dump2 = LIST_NEWNODE(0, env->dump2);
    if (count % 2) { /* carry the last value over; the GC above moved it */
        n = DMP1;
        for (int i = 0; i < pairs * 2; i++)
            n = nextnode1(n);
        value = newnode2(env, n, 0);
        DMP2 = value;
    }
    for (int i = pairs - 1; i >= 0; i--) {
        value = parallel_copy_value(env, tasks[i].child_env.memory,
                                    tasks[i].result);
        nextnode1(value) = DMP2;
        DMP2 = value;
    }
    DMP1 = DMP2;
    POP(env->dump2);
    tasks_destroy(tasks, pairs);
    return 1;
}

static int preduce_parallel(pEnv env)
{
    int count = list_length(env, nodevalue(SAVED2).lis);

    if (count < PARALLEL_MIN_ITEMS || !can_parallelize(env))
        return 0;
    env->dump1 = LIST_NEWNODE(nodevalue(SAVED2).lis, env->dump1);
    for (; count > 1; count = count / 2 + count % 2) {
        if (!preduce_level(env, count)) {
            POP(env->dump1);
            return 0;
        }
    }
    env->stck = newnode2(env, DMP1, SAVED3);
    POP(env->dump1);
    POP(env->dump);
    return 1;
}
#endif /* JOY_PARALLEL */

/* Parallel operations */
/**
Q2  OK  3271  pfork  :  X [P1] [P2]  ->  R1 R2
[PARALLEL] Parallel fork: executes P1 and P2, each with X on top,
producing two results. With JOY_PARALLEL, both quotations may execute
concurrently using OpenMP.
*/
void pfork_(pEnv env)
{
    THREEPARAMS("pfork");
    TWOQUOTES("pfork");
    SAVESTACK;

#ifdef JOY_PARALLEL
    if (pfork_parallel(env))
        return;
#endif

    /*
     * Sequential implementation (fallback or when JOY_PARALLEL not defined).
     * Equivalent to cleave.
     */
    env->stck = SAVED3;
    exec_term(env, nodevalue(SAVED2).lis);               /* [P1] */
    env->dump1 = newnode2(env, env->stck, env->dump1); /* R1 */
    env->stck = SAVED3;
    exec_term(env, nodevalue(SAVED1).lis);               /* [P2] */
    env->dump1 = newnode2(env, env->stck, env->dump1); /* R2 */
    env->stck = env->dump1;
    env->dump1 = nextnode2(env->dump1);
    nextnode2(env->stck) = SAVED4;
    POP(env->dump);
}

/**
Q1  OK  3270  pmap  :  A [P]  ->  B
[PARALLEL] Parallel map: executes P on each member of list A,
collects results in list B. Order of results matches order of inputs.
With JOY_PARALLEL, executes in parallel using OpenMP.
*/
void pmap_(pEnv env)
{
    TWOPARAMS("pmap");
    ONEQUOTE("pmap");
    SAVESTACK;
    if (nodetype(SAVED2) != LIST_)
        BADAGGREGATE("pmap");

#ifdef JOY_PARALLEL
    if (pmap_parallel(env))
        return;
#endif

    /*
     * Sequential implementation (fallback or when JOY_PARALLEL not defined).
     * Equivalent to map on lists.
     */
    {
        Index temp;
        env->dump1 = LIST_NEWNODE(nodevalue(SAVED2).lis, env->dump1);
        env->dump2 = LIST_NEWNODE(0, env->dump2); /* head of new list */
        env->dump3 = LIST_NEWNODE(0, env->dump3); /* tail of new list */
        for (; DMP1; DMP1 = nextnode1(DMP1)) {
            env->stck = newnode2(env, DMP1, SAVED3);
            exec_term(env, nodevalue(SAVED1).lis);
            CHECKSTACK("pmap");
            temp = newnode2(env, env->stck, 0);
            if (!DMP2) { /* first element */
                DMP2 = temp;
                DMP3 = DMP2;
            } else { /* subsequent elements */
                nextnode1(DMP3) = temp;
                DMP3 = nextnode1(DMP3);
            }
        }
        env->stck = LIST_NEWNODE(DMP2, SAVED3);
        POP(env->dump3);
        POP(env->dump2);
        POP(env->dump1);
        POP(env->dump);
    }
}

/**
Q1  OK  3272  pfilter  :  A [P]  ->  B
[PARALLEL] Parallel filter: executes predicate P on each member of list A,
collects members where P returns true in list B. Order is preserved.
With JOY_PARALLEL, predicates execute in parallel using OpenMP.
*/
void pfilter_(pEnv env)
{
    TWOPARAMS("pfilter");
    ONEQUOTE("pfilter");
    SAVESTACK;
    if (nodetype(SAVED2) != LIST_)
        BADAGGREGATE("pfilter");

#ifdef JOY_PARALLEL
    if (pfilter_parallel(env))
        return;
#endif

    /*
     * Sequential implementation (fallback or when JOY_PARALLEL not defined).
     * Equivalent to filter.
     */
    {
        Index temp;
        env->dump1 = LIST_NEWNODE(nodevalue(SAVED2).lis, env->dump1);
        env->dump2 = LIST_NEWNODE(0, env->dump2); /* head of new list */
        env->dump3 = LIST_NEWNODE(0, env->dump3); /* tail of new list */
        for (; DMP1; DMP1 = nextnode1(DMP1)) {
            env->stck = newnode2(env, DMP1, SAVED3);
            exec_term(env, nodevalue(SAVED1).lis);
            CHECKSTACK("pfilter");
            if (pfilter_keeps(env->memory, env->stck)) {
                temp = newnode2(env, DMP1, 0);
                if (!DMP2) {
                    DMP2 = temp;
                    DMP3 = DMP2;
                } else {
                    nextnode1(DMP3) = temp;
                    DMP3 = nextnode1(DMP3);
                }
            }
        }
        env->stck = LIST_NEWNODE(DMP2, SAVED3);
        POP(env->dump3);
        POP(env->dump2);
        POP(env->dump1);
        POP(env->dump);
    }
}

/**
Q1  OK  3273  preduce  :  A [P]  ->  R
[PARALLEL] Parallel tree reduction: reduces list A using binary operation P.
P must be associative (e.g., +, *, max, min). Uses divide-and-conquer
parallelism to combine elements pairwise.
With JOY_PARALLEL, reduction levels execute in parallel using OpenMP.
For a list [a b c d], computes ((a P b) P (c P d)).
*/
void preduce_(pEnv env)
{
    TWOPARAMS("preduce");
    ONEQUOTE("preduce");
    SAVESTACK;
    if (nodetype(SAVED2) != LIST_)
        BADAGGREGATE("preduce");

    Index list = nodevalue(SAVED2).lis;

    /* Handle empty and singleton lists */
    if (!list) {
        execerror(env, "non-empty list", "preduce");
        return;
    }
    if (!nextnode1(list)) {
        /* Single element - return it */
        env->stck = newnode2(env, list, SAVED3);
        POP(env->dump);
        return;
    }

#ifdef JOY_PARALLEL
    if (preduce_parallel(env))
        return;
#endif

    /*
     * Sequential implementation (fallback or when JOY_PARALLEL not defined).
     * Left fold: ((a P b) P c) P d ...
     */
    {
        env->dump1 = LIST_NEWNODE(nodevalue(SAVED2).lis, env->dump1);

        /* Start with first element */
        env->stck = newnode2(env, DMP1, SAVED3);
        DMP1 = nextnode1(DMP1);

        /* Fold remaining elements */
        for (; DMP1; DMP1 = nextnode1(DMP1)) {
            /* Stack has accumulator, push next element */
            env->stck = newnode2(env, DMP1, env->stck);
            exec_term(env, nodevalue(SAVED1).lis);
            CHECKSTACK("preduce");
        }

        /* Result is on stack */
        Index result = env->stck;
        env->stck = newnode2(env, result, SAVED3);

        POP(env->dump1);
        POP(env->dump);
    }
}

