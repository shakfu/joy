# Parallel Execution in Joy

This document describes the parallel execution implementation in the Joy interpreter, using OpenMP for multi-core utilization.

## Implementation Status

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 1 | NOBDW GC Context Isolation | Complete |
| Phase 2 | Conservative GC Context Isolation | Complete |
| Phase 3 | Per-Joy-Context GC Integration | Complete |
| Phase 4 | Parallel Combinator Infrastructure | Complete |
| Phase 5 | OpenMP Parallel Execution | Complete |
| Phase 6 | User-Defined Symbol Support | Complete |

All 178 tests pass with parallel execution enabled.

---

## Quick Start

### Building with Parallel Support

```bash
# macOS (requires libomp from Homebrew)
brew install libomp
cmake -B build -DJOY_PARALLEL=ON
cmake --build build

# Linux (OpenMP usually built into GCC)
cmake -B build -DJOY_PARALLEL=ON
cmake --build build
```

### Basic Usage

```joy
(* Parallel map - square each element *)
[1 2 3 4 5 6 7 8] [dup *] pmap.
(* Result: [1 4 9 16 25 36 49 64] *)

(* Parallel fork - compute two results concurrently *)
10 [2 *] [3 +] pfork.
(* Results: 20 13 *)
```

### Performance

`pmap` has thread overhead, so it needs substantial work to outperform `map`:

| Work per Element | Recommendation | Speedup |
|------------------|----------------|---------|
| Light (simple ops) | Use `map` | pmap has overhead |
| Heavy (~100k iterations) | Use `pmap` | 20-25% faster |
| Very Heavy (~1M iterations) | Use `pmap` | 35-40% faster |

**Rule of thumb:** Use `pmap` when each element takes >10ms to process.

See [parallel_performance.md](parallel_performance.md) for detailed benchmarks.

---

## Parallel Combinators

### `pmap` - Parallel Map

Apply a quotation to each element of a list in parallel.

```
Stack effect: A [P] -> B
```

```joy
[1 2 3 4 5 6 7 8] [dup *] pmap.
(* Result: [1 4 9 16 25 36 49 64] *)

["hello" "world" "test" "data"] [size] pmap.
(* Result: [5 5 4 4] *)
```

Behavior:
- Lists with fewer than 4 elements use sequential execution (overhead not worth it)
- Order of results matches order of inputs
- Each element is processed independently with isolated GC context
- Errors are propagated after all tasks complete

### `pfork` - Parallel Fork

Execute two quotations concurrently with the same input.

```
Stack effect: X [P1] [P2] -> R1 R2
```

```joy
10 [2 *] [3 +] pfork.
(* Results: 20 13 *)

5 [dup *] [dup dup * *] pfork.
(* Results: 25 125 - square and cube computed concurrently *)
```

---

## MapReduce Library

A MapReduce framework is provided in `lib/mapreduce.joy` demonstrating parallel data processing:

```joy
"lib/mapreduce.joy" include.

(* Word count example *)
["hello" "world" "hello" "joy" "world" "hello"]
    [1 pair [] cons] pmap    (* Map: word -> [[word 1]] *)
    flatten                   (* Flatten results *)
    group-by-key             (* Group by word *)
    [count-reducer] pmap.    (* Reduce: count occurrences *)

(* Result: [["joy" 1] ["world" 2] ["hello" 3]] *)
```

### Library Functions

| Function | Stack Effect | Description |
|----------|--------------|-------------|
| `flatten` | `[[A]...] -> [A...]` | Flatten nested lists |
| `pair` | `X Y -> [X Y]` | Create a key-value pair |
| `fst` | `[X Y] -> X` | Get first element (key) |
| `snd` | `[X Y] -> Y` | Get second element (value) |
| `in-list` | `X [A] -> B` | Check if X is in list |
| `unique` | `[A] -> [A']` | Remove duplicates |
| `get-keys` | `[[k v]...] -> [k...]` | Extract unique keys |
| `vals-for-key` | `[[k v]...] k -> [v...]` | Get values for a key |
| `group-by-key` | `[[k v]...] -> [[k [v...]]...]` | Group pairs by key |
| `sum-reducer` | `[k [v...]] -> [k sum]` | Sum values for a key |
| `count-reducer` | `[k [v...]] -> [k count]` | Count values for a key |

---

## Architecture

### Execution Model

Each parallel task executes with complete isolation:

```
Parent Environment
    |
    +-- Task 1: Cloned Env + Own GC Context + Own Memory
    |
    +-- Task 2: Cloned Env + Own GC Context + Own Memory
    |
    +-- Task N: Cloned Env + Own GC Context + Own Memory
    |
    v
Results copied back to Parent
```

