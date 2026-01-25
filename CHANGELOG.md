# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### [1.44]

### Added

- **Persistent SQLite-backed sessions** - Store and restore symbol definitions across Joy sessions
  - Enable with `-DJOY_SESSION=ON` in CMake (requires SQLite3, default: OFF)
  - Automatic persistence: DEFINE operations write-through to database
  - Snapshot/restore: Save and restore session state to named checkpoints
  - Session management: List, merge, diff, and selectively copy between sessions
  - SQL queries: Execute raw SQL on session data for advanced introspection
  - Operators:
    - `session` - Open/create persistent session: `"myproj" session.`
    - `session-close` - Close current session
    - `sessions` - List available sessions (*.joy.db files)
    - `snapshot` - Save current state: `"v1" snapshot.`
    - `restore` - Restore to snapshot: `"v1" restore.`
    - `snapshots` - List available snapshots
    - `session-merge` - Merge another session into current
    - `session-diff` - Compare sessions (returns dict with added/modified/removed)
    - `session-take` - Copy specific symbol from another session
    - `sql` - Execute SQL query: `"SELECT * FROM symbols" [] sql.`
  - Uses SQLite WAL mode for better concurrency
  - Generator scripts detect `[SESSION]` marker for conditional compilation

### Fixed

- **Dictionaries unusable with `let` combinator** - The `DICT_` type was missing from the `exec_term` switch statement in `src/interp.c`, causing "valid factor needed for exec_term" errors when dictionaries were bound to names via `let`. Now dictionaries work correctly with local bindings:
  ```joy
  [["name" "Bob"] ["score" 100]] >dict [d] [d "name" dget] let.
  (* -> "Bob" *)
  ```

---

### [1.43]

### Added

- **`JOY_NATIVE_TYPES` compile option** - Make native vector/matrix types optional (default: ON)
  - Enable with `-DJOY_NATIVE_TYPES=ON` (default), disable with `-DJOY_NATIVE_TYPES=OFF`
  - When disabled: `v[...]` and `m[[...]]` syntax not recognized, native type builtins excluded
  - Guarded components: scanner literals, parser cases, GC handling, display, BLAS operations
  - Pattern follows existing `JOY_BLAS` and `JOY_PARALLEL` options
  - Docstring marker `[NATIVE]` for code generators to emit `#ifdef` guards

### Fixed

- **Scanner: `$ foo` parsed as single token** - The `$` operator followed by whitespace was incorrectly combined with the following identifier (e.g., `$ arith` became `"$ arith"` instead of two tokens `$` and `arith`). This broke the grammar library's recursive descent operator. Fixed by checking for whitespace after `$` and returning it as a standalone symbol.

- **grmlib.joy: Missing `END` statement** - The `LIBRA` block starting at line 8 was never closed. Line 220 had only a comment `(* end LIBRA *)` but no actual `END`. This caused all definitions inside LIBRA (`unops`, `binops`, `gen-put`, etc.) to never be committed to the symbol table, breaking the grammar library's expression generators.

---

### [1.42]

### Added

- **Native contiguous vector and matrix types** - Eliminate linked-list conversion overhead for BLAS operations
  - New types: `VECTOR_` (contiguous double array) and `MATRIX_` (row-major contiguous storage)
  - Literal syntax: `v[1 2 3]` for vectors, `m[[1 2][3 4]]` for matrices
  - Type predicates: `vector?`, `matrix?`
  - Conversion operators:
    - `>vec` - Convert numeric list to native vector: `[1 2 3] >vec -> v[1.0 2.0 3.0]`
    - `>mat` - Convert list of lists to native matrix: `[[1 2][3 4]] >mat -> m[[1.0 2.0][3.0 4.0]]`
    - `>list` - Convert native type back to list: `v[1 2 3] >list -> [1.0 2.0 3.0]`
  - Native BLAS operations (no conversion overhead):
    - `ndot` - Native dot product: `v[1 2 3] v[4 5 6] ndot -> 32.0`
    - `nmv` - Native matrix-vector multiply: `m[[1 0][0 1]] v[3 4] nmv -> v[3.0 4.0]`
    - `nmm` - Native matrix multiply: `m[[1 2][3 4]] m[[1 0][0 1]] nmm -> m[[1.0 2.0][3.0 4.0]]`
  - Native creation functions:
    - `nvzeros`, `nvones` - Create native vectors: `3 nvzeros -> v[0.0 0.0 0.0]`
    - `nmzeros`, `nmones` - Create native matrices: `2 2 nmzeros -> m[[0.0 0.0][0.0 0.0]]`
    - `nmeye` - Create native identity matrix: `3 nmeye -> m[[1.0 0.0 0.0][0.0 1.0 0.0][0.0 0.0 1.0]]`
  - Performance: Native dot product is ~3x faster than list-based (eliminates O(n) list traversal)
  - Tests: `tests/test2/vector_native.joy` (28 test cases)

