# Joy Language TODO

## Completed

### Parallel Execution Fixes (January 2026)

- [x] **Stack overflow in `copy_node_to_parent`** - Converted recursive list traversal to iteration
- [x] **Stack overflow in GC `copy` function** - Same fix applied to garbage collector
- [x] **Parallel stress tests** - Added `tests/parallel_stress.joy` (16 tests, 100k+ iterations)
- [x] **All combinators verified working in parallel**:
  - Iterative: `times`, `while`, `step`
  - Recursive: `linrec`, `binrec`, `genrec`, `primrec`, `tailrec`
  - Conditional: `condlinrec`, `condnestrec`
  - Tree: `treestep`, `treerec`, `treegenrec`
- [x] **Convert `copy_body_from_parent` to iterative** (`src/interp.c`) - Completes the pattern
- [x] **Performance benchmarking** - Created `tests/parallel_benchmark.sh` and `doc/parallel_performance.md`
  - Crossover point: ~10,000+ operations per element
  - Heavy work (100k iterations): pmap 20-25% faster
  - Very heavy work (1M iterations): pmap 35-40% faster
- [x] **Parallel execution user documentation** - Updated `doc/parallel.md` with:
  - 7 practical example use cases (factorials, tree processing, MapReduce, etc.)
  - Updated combinator compatibility table (all working)
  - Performance guidelines and when NOT to use pmap

See `doc/parallel_fixes.md` for technical details.
See `doc/parallel_performance.md` for benchmark results and usage guidelines.
See `doc/parallel.md` for user guide and examples.

---

## TODO (Prioritized)

All parallel execution tasks complete.

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
| `doc/parallel.md` | User guide and examples |
| `doc/parallel_fixes.md` | Technical documentation of parallel execution fixes |
| `doc/parallel_performance.md` | Performance benchmarks and usage guidelines |
| `include/parallel.h` | Parallel infrastructure (`copy_node_to_parent`) |
| `src/utils.c` | GC and memory management (`copy` function) |
| `src/interp.c` | Interpreter (`copy_body_from_parent`) |
| `src/builtin/pmap.c` | Parallel map implementation |
| `src/builtin/pfork.c` | Parallel fork implementation |
| `tests/parallel_benchmark.sh` | Wall-time benchmark script |
| `tests/parallel_benchmark.joy` | CPU-time benchmark (Joy) |
