/*
 *  module  : parallel.c
 *  version : 1.1
 *  date    : 01/22/26
 *
 *  Grouped parallel builtins: pfork, pmap, pfilter, preduce
 */
#include "globals.h"

/* Parallel operations */
/**
Q2  OK  3271  pfork  :  X [P1] [P2]  ->  R1 R2
[PARALLEL] Parallel fork: executes P1 and P2, each with X on top,
producing two results. With JOY_PARALLEL, both quotations execute
concurrently using OpenMP.
*/
void pfork_(pEnv env)
{
    THREEPARAMS("pfork");
    TWOQUOTES("pfork");
    SAVESTACK;

#ifdef JOY_PARALLEL
    /*
     * Parallel implementation using OpenMP sections.
     * Both quotations execute concurrently with X on the stack.
     */
    Index quot1 = nodevalue(SAVED2).lis;  /* [P1] */
    Index quot2 = nodevalue(SAVED1).lis;  /* [P2] */
    Index input = SAVED3;                  /* X */

    /* Two tasks for the two branches */
    ParallelTask tasks[2];
    int init_ok = 1;

    /* Initialize both tasks */
    for (int i = 0; i < 2; i++) {
        tasks[i].parent_env = env;
        tasks[i].quotation = (i == 0) ? quot1 : quot2;
        tasks[i].input = input;
        tasks[i].result = 0;
        tasks[i].has_error = 0;
        tasks[i].error_msg[0] = '\0';
        env_clone_for_parallel(env, &tasks[i].child_env);
        if (!tasks[i].child_env.memory) {
            init_ok = 0;
            /* Clean up already initialized */
            for (int j = 0; j < i; j++) {
                env_destroy_parallel(&tasks[j].child_env);
            }
            break;
        }
    }

    if (!init_ok) {
        goto sequential;
    }

    /* Execute both branches in parallel */
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            ParallelTask* task = &tasks[0];
            pEnv child = &task->child_env;

#ifdef NOBDW
            char stack_marker;
            if (child->gc_ctx)
                child->gc_ctx->stack_bottom = &stack_marker;

            /* Copy input X to child context */
            Index copied_input = copy_node_to_parent(child, env, task->input);
            child->stck = copied_input;

            /* Copy quotation to child context */
            Index child_quot = copy_node_to_parent(child, env, task->quotation);
#else
            child->stck = task->input;
            Index child_quot = task->quotation;
#endif

            if (setjmp(child->error_jmp) == 0) {
                exec_term(child, child_quot);
                task->result = child->stck;
                task->has_error = 0;
            } else {
                task->has_error = 1;
                strncpy(task->error_msg, child->error.message, 255);
                task->error_msg[255] = '\0';
                task->result = 0;
            }
        }

        #pragma omp section
        {
            ParallelTask* task = &tasks[1];
            pEnv child = &task->child_env;

#ifdef NOBDW
            char stack_marker;
            if (child->gc_ctx)
                child->gc_ctx->stack_bottom = &stack_marker;

            /* Copy input X to child context */
            Index copied_input = copy_node_to_parent(child, env, task->input);
            child->stck = copied_input;

            /* Copy quotation to child context */
            Index child_quot = copy_node_to_parent(child, env, task->quotation);
#else
            child->stck = task->input;
            Index child_quot = task->quotation;
#endif

            if (setjmp(child->error_jmp) == 0) {
                exec_term(child, child_quot);
                task->result = child->stck;
                task->has_error = 0;
            } else {
                task->has_error = 1;
                strncpy(task->error_msg, child->error.message, 255);
                task->error_msg[255] = '\0';
                task->result = 0;
            }
        }
    }

    /* Check for errors */
    for (int i = 0; i < 2; i++) {
        if (tasks[i].has_error) {
            char error_copy[256];
            strncpy(error_copy, tasks[i].error_msg, 255);
            error_copy[255] = '\0';

            for (int j = 0; j < 2; j++) {
                env_destroy_parallel(&tasks[j].child_env);
            }
            POP(env->dump);
            execerror(env, error_copy, "pfork");
            return;
        }
    }

    /* Copy results back and build stack */
    /* R1 from P1, R2 from P2 - stack should have R2 on top, R1 below */
#ifdef NOBDW
    Index r1 = copy_node_to_parent(env, &tasks[0].child_env, tasks[0].result);
    Index r2 = copy_node_to_parent(env, &tasks[1].child_env, tasks[1].result);