### Changed

- Repurposed `LIST_PRIME_` (type index 13) as `VECTOR_` to fit within NOBDW 4-bit opcode limit
- Added `MATRIX_` at type index 15
- Updated `casting` builtin to reject `VECTOR_` and `MATRIX_` types (require special allocation)

---

### [1.41]

### Added

- **Dictionary type** - Key-value data structure using khash (`src/builtin/dict.c`)
  - `dempty` - Create empty dictionary
  - `dput` - Insert or update key-value pair: `dict "key" value dput -> dict'`
  - `dget` - Get value by key (error if missing): `dict "key" dget -> value`
  - `dgetd` - Get value with default: `dict "key" default dgetd -> value`
  - `dhas` - Check if key exists: `dict "key" dhas -> bool`
  - `ddel` - Delete key: `dict "key" ddel -> dict'`
  - `dkeys` - Get all keys: `dict dkeys -> [keys...]`
  - `dvals` - Get all values: `dict dvals -> [values...]`
  - `dsize` - Get entry count: `dict dsize -> int`
  - `>dict` - Create from assoc list: `[["k1" v1] ["k2" v2]] >dict -> dict`
  - `dict>` - Convert to assoc list: `dict dict> -> [["k1" v1] ...]`
  - `dmerge` - Merge two dicts: `dict1 dict2 dmerge -> dict'`
  - Tests: `tests/test2/dict.joy` (24 test cases)

- **JSON support** - Parse and emit JSON (`src/builtin/json.c`)
  - `json>` - Parse JSON string to Joy value: `"[1,2,3]" json> -> [1 2 3]`
  - `>json` - Emit Joy value as JSON string: `[1 2 3] >json -> "[1,2,3]"`
  - Type mapping: object<->dict, array<->list, string<->string, number<->int/float, true/false<->bool
  - Handles nested structures, escape sequences, Unicode
  - Tests: `tests/test2/json.joy` (26 test cases)

- **String interpolation** - Embed expressions in strings (`src/scan.c`)
  - Syntax: `$"Hello ${expr}!"` - expressions in `${...}` are evaluated and converted to strings
  - Example: `$"2+2=${2 2 +}"` -> `"2+2=4"`
  - Example: `$"pi=${3.14159}"` -> `"pi=3.14159"`
  - Supports integers, floats, strings, booleans, user-defined symbols, operators
  - Multiple interpolations: `$"a=${1}b=${2}"` -> `"a=1b=2"`
  - Literal `$` preserved when not followed by `{`: `$"$100"` -> `"$100"`
  - Tests: `tests/test2/strinterp.joy` (18 test cases)

- **String conversion operators** (`src/builtin/tostring.c`)
  - `toString` - Convert any value to its string representation: `42 toString -> "42"`
  - `unquoted` - Convert to string without quotes for strings: `42 unquoted -> "42"`, `"hi" unquoted -> "hi"`

---

### [1.40]

### Added

- **SIMD vectorization** - Enabled `#pragma omp simd` directives via `-fopenmp-simd` compiler flag
  - Applies to vector operations in `src/builtin/vector.c`
  - Works independently of full OpenMP parallel support (`-DJOY_PARALLEL`)
  - Enables auto-vectorization for element-wise operations on modern CPUs