### Environment Cloning

When a parallel task starts, it receives:

| Component | Handling |
|-----------|----------|
| Configuration flags | Copied |
| Symbol table | Shared (read-only) |
| Hash tables | Shared (read-only) |
| Memory array | New isolated array |
| GC context | New isolated context |
| Stack/dump | Fresh (empty) |
| Error handling | Isolated per-task |
| I/O | Disabled in tasks |

### User-Defined Symbol Execution

A key challenge is that user-defined symbols have their bodies stored in the parent's memory. When a child executes a symbol like `pair`, it must access the parent's memory.

Solution (Phase 6):
- Added `parent_memory` field to Env structure
- When executing USR_ nodes in parallel context, symbol bodies are copied from parent memory to child memory via `copy_body_from_parent()` in `interp.c`

### Result Copying

Results must be deep-copied from child to parent context before the child's GC is destroyed:

```c
static Index copy_node_to_parent(pEnv parent, pEnv child, Index node) {
    // Recursively copy node tree from child->memory to parent->memory
    // Handles all node types: INTEGER_, STRING_, LIST_, USR_, etc.
}
```

---

## Implementation Details

### Key Files

| File | Purpose |
|------|---------|
| `include/parallel.h` | Parallel infrastructure (env cloning, node copying) |
| `include/globals.h` | Env structure with `parent_memory` field |
| `src/builtin/pmap.c` | Parallel map combinator |
| `src/builtin/pfork.c` | Parallel fork combinator |
| `src/interp.c` | `copy_body_from_parent()` for symbol execution |
| `src/gc.c` | Context-aware conservative GC |
| `lib/mapreduce.joy` | MapReduce library |

### OpenMP Pragmas

**pmap** uses dynamic scheduling for load balancing:
```c
#pragma omp parallel for schedule(dynamic)
for (int i = 0; i < count; i++) {
    // Execute quotation on element i
}
```

**pfork** uses parallel sections:
```c
#pragma omp parallel sections
{
    #pragma omp section
    { /* Execute P1 */ }
    #pragma omp section
    { /* Execute P2 */ }
}
```

### Error Handling

- Each task has its own `setjmp`/`longjmp` error context
- Errors are captured in `ParallelTask.error_msg`
- After all tasks complete, first error is propagated to parent
- No cancellation mechanism (all tasks run to completion)

---

## Limitations

### Semantic Restrictions

| Restriction | Reason |
|-------------|--------|
| No symbol definition in parallel tasks | Would require locking the symbol table |
| No I/O in parallel tasks | stdout/stdin are not thread-safe |
| No file operations | FILE* handles are not safely shareable |
| No `include` in parallel | Scanner state is per-environment |

### Performance Considerations

| Factor | Impact |
|--------|--------|
| Task creation overhead | ~1-5 microseconds per task |
| Env cloning cost | Proportional to setup, not memory size |
| Result copying | Deep copy required across GC contexts |
| Minimum useful parallelism | 4+ elements for `pmap` to be worthwhile |

### Memory Overhead

Each parallel task requires:
- One `Env` structure (~2KB)
- One `GC_Context` structure (~64 bytes + hash table)
- Own memory array (initial allocation)
- Duplicated string data for results

---

## Verified Combinators

All major combinators work correctly in parallel quotations:

| Category | Combinators | Status |
|----------|-------------|--------|
| Iterative | `times`, `while`, `step` | Working |
| Recursive | `linrec`, `tailrec`, `binrec`, `genrec`, `primrec` | Working |
| Conditional | `condlinrec`, `condnestrec` | Working |
| Tree | `treestep`, `treerec`, `treegenrec` | Working |

Stress-tested with up to 1,000,000 iterations per element. See `tests/parallel_stress.joy`.

## Future Development

### Short Term

- [ ] Add `pfilter` - parallel filter combinator
- [ ] Improve error messages for parallel failures

### Medium Term

- [ ] Work-stealing for nested parallelism
- [ ] Cancellation support for early termination
- [ ] `preduce` - parallel tree reduction for associative operations

### Long Term

- [ ] `par`/`await` - async futures for fine-grained parallelism
- [ ] Distributed execution across machines

---

## Platform Support

### macOS

Requires libomp from Homebrew:

```bash
brew install libomp
cmake -B build -DJOY_PARALLEL=ON
cmake --build build
```

### Linux (Ubuntu/Debian)

OpenMP is typically built into GCC:

