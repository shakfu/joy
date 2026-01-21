# Joy Language TODO

## Completed

### Parallel Execution Fixes (January 2026)

- [x] **Stack overflow in `copy_node_to_parent`** - Converted recursive list traversal to iteration
- [x] **Stack overflow in GC `copy` function** - Same fix applied to garbage collector
- [x] **Parallel stress tests** - Added `tests/parallel_stress.joy` (13 tests, 100k+ iterations)
- [x] **All combinators verified working in parallel**:
  - Iterative: `times`, `while`, `step`
  - Recursive: `linrec`, `binrec`, `genrec`, `primrec`, `tailrec`
  - Conditional: `condlinrec`, `condnestrec`

See `FIXES_FOR_PARALLEL.md` for technical details.

---

## TODO (Prioritized)

### Priority 1: Low Effort

- [ ] **Test tree combinators in parallel**
  - `treestep` - Tree traversal
  - `treerec` - Tree recursion
  - `treegenrec` - General tree recursion
  - May work already, needs verification

### Priority 2: Medium Effort

- [ ] **Convert `copy_body_from_parent` to iterative** (`src/interp.c`)
  - Currently recursive for `next` chain
  - Low risk (symbol bodies are typically small)
  - Would complete the pattern established in parallel.h and utils.c

### Priority 3: Nice to Have

- [ ] **Performance benchmarking**
  - Compare parallel vs sequential execution times
  - Identify optimal list sizes for parallel execution
  - Document when `pmap` is faster than `map`

- [ ] **Parallel execution documentation**
  - User guide for `pmap` and `pfork`
  - Best practices and limitations
  - Example use cases

---

## Test Examples

All of these now work correctly:

```joy
(* Iterative combinators *)
[1 2 3 4] [10 [dup +] times] pmap.                        (* -> [1024 2048 3072 4096] *)
[10 20 30 40] [[0 >] [1 -] while] pmap.                   (* -> [0 0 0 0] *)

(* Recursive combinators *)
[5 6 7 8] [[0 =] [pop 1] [dup 1 -] [*] linrec] pmap.     (* -> [120 720 5040 40320] *)
[100 500 1000] [[0 =] [] [dup 1 -] [+] linrec] pmap.     (* -> [5050 125250 500500] *)

(* Stress test *)
[1 2 3 4 5 6 7 8] [1000000 [dup * 2 mod] times] pmap.    (* 1M iterations - works! *)
```

---

## Related Files

| File | Description |
|------|-------------|
| `FIXES_FOR_PARALLEL.md` | Detailed documentation of parallel execution fixes |
| `include/parallel.h` | Parallel infrastructure (`copy_node_to_parent`) |
| `src/utils.c` | GC and memory management (`copy` function) |
| `src/interp.c` | Interpreter (`copy_body_from_parent`) |
| `src/builtin/pmap.c` | Parallel map implementation |
| `src/builtin/pfork.c` | Parallel fork implementation |
