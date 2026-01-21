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

Benchmark with 8 elements and heavy computation:

| Mode | CPU Usage | Speedup |
|------|-----------|---------|
| `pmap` (parallel) | 448% | **2.8x faster** |
| `map` (sequential) | 97% | baseline |

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

## Known Issues and Future Work

### Known Issues

Some combinators do not work correctly inside parallel quotations:

| Combinator | Issue |
|------------|-------|
| `times` | Crashes or hangs in parallel context |
| Other recursive combinators | May have similar issues |

Root cause: Recursive combinators may bypass the `copy_body_from_parent` path and access parent memory indices directly.

See `TODO.md` for the full list of combinators to investigate.

### Future Development

#### Short Term

- [ ] Fix `times` and other recursive combinators for parallel use
- [ ] Add `pfilter` - parallel filter combinator
- [ ] Improve error messages for parallel failures

#### Medium Term

- [ ] Work-stealing for nested parallelism
- [ ] Cancellation support for early termination
- [ ] `preduce` - parallel tree reduction for associative operations
- [ ] Parallel I/O with proper synchronization

#### Long Term

- [ ] `par`/`await` - async futures for fine-grained parallelism
- [ ] Distributed execution across machines
- [ ] GPU offload for numeric operations

### Combinators to Investigate

The following combinators need testing and possible fixes for parallel compatibility:

**Iterative:**
- `times`, `while`, `step`

**Recursive:**
- `linrec`, `tailrec`, `binrec`, `genrec`, `primrec`
- `condlinrec`, `condnestrec`

**Tree:**
- `treestep`, `treerec`, `treegenrec`

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
./joy tests/parallel_test.joy      # Correctness tests
./joy tests/parallel_benchmark.joy  # Performance benchmark (may crash - known issue)
./joy tests/mapreduce_test.joy     # MapReduce library tests
./joy tests/wordcount_test.joy     # Word count example
```

### Test Coverage

The parallel implementation is verified by:
- 10 parallel correctness tests in `tests/parallel_test.joy`
- MapReduce library tests in `tests/mapreduce_test.joy`
- Word count integration test in `tests/wordcount_test.joy`
- All 178 existing tests continue to pass

---

## Example Programs

### Parallel Squares

```joy
[1 2 3 4 5 6 7 8] [dup *] pmap.
(* [1 4 9 16 25 36 49 64] *)
```

### Parallel String Processing

```joy
["hello" "world" "test" "data"] [size] pmap.
(* [5 5 4 4] *)
```

### Concurrent Computation

```joy
10 [2 *] [3 +] pfork + .
(* 33 - sum of 20 and 13 *)
```

### Word Count (Full MapReduce)

```joy
"lib/mapreduce.joy" include.

["apple" "banana" "apple" "cherry" "banana" "apple"]
    [1 pair [] cons] pmap
    flatten
    group-by-key
    [count-reducer] pmap.

(* [["cherry" 1] ["banana" 2] ["apple" 3]] *)
```

---

## References

1. **OpenMP Specification**: https://www.openmp.org/specifications/
2. **Joy Language**: Manfred von Thun's papers on concatenative programming
3. **Conservative GC**: Hans Boehm's garbage collection papers
4. **libomp**: https://openmp.llvm.org/
