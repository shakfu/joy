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

- [x] **Extended parallel combinators** - Additional parallel operators
  - `pfilter` - Parallel filter: `[list] [predicate] pfilter -> [filtered]`
  - `preduce` - Parallel tree reduction: `[list] [binary-op] preduce -> result`
  - Tests added to `tests/parallel_test.joy`

### Architecture Refactoring (January 2026)

- [x] **Group related builtins** - Reduced 229 files to 18 grouped files
  - Grouped files in `src/builtin/`: `arithmetic.c`, `aggregate.c`, `boolean.c`,
    `combinators.c`, `comparison.c`, `config.c`, `control.c`, `internal.c`,
    `io.c`, `math.c`, `n_ary.c`, `parallel.c`, `recursion.c`, `sets.c`,
    `stacks.c`, `strings.c`, `systems.c`, `type.c`
  - Build system updated (`gen_builtins.py`, `gen_table.py`, `CMakeLists.txt`)

- [x] **Extract Env sub-structs** - Refactored monolithic Env struct
  - `EnvConfig` - configuration flags (autoput, echoflag, debugging, etc.)
  - `EnvStats` - runtime statistics (nodes, avail, collect, calls, opers)
  - `EnvScanner` - scanner/lexer state (srcfile, linenum, linebuf, infile[])
  - `EnvError` - error handling state (message, line, column)

- [x] **Add inline node accessors** - Type-safe alternatives to macros
  - `node_type()`, `node_value()`, `node_next()`, `node_next2()` in `globals.h`
  - Macros retained for performance-critical paths
  - Better debugging, IDE support, type safety

- [x] **Document macro contracts** - Added documentation
  - `@requires`, `@param`, `@modifies` annotations in `macros.h`, `runtime.h`
  - Examples for all major macro categories
  - Stack effects documented for NULLARY/UNARY/BINARY

- [x] **Standardize naming conventions** - Incremental
  - Core functions use `module_action` pattern
  - Removed `my_` prefix from public functions (remaining only in static helpers)

See `doc/architecture.md` for design rationale.

### Vector and Matrix Operations (January 2026)

- [x] **Vector operations** - Vectorized operators in `src/builtin/vector.c`
  - Element-wise: `v+`, `v-`, `v*`, `v/`
  - Scalar: `vscale`
  - Linear algebra: `dot`, `cross`
  - Reductions: `vsum`, `vprod`, `vmin`, `vmax`, `vmean`
  - Norms: `vnorm`, `vnormalize`
  - Creation: `vzeros`, `vones`, `vrange`, `vlinspace`
  - Tests: `tests/test2/vector.joy` (42 assertions)

- [x] **Matrix operations** - 2D array operations in `src/builtin/vector.c`
  - Element-wise: `m+`, `m-`, `m*`, `m/`
  - Scalar: `mscale`
  - Linear algebra: `mm` (matmul), `mv` (matrix-vector), `transpose`
  - Properties: `det`, `inv`, `trace`
  - Creation: `meye` (identity matrix)
  - Tests: `tests/test2/matrix.joy` (26 assertions)

See `doc/vector.md` for design rationale.
See `doc/vector_impl.md` for implementation details.

### Bug Fixes (January 2026)

- [x] **GC invalidates local Index variables during list construction**
  - **Symptom:** Repeated vector operations on 500+ element lists failed with
    "numeric list needed for v+" on second/third operation
  - **Root Cause:** `build_float_list()` and `build_int_list()` stored `Index`
    values in local C variables (`head`, `tail`). When `newnode()` triggered GC,
    these local variables were not updated (GC only updates `env->stck`,
    `env->prog`, `env->dump*`), causing memory corruption
  - **Fix:** Added `ensure_capacity(env, num)` function in `src/utils.c` that
    pre-triggers GC before any local Index variables are created. Called at
    start of `build_float_list()` and `build_int_list()` in `src/builtin/vector.c`
  - **Files changed:** `src/utils.c`, `include/globals.h`, `src/builtin/vector.c`

---

## TODO (Prioritized)

### Priority 1: High Value / Low-Medium Effort

#### Numeric Computing