- **Optional BLAS support** - Build-time option for hardware-optimized linear algebra
  - Enable with `-DJOY_BLAS=ON` in CMake
  - macOS: Uses Apple Accelerate framework (vecLib/BLAS)
  - Linux: Uses OpenBLAS, Intel MKL, or system BLAS via `FindBLAS`
  - `mm` (matrix multiply) uses `cblas_dgemm` for matrices >= 32x32 (4-8x speedup)
  - Graceful fallback to SIMD-optimized manual implementation below threshold
  - Note: `mv` and Level 1 ops use SIMD only (list-to-array conversion negates BLAS benefit)

- **Contiguous matrix type exploration** - Design document for future consideration
  - `doc/contiguous_matrix_exploration.md` - Analysis of native array types for Joy
  - Would enable full BLAS performance for all operations (10-50x projected speedup)

- **Local bindings (`let` combinator)** - Bind stack values to names within quotation
  - Syntax: `X1 X2 ... Xn [name1 name2 ... namen] [body] let -> result`
  - Binds n values from stack to names, executes body, restores original bindings
  - Example: `10 20 [aa bb] [aa bb + aa bb - *] let` -> `-300`
  - Simplifies complex stack manipulation into readable, math-like expressions
  - Limitation: Names must be user symbols, not builtins (`i` and `x` are reserved; other letters work)
  - Tests: `tests/test2/let.joy` (12 test cases)

- **Pattern matching (`match` and `cases` combinators)** - Match and destructure values
  - `match`: `value [pattern] [action] match -> result | false`
  - `cases`: `value [[pat1] [act1]] [[pat2] [act2]] ... cases -> result`
  - Patterns support: literals, wildcards (`_`), variables, empty list (`[]`), cons patterns (`[h : t]`), exact list patterns (`[a b c]`)
  - Variables are bound during action execution and restored afterward
  - Example: `5 [n] [n n *] match` -> `25`
  - Example: `[1 2 3] [[h : t]] [h t] match` -> `1 [2 3]`
  - Example: `DEFINE fact == [[[0] [1]] [[n] [n 1 - fact n *]]] cases.`
  - Tests: `tests/test2/match.joy` (30 test cases), `tests/test2/cases.joy` (28 test cases)

### Fixed

- **GC invalidates local Index variables during list construction** - Critical bug fix
  - **Symptom:** Repeated vector operations on 500+ element lists failed with
    "numeric list needed for v+" on second/third operation
  - **Root Cause:** `build_float_list()` and `build_int_list()` stored `Index`
    values in local C variables (`head`, `tail`). When `newnode()` triggered GC
    mid-loop, these local variables became stale (GC only updates `env->stck`,
    `env->prog`, `env->dump*`, not C stack variables)
  - **Fix:** Added `ensure_capacity(env, num)` in `src/utils.c` that pre-triggers
    GC before any local Index variables are created

---

### [1.39]

### Added

- **Vector operations** - Vectorized operators on numeric lists (`src/builtin/vector.c`)
  - Element-wise: `v+`, `v-`, `v*`, `v/` - arithmetic on pairs of numeric lists
  - Scalar: `vscale` - multiply all elements by a scalar
  - Linear algebra: `dot` - dot product, `cross` - cross product (3D)
  - Reductions: `vsum`, `vprod`, `vmin`, `vmax`, `vmean` - reduce list to single value
  - Norms: `vnorm` - Euclidean magnitude, `vnormalize` - unit vector
  - Creation: `vzeros`, `vones`, `vrange`, `vlinspace` - generate numeric lists
- **Matrix operations** - 2D array operations on lists of lists
  - Element-wise: `m+`, `m-`, `m*`, `m/` - arithmetic on pairs of matrices
  - Scalar: `mscale` - multiply all elements by a scalar
  - Linear algebra: `mm` (matmul), `mv` (matrix-vector), `transpose`
  - Properties: `det` (determinant), `inv` (inverse), `trace`
  - Creation: `meye` - identity matrix
