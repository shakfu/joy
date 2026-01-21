/*
 *  module  : parallel.h
 *  version : 1.0
 *  date    : 01/21/26
 *
 *  Infrastructure for parallel execution of Joy programs.
 *  Provides environment cloning and result copying between GC contexts.
 */
#ifndef PARALLEL_H
#define PARALLEL_H

#include "globals.h"

#ifdef JOY_PARALLEL

/* Disable pedantic warnings for omp.h (has large enum values) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <omp.h>
#pragma GCC diagnostic pop

/*
 * ParallelTask - Structure for parallel task execution
 */
typedef struct ParallelTask {
    pEnv parent_env;        /* parent environment (for result copying) */
    Env child_env;          /* cloned child environment */
    Index quotation;        /* code to execute */
    Index input;            /* input value (for pmap) */
    Index result;           /* output after execution */
    int has_error;          /* error flag */
    char error_msg[256];    /* error message if failed */
} ParallelTask;

/*
 * Clone an environment for parallel execution.
 * The child shares read-only symbol tables but has isolated:
 * - Stack
 * - GC context
 * - Error handling
 */
static inline void env_clone_for_parallel(pEnv parent, pEnv child)
{
    /* Zero out the child */
    memset(child, 0, sizeof(Env));

    /* Copy configuration flags */
    child->autoput = parent->autoput;
    child->echoflag = parent->echoflag;
    child->tracegc = parent->tracegc;
    child->undeferror = parent->undeferror;
    child->debugging = parent->debugging;
    child->ignore = parent->ignore;
    child->overwrite = parent->overwrite;

    /* Share read-only tables (no locking needed for reads) */
    child->symtab = parent->symtab;
    child->hash = parent->hash;
    child->prim = parent->prim;

    /* Store parent's memory for symbol body lookup */
    child->parent_memory = parent->memory;

#ifdef NOBDW
    /* Create isolated GC context for child */
    child->gc_ctx = gc_ctx_create();

    /* Initialize NOBDW memory for child */
    inimem1(child, 0);
    inimem2(child);
#endif

    /* Fresh execution state */
    child->stck = 0;
    child->prog = 0;
#ifdef NOBDW
    child->dump = child->dump1 = child->dump2 = 0;
    child->dump3 = child->dump4 = child->dump5 = 0;
    child->conts = 0;
#endif

    /* Isolated error handling */
    child->error_message[0] = '\0';
    child->error_line = 0;
    child->error_column = 0;

    /* No I/O in parallel tasks */
    child->io.user_data = NULL;
    child->io.read_char = NULL;
    child->io.write_char = NULL;
    child->io.write_string = NULL;
    child->io.on_error = NULL;
}

/*
 * Destroy a parallel child environment.
 */
static inline void env_destroy_parallel(pEnv child)
{
#ifdef NOBDW
    /* Free NOBDW memory */
    if (child->memory) {
        free(child->memory);
        child->memory = NULL;
    }
    /* Destroy GC context */
    if (child->gc_ctx) {
        gc_ctx_destroy(child->gc_ctx);
        child->gc_ctx = NULL;
    }
#endif
}

/*
 * Deep copy a node from child context to parent context.
 * This is needed because child GC context will be destroyed after task completion.
 */
static Index copy_node_to_parent(pEnv parent, pEnv child, Index node);

static inline Index copy_node_to_parent_impl(pEnv parent, pEnv child, Index node)
{
    if (!node)
        return 0;

#ifdef NOBDW
    Types u;
    Operator op = child->memory[node].op;

    switch (op) {
    case INTEGER_:
    case BOOLEAN_:
    case CHAR_:
    case SET_:
    case FLOAT_:
        u = child->memory[node].u;
        break;

    case STRING_:
    case BIGNUM_:
        /* Duplicate string in parent's context */
        u.str = GC_CTX_STRDUP(parent, (char*)&child->memory[node].u);
        break;

    case LIST_:
        /* Recursively copy list contents */
        u.lis = copy_node_to_parent(parent, child, child->memory[node].u.lis);
        break;

    case USR_:
        /* User symbols are shared via symtab, just copy the index */
        u = child->memory[node].u;
        break;

    default:
        /* Other types - copy as-is (may need special handling) */
        u = child->memory[node].u;
        break;
    }

    /* Copy the next pointer recursively */
    Index next = copy_node_to_parent(parent, child, child->memory[node].next);

    return newnode(parent, op, u, next);
#else
    /* Non-NOBDW mode uses BDW GC which is shared, no copying needed */
    return node;
#endif
}

static inline Index copy_node_to_parent(pEnv parent, pEnv child, Index node)
{
    return copy_node_to_parent_impl(parent, child, node);
}

/*
 * Execute a parallel task.
 * Called by OpenMP on a worker thread.
 */
static inline void execute_parallel_task(void* context)
{
    ParallelTask* task = (ParallelTask*)context;
    pEnv env = &task->child_env;

#ifdef NOBDW
    /* Set up stack scanning for this thread */
    char stack_marker;
    if (env->gc_ctx)
        env->gc_ctx->stack_bottom = &stack_marker;
#endif

    /* Push input onto stack if present */
    if (task->input) {
#ifdef NOBDW
        /* Copy input from parent to child context */
        env->stck = copy_node_to_parent(env, task->parent_env, task->input);
#else
        env->stck = task->input;
#endif
    }

    /* Execute with error handling */
    if (setjmp(env->error_jmp) == 0) {
        exeterm(env, task->quotation);
        task->result = env->stck;
        task->has_error = 0;
    } else {
        task->has_error = 1;
        strncpy(task->error_msg, env->error_message, 255);
        task->error_msg[255] = '\0';
        task->result = 0;
    }
}

#endif /* JOY_PARALLEL */

#endif /* PARALLEL_H */