#else
    Index r1 = tasks[0].result;
    Index r2 = tasks[1].result;
#endif

    /* Clean up tasks */
    for (int i = 0; i < 2; i++) {
        env_destroy_parallel(&tasks[i].child_env);
    }

    /* Build result stack: R1 R2 (R2 on top) */
    env->stck = SAVED4;  /* restore stack below X [P1] [P2] */
    env->stck = newnode2(env, r1, env->stck);
    env->stck = newnode2(env, r2, env->stck);

    POP(env->dump);
    return;

sequential:
#endif /* JOY_PARALLEL */

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
    /*
     * Parallel implementation using OpenMP.
     * Each list element is processed by a separate task with its own
     * cloned environment and GC context.
     */
    Index list = nodevalue(SAVED2).lis;
    Index quotation = nodevalue(SAVED1).lis;

    /* Count elements */
    int count = 0;
    for (Index n = list; n; n = nextnode1(n))
        count++;

    if (count == 0) {
        /* Empty list -> empty result */
        env->stck = LIST_NEWNODE(0, SAVED3);
        POP(env->dump);
        return;
    }

    /* For small lists, use sequential execution (overhead not worth it) */
    if (count < 4) {
        goto sequential;
    }

    /* Allocate task array */
    ParallelTask* tasks = (ParallelTask*)malloc(count * sizeof(ParallelTask));
    if (!tasks) {
        goto sequential;  /* Fall back to sequential on allocation failure */
    }

    /* Build array of input nodes for indexed access */
    Index* inputs = (Index*)malloc(count * sizeof(Index));
    if (!inputs) {
        free(tasks);
        goto sequential;
    }

    Index n = list;
    for (int i = 0; i < count; i++, n = nextnode1(n)) {
        inputs[i] = n;
    }

    /* Initialize all tasks */
    int init_failed = 0;
    for (int i = 0; i < count; i++) {
        tasks[i].parent_env = env;
        tasks[i].quotation = quotation;
        tasks[i].input = inputs[i];
        tasks[i].result = 0;
        tasks[i].has_error = 0;
        tasks[i].error_msg[0] = '\0';
        env_clone_for_parallel(env, &tasks[i].child_env);
        if (!tasks[i].child_env.memory) {
            init_failed = 1;
            /* Clean up already initialized tasks */
            for (int j = 0; j < i; j++) {
                env_destroy_parallel(&tasks[j].child_env);
            }
            break;
        }
    }

    if (init_failed) {
        free(inputs);
        free(tasks);
        goto sequential;
    }

    /* Execute tasks in parallel */
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < count; i++) {
        ParallelTask* task = &tasks[i];
        pEnv child = &task->child_env;

#ifdef NOBDW
        /* Set up stack scanning for this thread */
        char stack_marker;
        if (child->gc_ctx)
            child->gc_ctx->stack_bottom = &stack_marker;

        /* Copy input from parent to child context */
        Operator input_op = env->memory[task->input].op;
        Types input_u = env->memory[task->input].u;

        /* For complex types, deep copy */
        if (input_op == STRING_ || input_op == BIGNUM_) {
            input_u.str = GC_CTX_STRDUP(child, (char*)&env->memory[task->input].u);
        } else if (input_op == LIST_) {
            input_u.lis = copy_node_to_parent(child, env, env->memory[task->input].u.lis);
        }

        child->stck = newnode(child, input_op, input_u, 0);

        /* Copy quotation to child context */
        Index child_quot = copy_node_to_parent(child, env, task->quotation);
#else
        child->stck = task->input;
        Index child_quot = task->quotation;
#endif

        /* Execute with error handling */
        if (setjmp(child->error_jmp) == 0) {
            exec_term(child, child_quot);
            task->result = child->stck;
            task->has_error = 0;
        } else {
            task->has_error = 1;
            strncpy(task->error_msg, child->error.message, 255);
            task->error_msg[255] = '\0';
            task->result = 0;
        }
    }

    /* Check for errors and collect results */
    int first_error = -1;
    for (int i = 0; i < count; i++) {
        if (tasks[i].has_error) {
            first_error = i;
            break;
        }
    }

    if (first_error >= 0) {
        /* Clean up all tasks */
        char error_copy[256];
        strncpy(error_copy, tasks[first_error].error_msg, 255);
        error_copy[255] = '\0';

        for (int i = 0; i < count; i++) {
            env_destroy_parallel(&tasks[i].child_env);
        }
        free(inputs);
        free(tasks);
        POP(env->dump);
        execerror(env, error_copy, "pmap");
        return;
    }

    /* Build result list (in reverse order, then it's correct) */
    /* Root the result list in dump4 to protect from GC during construction */
    env->dump4 = LIST_NEWNODE(0, env->dump4);
    for (int i = count - 1; i >= 0; i--) {
        /* Copy result from child to parent context */
        Index child_result = tasks[i].result;
        if (child_result) {
#ifdef NOBDW
            Index copied = copy_node_to_parent(env, &tasks[i].child_env, child_result);
            nodevalue(env->dump4).lis = newnode2(env, copied, nodevalue(env->dump4).lis);
#else
            nodevalue(env->dump4).lis = newnode2(env, child_result, nodevalue(env->dump4).lis);
#endif
        }
    }
    Index result_list = nodevalue(env->dump4).lis;
    POP(env->dump4);

    /* Clean up tasks */
    for (int i = 0; i < count; i++) {
        env_destroy_parallel(&tasks[i].child_env);
    }
    free(inputs);
    free(tasks);

    env->stck = LIST_NEWNODE(result_list, SAVED3);
    POP(env->dump);
    return;