- `doc/vector_impl.md` - Implementation documentation for vector/matrix operations
- `tests/test2/vector.joy` - Test suite for vector operations (42 assertions)
- `tests/test2/matrix.joy` - Test suite for matrix operations (26 assertions)
- **Extended parallel combinators** (`src/builtin/parallel.c`)
  - `pfilter` - Parallel filter: `[list] [predicate] pfilter -> [filtered]`
  - `preduce` - Parallel tree reduction: `[list] [binary-op] preduce -> result`

### Changed

- Architecture refactoring - consolidated builtin macro headers into `src/builtin/builtin_macros.h`
- Fixed `gen_table.py` to scan grouped builtin files instead of individual files
- Added `include/joy/joy.h` public API header for embedding

---

### [1.38]

### Added

- `doc/parallel.md` - User guide and design document for parallel execution (updated with 7 practical examples)
- `include/parallel.h` - Infrastructure for parallel execution (env cloning, task structures, result copying)
- `pmap` combinator - Parallel map: `[list] [quotation] pmap -> [results]`
- `pfork` combinator - Parallel fork/join: `X [P1] [P2] pfork -> R1 R2`
- `JOY_PARALLEL` CMake option - Enable parallel combinators with OpenMP support
- `env->mem_low`, `env->memoryindex`, `env->memorymax` - Memory tracking fields moved to Env struct for context isolation
- `env->old_memory` - GC working memory pointer moved to Env struct
- `env->stack_bottom` - Per-context stack bound for future parallel GC support
- `GC_Context` struct - Per-context state for conservative GC (gc.c), enabling independent garbage collectors
- `gc_ctx_create()`, `gc_ctx_destroy()` - Create/destroy independent GC contexts
- `gc_ctx_malloc()`, `gc_ctx_malloc_atomic()`, `gc_ctx_realloc()`, `gc_ctx_strdup()` - Context-aware allocation functions
- `gc_ctx_collect()` - Trigger garbage collection on a specific context
- `gc_ctx_set_stack_bottom()` - Set stack bounds for context-specific stack scanning
- `gc_ctx_get_gc_no()`, `gc_ctx_get_memory_use()`, `gc_ctx_get_free_bytes()` - Context-aware statistics
- `gc_get_default_context()` - Access the global default GC context
- `env->gc_ctx` - Per-Joy-context conservative GC context pointer in Env struct
- `GC_CTX_MALLOC()`, `GC_CTX_MALLOC_ATOMIC()`, `GC_CTX_REALLOC()`, `GC_CTX_STRDUP()` - Convenience macros for context-aware allocation (fall back to global GC if `gc_ctx` is NULL)
- `check_malloc()` - Safe malloc wrapper with null pointer check
- `check_strdup()` - Safe strdup wrapper with null pointer check
- `count()` - Pre-calculates nodes needed before garbage collection
- `fatal()` - Fatal error handler in core library (error.c)
- `TEST_MALLOC_RETURN` define - Portable malloc return value checking (replaces `_MSC_VER` guards)
- `ALLOW_SYSTEM_CALLS` define - Explicit control over system() call availability

### Changed

#### GC Context Isolation for Parallel Execution

- **Phase 1 - NOBDW Copying GC**: Per-context state instead of global variables, enabling independent memory management for multiple Joy contexts
  - `utils.c` - Refactored `inimem1()`, `inimem2()`, `copy()`, `gc1()`, `gc2()`, `newnode()`, `count()`, `my_memoryindex()`, `my_memorymax()` to use `env->*` fields
  - `gc1()` now accepts `Index *l, Index *r` parameters to preserve list and next pointers during garbage collection
  - `newnode()` uses `count()` to ensure sufficient memory before triggering GC
  - `newnode2()` uses `check_strdup()` instead of raw strdup with manual null check

- **Phase 2 - Conservative GC API**: Context-aware API for gc.c - all internal functions now operate on explicit `GC_Context*` parameter
  - `gc.c` - Refactored internal functions (`ctx_mark_ptr()`, `ctx_mark_stk()`, `ctx_scan()`, `ctx_remind()`, `ctx_mem_block()`, `ctx_forget()`)
  - Legacy API (`GC_malloc()`, `GC_gcollect()`, etc.) now wraps context-aware API using global default context

