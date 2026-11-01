/*
 *  module  : parallel.h
 *  version : 2.0
 *  date    : 11/01/26
 *
 *  Tasks for the parallel combinators. A task runs in a cloned environment
 *  with its own node memory and GC context. See doc/parallel.md.
 */
#ifndef PARALLEL_H
#define PARALLEL_H

#include "globals.h"

#ifdef JOY_PARALLEL

typedef struct ParallelTask {
    pEnv parent_env;  /* holds the inputs; frozen while tasks run */
    Env child_env;
    Index quotation;  /* in parent memory */
    Index inputs[2];  /* in parent memory; inputs[ninputs - 1] ends on top */
    int ninputs;
    int whole_stack;  /* inputs[0] is a chain: copy it, not just its head */
    Index result;     /* in child memory */
    int failed;       /* replay the combinator sequentially */
} ParallelTask;

/* Copy a node chain, or one node with next = 0, from src into dst's memory. */
Index parallel_copy_chain(pEnv dst, Node* src, Index node);
Index parallel_copy_value(pEnv dst, Node* src, Index node);

#endif /* JOY_PARALLEL */

#endif /* PARALLEL_H */
