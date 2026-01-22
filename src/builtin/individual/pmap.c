/*
    module  : pmap.c
    version : 2.0
    date    : 01/21/26
*/
#ifndef PMAP_C
#define PMAP_C

#ifdef JOY_PARALLEL
#include "parallel.h"
#endif

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
#endif