sequential:
#endif /* JOY_PARALLEL */

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
    /*
     * Parallel implementation using OpenMP.
     * Each predicate is evaluated in parallel, then results are collected.
     */
    Index list = nodevalue(SAVED2).lis;
    Index quotation = nodevalue(SAVED1).lis;

    /* Count elements */
    int count = 0;
    for (Index n = list; n; n = nextnode1(n))
        count++;

    if (count == 0) {
        /* Empty list -> empty result */
        env->stck = LIST_NEWNODE(0, SAVED3);
        POP(env->dump);
        return;
    }

    /* For small lists, use sequential execution */
    if (count < 4) {
        goto sequential;
    }

    /* Allocate task array */
    ParallelTask* tasks = (ParallelTask*)malloc(count * sizeof(ParallelTask));
    if (!tasks) {
        goto sequential;
    }

    /* Build array of input nodes */
    Index* inputs = (Index*)malloc(count * sizeof(Index));
    if (!inputs) {
        free(tasks);
        goto sequential;
    }

    /* Array to store predicate results (1 = keep, 0 = filter out) */
    int* keep = (int*)malloc(count * sizeof(int));
    if (!keep) {
        free(inputs);
        free(tasks);
        goto sequential;
    }

    Index n = list;
    for (int i = 0; i < count; i++, n = nextnode1(n)) {
        inputs[i] = n;
        keep[i] = 0;
    }

    /* Initialize all tasks */
    int init_failed = 0;
    for (int i = 0; i < count; i++) {
        tasks[i].parent_env = env;
        tasks[i].quotation = quotation;
        tasks[i].input = inputs[i];
        tasks[i].result = 0;
        tasks[i].has_error = 0;
        tasks[i].error_msg[0] = '\0';
        env_clone_for_parallel(env, &tasks[i].child_env);
        if (!tasks[i].child_env.memory) {
            init_failed = 1;
            for (int j = 0; j < i; j++) {
                env_destroy_parallel(&tasks[j].child_env);
            }
            break;
        }
    }

    if (init_failed) {
        free(keep);
        free(inputs);
        free(tasks);
        goto sequential;
    }

    /* Execute predicates in parallel */
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < count; i++) {
        ParallelTask* task = &tasks[i];
        pEnv child = &task->child_env;

#ifdef NOBDW
        char stack_marker;
        if (child->gc_ctx)
            child->gc_ctx->stack_bottom = &stack_marker;

        /* Copy input from parent to child context */
        Operator input_op = env->memory[task->input].op;
        Types input_u = env->memory[task->input].u;

        if (input_op == STRING_ || input_op == BIGNUM_) {
            input_u.str = GC_CTX_STRDUP(child, (char*)&env->memory[task->input].u);
        } else if (input_op == LIST_) {
            input_u.lis = copy_node_to_parent(child, env, env->memory[task->input].u.lis);
        }

        child->stck = newnode(child, input_op, input_u, 0);

        /* Copy quotation to child context */
        Index child_quot = copy_node_to_parent(child, env, task->quotation);
#else
        child->stck = task->input;
        Index child_quot = task->quotation;
#endif

        if (setjmp(child->error_jmp) == 0) {
            exec_term(child, child_quot);
            task->result = child->stck;
            task->has_error = 0;
        } else {
            task->has_error = 1;
            strncpy(task->error_msg, child->error.message, 255);
            task->error_msg[255] = '\0';
            task->result = 0;
        }
    }

    /* Check for errors */
    int first_error = -1;
    for (int i = 0; i < count; i++) {
        if (tasks[i].has_error) {
            first_error = i;
            break;
        }
    }

    if (first_error >= 0) {
        char error_copy[256];
        strncpy(error_copy, tasks[first_error].error_msg, 255);
        error_copy[255] = '\0';

        for (int i = 0; i < count; i++) {
            env_destroy_parallel(&tasks[i].child_env);
        }
        free(keep);
        free(inputs);
        free(tasks);
        POP(env->dump);
        execerror(env, error_copy, "pfilter");
        return;
    }

    /* Evaluate which elements to keep */
    for (int i = 0; i < count; i++) {
        Index result = tasks[i].result;
        if (result) {
            /* Check if result is truthy */
            Operator op = tasks[i].child_env.memory[result].op;
            Types u = tasks[i].child_env.memory[result].u;
            if (op == BOOLEAN_ || op == CHAR_ || op == INTEGER_) {
                keep[i] = (u.num != 0);
            } else if (op == FLOAT_) {
                keep[i] = (u.dbl != 0.0);
            } else if (op == LIST_) {
                keep[i] = (u.lis != 0);  /* non-empty list is truthy */
            } else if (op == STRING_) {
                keep[i] = (u.str != NULL && u.str[0] != '\0');
            } else {
                keep[i] = 1;  /* other types are truthy */
            }
        }
    }

    /* Build result list with elements where keep[i] is true */
    env->dump4 = LIST_NEWNODE(0, env->dump4);
    for (int i = count - 1; i >= 0; i--) {
        if (keep[i]) {
            /* Copy original input element (not predicate result) */
#ifdef NOBDW
            Index copied = copy_node_to_parent(env, &tasks[i].child_env,
                copy_node_to_parent(&tasks[i].child_env, env, inputs[i]));
            /* Actually, we should copy from parent directly since inputs[i] is in parent */
            Operator input_op = env->memory[inputs[i]].op;
            Types input_u = env->memory[inputs[i]].u;
            copied = newnode(env, input_op, input_u, 0);
            if (input_op == STRING_ || input_op == BIGNUM_) {
                /* String already in parent, just reference */
            } else if (input_op == LIST_) {
                /* Deep copy not needed, it's already in parent */
            }
            nodevalue(env->dump4).lis = newnode2(env, copied, nodevalue(env->dump4).lis);
#else
            nodevalue(env->dump4).lis = newnode2(env, inputs[i], nodevalue(env->dump4).lis);
#endif
        }
    }
    Index result_list = nodevalue(env->dump4).lis;
    POP(env->dump4);

    /* Clean up */
    for (int i = 0; i < count; i++) {
        env_destroy_parallel(&tasks[i].child_env);
    }
    free(keep);
    free(inputs);
    free(tasks);

    env->stck = LIST_NEWNODE(result_list, SAVED3);
    POP(env->dump);
    return;