- [x] **SIMD optimization** - Performance improvements for vector/matrix ops
  - ~~Contiguous storage for homogeneous numeric lists~~
  - [x] SIMD vectorization with `#pragma omp simd` (via `-fopenmp-simd` flag)
  - [x] Optional BLAS integration (Phase 1 & 2 complete)

- [x] **Optional BLAS support** - Build-time option for hardware-optimized linear algebra
  - Build flag: `-DJOY_BLAS=ON`
  - CMake detection: Apple Accelerate (macOS), OpenBLAS, Intel MKL, reference BLAS
  - Graceful fallback to SIMD implementation when BLAS unavailable or below threshold

  **Completed:**
  - [x] CMake `FindBLAS` integration with vendor detection
  - [x] `#ifdef JOY_BLAS` guards in `src/builtin/vector.c`
  - [x] `BLAS_THRESHOLD 32` - minimum dimension to use BLAS
  - [x] `mm` via `cblas_dgemm` - matrix multiply (4-8x speedup)

  **Not implemented (benchmarked, no benefit):**
  - `mv` - List-to-array conversion overhead (~50% of runtime) negates BLAS benefit
  - Level 1 ops (`dot`, `vnorm`, `vscale`) - Same issue, O(n) conversion vs O(n) compute

  **Future consideration:**
  - [ ] Native contiguous array type - Would enable full BLAS performance for all ops
  - See `doc/contiguous_matrix_exploration.md` for design exploration

### Priority 2: Medium Value / Medium Effort

#### Language Features

- [x] **Local bindings (`let`)** - Bind stack values to names within quotation

  **Syntax:** `X1 X2 ... Xn [name1 name2 ... namen] [body] let -> result`

  Binds `n` values from the stack to names, executes body, then restores original bindings.
  Names are bound in order: `name1` gets `X1` (deepest), `namen` gets `Xn` (TOS).

  **Example: `(a + b) * (a - b)`**
  ```joy
  (* Without local bindings - stack juggling *)
  (* Stack: a b *)
  dup2           (* a b a b *)
  +              (* a b sum *)
  rolldown       (* sum a b *)
  -              (* sum diff *)
  *              (* result *)

  (* With local bindings - clear and direct *)
  10 20 [aa bb] [aa bb + aa bb - *] let   (* -> -300 *)
  ```

  **Example: Point distance between (x1,y1) and (x2,y2)**
  ```joy
  (* Without - error-prone stack manipulation *)
  rolldown - dup * rolldown swap - dup * + sqrt

  (* With local bindings *)
  0 0 3 4 [x1 y1 x2 y2] [
    x2 x1 - dup *
    y2 y1 - dup * + sqrt
  ] let   (* -> 5.0 *)
  ```

  **Example: Quadratic formula root**
  ```joy
  (* For a=1, b=-3, c=2: x = (3 + sqrt(9-8))/2 = 2 *)
  1 -3 2 [a_coef b_coef c_coef] [
    b_coef neg
    b_coef b_coef * 4 a_coef * c_coef * - sqrt +
    2 a_coef * /
  ] let   (* -> 2.0 *)
  ```

  **Example: Using let in definitions**
  ```joy
  DEFINE pyth == [a_val b_val] [a_val a_val * b_val b_val * + sqrt] let.
  3 4 pyth   (* -> 5.0 *)
  ```

  **Limitations:**
  - Names must be user symbols, not builtin operators
  - Only `i` and `x` are single-letter builtins; all other letters (`a`-`h`, `j`-`w`, `y`, `z`) work fine

  **Benefits:**
  - **Readability** - Code reads like math, not stack manipulation
  - **Correctness** - No risk of wrong `swap`/`rollup`/`rolldown` sequence
  - **Maintainability** - Adding a term doesn't require rethinking the whole stack
  - **Debugging** - Variables have names, not stack positions

  See `tests/test2/let.joy` for more examples.

