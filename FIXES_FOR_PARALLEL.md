# Fixes for Parallel Execution

This document describes the bugs found and fixed to enable `times` (and other combinators) to work correctly in parallel execution contexts (`pmap`, `pfork`).

## Executive Summary

Two stack overflow bugs were found in recursive functions that traverse linked lists. Both followed the same anti-pattern: recursing on both nested LIST_ contents AND the `next` pointer chain. The fix was to make the `next` chain traversal iterative while keeping recursion only for nested lists (which is bounded by actual nesting depth, not list length).

---

## Bug #1: `copy_node_to_parent` in `include/parallel.h`

### Problem

The function that copies results from child execution contexts back to the parent was doubly recursive:

```c
// OLD CODE (buggy)
static inline Index copy_node_to_parent_impl(pEnv parent, pEnv child, Index node)
{
    // ... copy node value ...

    if (op == LIST_)
        u.lis = copy_node_to_parent(parent, child, child->memory[node].u.lis);  // Recursion #1

    // PROBLEM: Recursion on next chain causes stack overflow for long lists
    Index next = copy_node_to_parent(parent, child, child->memory[node].next);  // Recursion #2

    return newnode(parent, op, u, next);
}
```

### Symptom

Stack overflow (segfault) when `pmap` tried to copy results containing 130,000+ nodes back to the parent context.

### Root Cause

After executing `[132000 [dup * 2 mod] times]`, the child's stack/dump contains a chain of 130,000+ nodes linked by `next` pointers. Each node in the chain caused a recursive call, exhausting the C stack.

### Fix

Split into two functions:
1. `copy_single_node` - copies one node's value (with recursion for nested LIST_ only)
2. `copy_node_to_parent` - iterates the `next` chain

```c
// NEW CODE (fixed)
static inline Index copy_single_node(pEnv parent, pEnv child, Index node)
{
    // Copy node value, recurse only for LIST_ contents (bounded by nesting depth)
    if (op == LIST_)
        u.lis = copy_node_to_parent(parent, child, child->memory[node].u.lis);

    return newnode(parent, op, u, 0);  // next = 0, will be linked later
}

static inline Index copy_node_to_parent(pEnv parent, pEnv child, Index node)
{
    // GC protection for head and tail
    parent->dump4 = newnode(parent, LIST_, u_prot, parent->dump4);
    parent->dump5 = newnode(parent, LIST_, u_prot, parent->dump5);

    // ITERATIVE traversal of next chain
    while (current) {
        Index new_node = copy_single_node(parent, child, current);

        // Link nodes as we go
        if (!head) {
            head = tail = new_node;
        } else {
            parent->memory[tail].next = new_node;
            tail = new_node;
        }
        current = child->memory[current].next;  // Iteration, not recursion
    }

    // Pop GC protection and return
    return head;
}
```

### Key Detail: GC Protection

During the iterative copy, `newnode` can trigger garbage collection. Local variables like `head` and `tail` are not GC roots, so they would become stale after GC moves nodes. The fix stores them in `dump4` and `dump5` (which ARE GC roots) and reads them back after each allocation.

---

## Bug #2: `copy` in `src/utils.c` (GC copy function)

### Problem

The garbage collector's node copying function had the same recursive pattern:

```c
// OLD CODE (buggy)
static Index copy(pEnv env, Index n)
{
    // ... copy node ...

    if (op == LIST_)
        env->memory[temp].u.lis = copy(env, env->old_memory[n].u.lis);  // Recursion #1

    // PROBLEM: Same recursive pattern on next chain
    env->memory[temp].next = copy(env, env->old_memory[n].next);  // Recursion #2

    return temp;
}
```

### Symptom

Stack overflow during garbage collection when the heap contained long linked lists (200,000+ nodes).

### Fix

Same pattern - split into `copy_one` (single node) and `copy` (iterative chain):

```c
// NEW CODE (fixed)
static Index copy_one(pEnv env, Index n)
{
    // Copy single node, recurse only for LIST_ contents
    if (op == LIST_)
        env->memory[temp].u.lis = copy(env, env->old_memory[n].u.lis);

    // Mark as copied
    env->old_memory[n].op = COPIED_;
    env->old_memory[n].u.lis = temp;
    return temp;
}

static Index copy(pEnv env, Index n)
{
    // ITERATIVE traversal
    while (current >= env->mem_low) {
        if (env->old_memory[current].op == COPIED_) {
            // Already copied - link and break
            if (tail) env->memory[tail].next = env->old_memory[current].u.lis;
            break;
        }

        Index new_node = copy_one(env, current);
        // Link nodes...
        current = old_next;  // Iteration, not recursion
    }
    return head;
}
```