sequential:
#endif /* JOY_PARALLEL */

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
            if (nodevalue(env->stck).num) {  /* truthy? */
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
    /*
     * Parallel tree reduction using OpenMP.
     * Build levels of the tree bottom-up, combining pairs in parallel.
     */
    Index quotation = nodevalue(SAVED1).lis;

    /* Count elements */
    int count = 0;
    for (Index n = list; n; n = nextnode1(n))
        count++;

    /* For small lists, use sequential */
    if (count < 4) {
        goto sequential;
    }

    /* Build array of current values */
    Index* values = (Index*)malloc(count * sizeof(Index));
    if (!values) {
        goto sequential;
    }

    Index n = list;
    for (int i = 0; i < count; i++, n = nextnode1(n)) {
        values[i] = n;
    }

    /* Reduce in parallel levels */
    int current_count = count;
    while (current_count > 1) {
        int pairs = current_count / 2;
        int has_odd = current_count % 2;

        /* Allocate tasks for this level */
        ParallelTask* tasks = (ParallelTask*)malloc(pairs * sizeof(ParallelTask));
        if (!tasks) {
            free(values);
            goto sequential;
        }

        /* Initialize tasks */
        int init_failed = 0;
        for (int i = 0; i < pairs; i++) {
            tasks[i].parent_env = env;
            tasks[i].quotation = quotation;
            tasks[i].input = 0;  /* Will use two inputs */
            tasks[i].result = 0;
            tasks[i].has_error = 0;
            tasks[i].error_msg[0] = '\0';
            env_clone_for_parallel(env, &tasks[i].child_env);
            if (!tasks[i].child_env.memory) {
                init_failed = 1;
                for (int j = 0; j < i; j++) {
                    env_destroy_parallel(&tasks[j].child_env);
                }
                break;
            }
        }

        if (init_failed) {
            free(tasks);
            free(values);
            goto sequential;
        }

        /* Process pairs in parallel */
        #pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < pairs; i++) {
            ParallelTask* task = &tasks[i];
            pEnv child = &task->child_env;
            Index left = values[i * 2];
            Index right = values[i * 2 + 1];

#ifdef NOBDW
            char stack_marker;
            if (child->gc_ctx)
                child->gc_ctx->stack_bottom = &stack_marker;

            /* Copy both values to child - push right first, then left */
            Operator right_op = env->memory[right].op;
            Types right_u = env->memory[right].u;
            if (right_op == LIST_) {
                right_u.lis = copy_node_to_parent(child, env, env->memory[right].u.lis);
            }
            child->stck = newnode(child, right_op, right_u, 0);

            Operator left_op = env->memory[left].op;
            Types left_u = env->memory[left].u;
            if (left_op == LIST_) {
                left_u.lis = copy_node_to_parent(child, env, env->memory[left].u.lis);
            }
            child->stck = newnode(child, left_op, left_u, child->stck);

            Index child_quot = copy_node_to_parent(child, env, task->quotation);
#else
            /* Push right, then left (so left is on top) */
            child->stck = newnode2(env, right, 0);
            child->stck = newnode2(env, left, child->stck);
            Index child_quot = task->quotation;
#endif

            if (setjmp(child->error_jmp) == 0) {
                exec_term(child, child_quot);
                task->result = child->stck;
                task->has_error = 0;
            } else {
                task->has_error = 1;
                strncpy(task->error_msg, child->error.message, 255);
                task->error_msg[255] = '\0';
                task->result = 0;
            }
        }

        /* Check for errors */
        int first_error = -1;
        for (int i = 0; i < pairs; i++) {
            if (tasks[i].has_error) {
                first_error = i;
                break;
            }
        }

        if (first_error >= 0) {
            char error_copy[256];
            strncpy(error_copy, tasks[first_error].error_msg, 255);
            error_copy[255] = '\0';

            for (int i = 0; i < pairs; i++) {
                env_destroy_parallel(&tasks[i].child_env);
            }
            free(tasks);
            free(values);
            POP(env->dump);
            execerror(env, error_copy, "preduce");
            return;
        }

        /* Collect results for next level */
        Index* new_values = (Index*)malloc((pairs + has_odd) * sizeof(Index));
        if (!new_values) {
            for (int i = 0; i < pairs; i++) {
                env_destroy_parallel(&tasks[i].child_env);
            }
            free(tasks);
            free(values);
            goto sequential;
        }

        for (int i = 0; i < pairs; i++) {
#ifdef NOBDW
            new_values[i] = copy_node_to_parent(env, &tasks[i].child_env, tasks[i].result);
#else
            new_values[i] = tasks[i].result;
#endif
        }

        /* If odd count, carry over the last element */
        if (has_odd) {
            new_values[pairs] = values[current_count - 1];
        }

        /* Clean up this level */
        for (int i = 0; i < pairs; i++) {
            env_destroy_parallel(&tasks[i].child_env);
        }
        free(tasks);
        free(values);

        values = new_values;
        current_count = pairs + has_odd;
    }

    /* Final result */
    env->stck = newnode2(env, values[0], SAVED3);
    free(values);
    POP(env->dump);
    return;

sequential:
#endif /* JOY_PARALLEL */

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