- [x] **Pattern matching** - Match and destructure values

  **Combinators:**
  - `cases` - Multi-pattern dispatch: `value [[pat1] [act1]] [[pat2] [act2]] ... cases`
  - `match` - Single pattern: `value [pattern] [action] match`

  **Pattern syntax:**
  | Pattern | Matches | Binds |
  |---------|---------|-------|
  | `0`, `"hi"`, `true` | Literal value | nothing |
  | `_` | Anything (wildcard) | nothing |
  | `n` | Anything | `n` = matched value |
  | `[h : t]` | Non-empty list | `h` = head, `t` = tail |
  | `[a b c]` | Exactly 3-element list | `a`, `b`, `c` = elements |
  | `[]` | Empty list | nothing |

  Note: `:` follows Haskell convention and disambiguates `[h : t]` (1+ elements)
  from `[h t]` (exactly 2 elements). Variable names must be user symbols
  (not builtins like `x` or `i`).

  **Examples:**
  ```joy
  (* Variable binding *)
  5 [n] [n n *] match .                    (* -> 25 *)

  (* Destructure list *)
  [1 2 3] [[h : t]] [h t] match .          (* -> 1 [2 3] *)

  (* Factorial with pattern matching *)
  DEFINE fact ==
      [[[0] [1]]
       [[n] [n 1 - fact n *]]] cases.
  5 fact .                                 (* -> 120 *)

  (* Safe head with default *)
  DEFINE safehead ==
      [[[[]] [0]]
       [[[h : _]] [h]]] cases.
  [] safehead .                            (* -> 0 *)
  [5 6] safehead .                         (* -> 5 *)

  (* List length *)
  DEFINE len ==
      [[[[]] [0]]
       [[[_ : vs]] [vs len 1 +]]] cases.
  [1 2 3 4] len .                          (* -> 4 *)
  ```

  **Difference from existing combinators:**
  - `ifte` - Tests conditions, doesn't destructure
  - `opcase` - Dispatches on type, doesn't bind parts
  - `cases`/`match` - Destructures AND binds in one step

  See `tests/test2/match.joy` and `tests/test2/cases.joy` for more examples.

- [ ] **Lazy sequences** - Infinite/deferred lists
  - `1 [1 +] iterate` -> lazy `[1 2 3 4 ...]`
  - `lazy-seq 10 take` -> `[1 2 3 4 5 6 7 8 9 10]`
  - Generators: `[yield-value] generator`

- [x] **Dictionaries** - Key-value data structure (DONE)
  - `dempty`, `dput`, `dget`, `dgetd`, `dhas`, `ddel`, `dkeys`, `dvals`, `dsize`
  - `>dict` from assoc list, `dict>` to assoc list, `dmerge`
  - See `tests/test2/dict.joy`

#### Data Formats

- [x] **JSON support** - Parse and emit JSON (DONE)
  - `json>` parses JSON to Joy values (dicts, lists, strings, numbers, bools, null)
  - `>json` emits Joy values as JSON
  - See `tests/test2/json.joy`

### Priority 3: Nice to Have

#### Language Features

- [ ] **Regular expressions** - Pattern matching on strings
  - `"hello world" "w.*d" regex-match` -> `true`
  - `"hello world" "(\w+)" regex-find-all` -> `["hello" "world"]`

- [x] **String interpolation** - Embed expressions in strings (DONE)
  - `$"Hello ${name}!"` syntax with `${...}` for expressions
  - See `tests/test2/strinterp.joy`

- [ ] **Futures/async** - Asynchronous computation
  - `[expensive-computation] future` -> future-handle
  - `future-handle await` -> result (blocks until ready)

#### Interop

- [ ] **FFI (Foreign Function Interface)** - Call C functions
  - `"libm.so" "sin" [double] [double] ffi` -> callable
  - Would enable integration with native libraries

- [ ] **HTTP client** - Basic HTTP requests
  - `"https://api.example.com" http-get` -> response
  - `url headers body http-post` -> response

#### Developer Experience

- [ ] **Stepper/debugger** - Interactive debugging
  - Step through execution one operation at a time
  - Inspect stack, dump, symbol table at each step
  - Breakpoints on symbols

- [ ] **Profiler** - Performance analysis
  - Time spent in each user-defined symbol
  - Call counts and cumulative times
  - `profile [code]` combinator

- [ ] **Doc generator** - Generate documentation
  - Extract `(* ... *)` comments from library files
  - Generate markdown/HTML documentation
  - Include stack effects and examples

