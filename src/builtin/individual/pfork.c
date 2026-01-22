/*
    module  : pfork.c
    version : 2.0
    date    : 01/21/26
*/
#ifndef PFORK_C
#define PFORK_C

#ifdef JOY_PARALLEL
#include "parallel.h"
#endif

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
#endif
