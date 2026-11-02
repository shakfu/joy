# Memory rules (NOBDW)

The rules C code must follow to keep values alive across allocations.
None of them are enforced; violations show up only when a collection
happens at the wrong moment. Test new code under `JOY_GC_STRESS` (see
Testing).

## Three allocators

| Memory | Holds | Collector | Roots |
|-|-|-|-|
| Node arena (`env->memory`) | Joy values, string bytes inline | Copying, `gc1`/`gc2` in `src/utils.c` | `stck`, `prog`, `conts`, `dump`, `dump1`-`dump5`, the arguments of the `newnode` call that collects, `is_root` symbol entries, the definition space |
| `GC_malloc` (`src/gc.c`) | `GC_strdup` strings, vector, matrix and lazy payloads, kvec buffers | Conservative mark-sweep | C stack, globals, both node arenas |
| `malloc` | Dictionary tables and keys, sessions, LSP state | Manual; tables swept by `sweep_dicts` | Tables: live `DICT_` nodes |

## 1. Node indices in C variables

- An `Index` in a C variable is stale after any call that can allocate
  nodes. Such calls are any `*_NEWNODE`, `newnode`, `newnode2`,
  `ensure_capacity` and `gc_collect`; executing Joy code (`exeterm`);
  and parsing (`readterm`, `readfactor`).
- Keep the value reachable from a root and re-read it after the call.
  Push it on `env->stck`, or use a free `dumpN` slot (`session_diff_`).
- `newnode` roots its own `next` argument, and `u.lis` for `LIST_`.
  `LIST_NEWNODE(a, b)` is safe for fresh `a` and `b`. Indices inside
  other payloads are not rooted by `newnode`: build a `LazyData` inside
  a reservation (`lazy.c`).
- `ensure_capacity(env, n)` guarantees the next `n` nodes without a
  collection. `n` counts nodes, not values: a string takes
  1 + ceil((len + 1 - sizeof(Types)) / sizeof(Node)) nodes (`newnode`).
  Nested reservations are allowed.

## 2. Strings

- `GETSTRING(n)` points into the node arena. `gc2` frees the old arena,
  so the pointer dangles after any allocation. Copy it with `GC_strdup`
  before allocating (`restore_`, `session_take_`).
- `newnode` copies `u.str`; the caller keeps ownership. `newnode2`
  shows the `strdup`/`free` pattern.

## 3. Definition space

- Nodes below `mem_low` are never moved and never traced: `copy`
  returns them unchanged and does not follow their fields.
- They are created only between `inimem1` and `inimem2`. In that window
  `flibrary_busy` disables collection, and `inimem2` makes everything
  allocated so far permanent.
- A definition-space node must not point at a heap node. Dictionary
  values are the exception: `sweep_dicts` forwards them.

## 4. Runtime bindings

- A symbol body set at runtime lives in the heap. Set
  `ent.is_root = ent.is_user = 1` and `env->variable_busy = 1`;
  `scan_roots` then copies the body. Examples: `assign_`, the `let`/
  `match` bindings (`symbol.c`), `load_symbols_from_db`.
- A runtime binding can reuse a builtin's entry (a session defining
  `rotate`), so `scan_roots` scans the whole table.

## 5. Dictionaries

- `dict_create` registers each table. At each collection `sweep_dicts`
  frees every table not reached from a live `DICT_` node, the definition
  space, or `env->bucket` (the table of the `DICT_NEWNODE` that
  triggered the collection).
- Put a new table on a rooted node before the next allocation:
  `env->stck = DICT_NEWNODE(dict_create(env), env->stck)`. The table
  pointer stays valid while that node is reachable.
- Values are indices, forwarded when the `DICT_` node is copied. The
  table owns its keys: `dict_set` copies a new key, so free your own.

## 6. `GC_malloc` payloads

- A payload survives if a pointer to it is on the C stack, in a global,
  or in either node arena. A pointer held only in `malloc`'d memory
  (a khash table, `Session`) does not keep it alive.
- Vector, matrix and lazy payloads are deep-copied at each node
  collection. A C pointer taken before an allocation refers to the old
  copy afterwards; re-read it from the node before writing.

## 7. Parallel tasks

- A child context shares the symbol table read-only, reads bodies from
  `parent_memory` through `parallel_copy_chain`, and skips `scan_roots`.
- A builtin that changes the symbol table or other shared state starts
  with `PARALLEL_UNSAFE(env)`. In a child this aborts the task, and the
  parent replays the combinator sequentially.

## 8. Errors

- `execerror` and `abortexecution_` `longjmp` out. Free `malloc`'d
  temporaries before calling them (`fgets_`). `GC_malloc` temporaries
  need no cleanup.

## Testing

- `-DJOY_GC_STRESS=ON` collects at every allocation outside a
  reservation, which exposes rule 1, 2 and 4 violations. Add
  `-DJOY_SESSION=ON` to cover `session.c`. Under stress, `test2_dict_gc`
  takes 900 s and `parallel_semantics` 250 s; the rest take seconds.
- ASan reports false positives in the conservative stack scan. Pass
  `-fsanitize-ignorelist=<file>` with:

  ```
  fun:ctx_mark_stk
  fun:ctx_mark_ptr
  fun:ctx_mark_bss
  fun:ctx_scan
  fun:gc_ctx_mark_range
  ```