---

## General Pattern: Recursive List Traversal

### The Anti-Pattern

```c
Index process_list(Index node) {
    if (!node) return 0;

    // Process nested structures (OK - bounded by nesting depth)
    if (is_list(node))
        process_nested(node->contents);

    // DANGER: Recursive call on next pointer
    return make_node(node->value, process_list(node->next));
}
```

This pattern causes stack overflow when list length exceeds stack depth (~130,000 on typical systems).

### The Fix Pattern

```c
Index process_list(Index node) {
    if (!node) return 0;

    Index head = 0, tail = 0;

    while (node) {
        // Process nested structures (still recursive - OK)
        if (is_list(node))
            process_nested(node->contents);

        Index new_node = make_node(node->value, 0);

        // Link iteratively
        if (!head) head = tail = new_node;
        else { tail->next = new_node; tail = new_node; }

        node = node->next;  // ITERATION instead of recursion
    }
    return head;
}
```

---

## Other Combinators to Check

Based on this pattern, the following areas should be reviewed:

### 1. `src/interp.c` - `copy_body_from_parent`

This function copies symbol bodies from parent memory to child memory. It has the same recursive structure:

```c
static Index copy_body_from_parent(pEnv env, Index node)
{
    // ...
    case LIST_:
        u.lis = copy_body_from_parent(env, pmem[node].u.lis);  // OK
        break;
    // ...
    Index next = copy_body_from_parent(env, pmem[node].next);  // POTENTIAL ISSUE
    return newnode(env, op, u, next);
}
```

**Risk**: If a symbol body contains a very long list, this could overflow. However, symbol bodies are typically small (the code of a definition), so this is lower risk than runtime data structures.

**Recommendation**: Convert to iterative if long symbol bodies are expected.

### 2. Other recursive combinators

The combinators listed in TODO.md (`while`, `linrec`, `tailrec`, `binrec`, etc.) operate on Joy-level recursion, not C-level linked list traversal. They should work correctly now that the underlying infrastructure is fixed.

However, if any of them internally build very long intermediate lists that get copied or garbage collected, they would benefit from the fixes above.

### 3. `writeterm` and similar output functions

Functions that traverse lists for printing/serialization may also have recursive patterns. These are typically bounded by output size limits but should be reviewed if stack overflows occur during I/O.

---

## Testing Recommendations

### Stress Tests for Parallel Execution

```joy
(* Test high iteration counts *)
[1 2 3 4 5 6 7 8] [100000 [dup * 2 mod] times] pmap.
[1 2 3 4 5 6 7 8] [500000 [dup * 2 mod] times] pmap.
[1 2 3 4 5 6 7 8] [1000000 [dup * 2 mod] times] pmap.

(* Test with nested operations *)
[1 2 3 4] [1000 [[1 2 3] size +] times] pmap.

(* Test while in parallel *)
[10 20 30 40] [[0 >] [1 -] while] pmap.

(* Test recursion combinators in parallel *)
[5 6 7 8] [[0 =] [1] [dup 1 -] [*] linrec] pmap.  (* factorial *)
```

### Memory Stress Tests

```joy
(* Force GC with large allocations *)
1000000 [1 2 3] times size.

(* Parallel with GC pressure *)
[1 2 3 4] [50000 [[1 2 3 4 5] reverse pop] times] pmap.
```

---

## Files Modified

| File | Change |
|------|--------|
| `include/parallel.h` | Rewrote `copy_node_to_parent` to use iteration for next-chain |
| `src/utils.c` | Split `copy` into `copy_one` + iterative `copy` |

---

## Conclusion

The root cause of parallel execution failures was not in the combinators themselves, but in the infrastructure that copies data between execution contexts and during garbage collection. By converting doubly-recursive functions to use iteration for linked list traversal, we eliminated stack overflow risks for arbitrarily long lists.

The key insight: **Recursion depth should be bounded by logical nesting depth (lists within lists), not by list length (number of elements).**