```bash
cmake -B build -DJOY_PARALLEL=ON
cmake --build build
```

If using Clang:
```bash
sudo apt-get install libomp-dev
```

### Linux (Fedora/RHEL)

```bash
cmake -B build -DJOY_PARALLEL=ON
cmake --build build
```

### Windows (MSVC)

OpenMP is supported by MSVC:

```bash
cmake -B build -DJOY_PARALLEL=ON
cmake --build build
```

---

## Testing

### Running Parallel Tests

```bash
./joy tests/parallel_test.joy       # Correctness tests
./joy tests/parallel_stress.joy     # Stress tests (100k+ iterations)
./joy tests/parallel_benchmark.joy  # Performance benchmark (CPU time)
bash tests/parallel_benchmark.sh    # Performance benchmark (wall time)
```

### Test Coverage

The parallel implementation is verified by:
- Parallel correctness tests in `tests/parallel_test.joy`
- 16 stress tests in `tests/parallel_stress.joy` (up to 100,000 iterations)
- All 180 tests pass with parallel execution enabled

---

## Example Use Cases

### 1. Parallel Numeric Computation

Compute factorials for multiple numbers:

```joy
DEFINE factorial == [0 =] [pop 1] [dup 1 -] [*] linrec.

[5 6 7 8 9 10] [factorial] pmap.
(* [120 720 5040 40320 362880 3628800] *)
```

Sum sequences 1..N for multiple N values:

```joy
DEFINE sum-to-n == [0 =] [] [dup 1 -] [+] linrec.

[100 500 1000 5000] [sum-to-n] pmap.
(* [5050 125250 500500 12502500] *)
```

### 2. Heavy Iterative Processing

When each element requires many iterations, `pmap` shines:

```joy
(* Double a number 20 times (multiply by 2^20 = 1048576) *)
[1 2 3 4] [20 [dup +] times] pmap.
(* [1048576 2097152 3145728 4194304] *)

(* Process with 100000 iterations per element *)
[1 2 3 4 5 6 7 8] [100000 [dup * 2 mod] times] pmap.
(* Each element processed in parallel - 20-25% faster than map *)
```

### 3. Concurrent Branch Computation

Use `pfork` when you need two different computations on the same input:

```joy
(* Compute both square and cube *)
5 [dup *] [dup dup * *] pfork.
(* Results: 25 125 *)

(* Compute sum and product of a list *)
[1 2 3 4 5] [0 [+] fold] [[*] infra first] pfork.
(* Results: 15 120 *)
```

### 4. Tree Processing

Process tree structures in parallel:

```joy
(* Square all leaves in nested lists *)
[[[1 2] 3] [[4 5] 6]] [[dup *] [map] treerec] pmap.
(* [[[1 4] 9] [[16 25] 36]] *)

(* Collect leaves from multiple trees *)
[[[1 2] 3] [[4 5] 6] [[7 8] 9]] [[] swap [swons] treestep] pmap.
(* [[3 2 1] [6 5 4] [9 8 7]] *)
```

### 5. Recursive Algorithms

Complex recursive algorithms benefit from parallelism:

```joy
(* Ackermann function (expensive recursion) *)
DEFINE ack == [[[dup null] [pop succ]]
               [[over null] [popd pred 1 swap] []]
               [[dup rollup [pred] dip] [swap pred ack]]] condlinrec.

[[2 1] [2 2] [2 3] [2 4]] [i swap ack] pmap.
(* [5 7 9 11] - computed in parallel *)
```

### 6. Word Count (MapReduce Pattern)

```joy
"lib/mapreduce.joy" include.

["apple" "banana" "apple" "cherry" "banana" "apple"]
    [1 pair [] cons] pmap    (* Map: word -> [[word 1]] *)
    flatten
    group-by-key
    [count-reducer] pmap.    (* Reduce: count per key *)

(* [["cherry" 1] ["banana" 2] ["apple" 3]] *)
```

### 7. When NOT to Use pmap

For simple operations, `map` is faster:

```joy
(* DON'T use pmap for trivial operations *)
[1 2 3 4] [dup *] map.     (* Use map - faster *)
[1 2 3 4] [1 +] map.       (* Use map - faster *)

(* DO use pmap for heavy computation *)
[1 2 3 4] [100000 [dup * 2 mod] times] pmap.  (* Use pmap *)
```

---

## References

1. **OpenMP Specification**: https://www.openmp.org/specifications/
2. **Joy Language**: Manfred von Thun's papers on concatenative programming
3. **Conservative GC**: Hans Boehm's garbage collection papers
4. **libomp**: https://openmp.llvm.org/