- **Phase 3 - Per-Joy-Context Integration**: Added `gc_ctx` field to Env struct, enabling per-Joy-context conservative GC
  - `joy.c` - Creates per-context GC in `joy_create()`, destroys in `joy_destroy()`
  - `scan.c`, `symbol.c`, `module.c`, `interp.c`, `undefs.c`, `intern.c` - Updated to use `GC_CTX_*` macros
  - Macros fall back to global GC when `gc_ctx` is NULL for backward compatibility with CLI

- **Phase 4 - Parallel Combinators**: Added `pmap` and `pfork` combinators with OpenMP support
  - `include/parallel.h` - Infrastructure for parallel execution (env cloning, result copying)
  - `src/builtin/pmap.c` - Parallel map combinator (`[list] [quotation] pmap -> [results]`)
  - `src/builtin/pfork.c` - Parallel fork/join combinator (`X [P1] [P2] pfork -> R1 R2`)
  - `CMakeLists.txt` - Added `JOY_PARALLEL` option with OpenMP support (macOS via libomp, Linux native)

- **Phase 5 - Parallel Execution**: Implemented actual parallel execution using OpenMP
  - `pmap` uses `#pragma omp parallel for` for lists with 4+ elements
  - `pfork` uses `#pragma omp parallel sections` for concurrent branch execution
  - Each parallel task gets a cloned environment with isolated GC context
  - Deep copying of inputs/quotations to child contexts and results back to parent
  - Graceful fallback to sequential execution for small lists or allocation failures

#### Other Changes

- `inimem1()`, `gc1()`, `gc2()`, `newnode()` use `TEST_MALLOC_RETURN` instead of `_MSC_VER` for portable memory checking
- `scan.c` uses `ALLOW_SYSTEM_CALLS` instead of `WINDOWS_S` for shell escape control
- Test runner now loads `lib/inilib.joy` first to set up library search paths

### Fixed

- GC parameter preservation - parameters passed to `newnode()` are now safely copied during garbage collection
- Test infrastructure - tests using `libload` (numlib, etc.) now work correctly (178/178 tests pass)
- **Stack overflow in `copy_node_to_parent`** - Converted recursive linked list traversal to iteration, fixing crashes with 130,000+ node results
- **Stack overflow in GC `copy` function** - Same fix applied to garbage collector, fixing crashes during GC on long lists
- **Stack overflow in `copy_body_from_parent`** - Converted recursive traversal to iteration in `src/interp.c`, completing the pattern across all copy functions
- **All combinators now work in parallel** - `times`, `while`, `step`, `linrec`, `binrec`, `genrec`, `primrec`, `tailrec`, `condlinrec`, `condnestrec`, `treestep`, `treerec`, `treegenrec` all verified working in `pmap`/`pfork`

### Added

- `tests/parallel_stress.joy` - 16 stress tests for parallel execution (up to 100,000 iterations)
- `tests/parallel_benchmark.joy` - Joy benchmark comparing `pmap` vs `map` (CPU time)
- `tests/parallel_benchmark.sh` - Shell benchmark measuring wall clock time
- `doc/parallel_fixes.md` - Technical documentation of parallel execution fixes
- `doc/parallel_performance.md` - Performance guide documenting when `pmap` is faster than `map`

### Upstream Sync

Core changes from [Wodan58/Joy](https://github.com/Wodan58/Joy) Patch Tuesday V (2026-01-13):
- Portable pointer handling with `pointer_t` type abstraction
- Improved memory safety with checked allocation wrappers
- GC improvements for parameter preservation during collection
- Configurable system call control

## [1.37]

### Added

- Tab completion in REPL
- Linenoise library for line editing

### Fixed

- REPL double-free bug

### Changed

- Restructured test output to build directory
- Moved source files to `src/` directory structure
- Added embedding examples in `examples/`