- [ ] **Code formatter** - Auto-format Joy code
  - Consistent indentation and spacing
  - `joy --fmt file.joy`

#### Performance

- [ ] **Bytecode compiler** - Faster execution
  - Compile Joy to bytecode instead of interpreting AST
  - Would significantly speed up tight loops

- [ ] **String interning** - Deduplicate strings
  - Identical strings share storage
  - Faster string comparison (pointer equality)

### Future Ideas (Research Required)

- [ ] **Channels/CSP** - Communicating Sequential Processes
  - `chan` creates channel, `send`/`recv` for message passing
  - Would enable more concurrent programming patterns

- [ ] **LSP server** - Editor integration
  - Autocompletion, go-to-definition, hover docs
  - VSCode, Neovim, Emacs integration

- [ ] **Effect system** - Algebraic effects
  - Track side effects in types
  - Enable effect handlers

- [ ] **Persistent sessions** - SQLite-backed transparent persistence
  - `"name" session.` opens persistent session where all DEFINEs survive
  - Snapshots: `"name" snapshot.` / `"name" restore.`
  - Session merging: `"other" session-merge.` with conflict detection
  - Lazy loading for large structures
  - SQL queries on session data
  - See `doc/persistent_session_design.md`

---

## Test Examples

All of these now work correctly:

```joy
(* Vector operations *)
[1 2 3] [4 5 6] v+.                                       (* -> [5.0 7.0 9.0] *)
[1 2 3] [4 5 6] dot.                                      (* -> 32.0 *)
[1 2 3 4 5] vsum.                                         (* -> 15.0 *)
[1 2 3] 10 vscale.                                        (* -> [10.0 20.0 30.0] *)
1 5 vrange.                                               (* -> [1 2 3 4 5] *)

(* Iterative combinators *)
[1 2 3 4] [10 [dup +] times] pmap.                        (* -> [1024 2048 3072 4096] *)
[10 20 30 40] [[0 >] [1 -] while] pmap.                   (* -> [0 0 0 0] *)

(* Recursive combinators *)
[5 6 7 8] [[0 =] [pop 1] [dup 1 -] [*] linrec] pmap.     (* -> [120 720 5040 40320] *)
[100 500 1000] [[0 =] [] [dup 1 -] [+] linrec] pmap.     (* -> [5050 125250 500500] *)

(* Vectors with parallel execution *)
[[1 2 3] [4 5 6] [7 8 9]] [[10 10 10] v+] pmap.          (* -> [[11 12 13] [14 15 16] [17 18 19]] *)

(* Stress test *)
[1 2 3 4 5 6 7 8] [1000000 [dup * 2 mod] times] pmap.    (* 1M iterations - works! *)
```

---

## Related Files

| File | Description |
|------|-------------|
| `doc/parallel.md` | Parallel execution user guide and examples |
| `doc/parallel_fixes.md` | Technical documentation of parallel execution fixes |
| `doc/parallel_performance.md` | Performance benchmarks and usage guidelines |
| `doc/vector.md` | Vector/matrix operations design document |
| `doc/vector_impl.md` | Vector operations implementation guide |
| `doc/architecture.md` | Architecture review and improvement plan |
| `include/parallel.h` | Parallel infrastructure (`copy_node_to_parent`) |
| `include/joy/joy.h` | Public API header for embedding |
| `src/utils.c` | GC and memory management (`copy` function) |
| `src/interp.c` | Interpreter (`copy_body_from_parent`) |
| `src/builtin/parallel.c` | Parallel combinators (pmap, pfork) |
| `src/builtin/vector.c` | Vector and matrix operations |
| `src/builtin/*.c` | Grouped builtin implementations (19 files) |
| `src/builtin/pattern.c` | Pattern matching combinators (match, cases) |
| `tests/test2/vector.joy` | Vector operations test suite |
| `tests/test2/match.joy` | Pattern matching test suite (match) |
| `tests/test2/cases.joy` | Pattern matching test suite (cases) |
| `tests/test2/matrix.joy` | Matrix operations test suite |
| `tests/parallel_benchmark.sh` | Wall-time benchmark script |
| `tests/parallel_benchmark.joy` | CPU-time benchmark (Joy) |
