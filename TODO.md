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
  - Linear algebra: `dot`
  - Reductions: `vsum`, `vprod`, `vmin`, `vmax`
  - Creation: `vzeros`, `vones`, `vrange`
  - Tests: `tests/test2/vector.joy` (28 assertions)

- [x] **Matrix operations** - 2D array operations in `src/builtin/vector.c`
  - Element-wise: `m+`, `m-`, `m*`, `m/`
  - Scalar: `mscale`
  - Linear algebra: `mm` (matmul), `mv` (matrix-vector), `transpose`
  - Properties: `det`, `inv`, `trace`
  - Creation: `meye` (identity matrix)
  - Tests: `tests/test2/matrix.joy` (26 assertions)

See `doc/vector.md` for design rationale.
See `doc/vector_impl.md` for implementation details.

---

## TODO (Prioritized)

### Priority 1: High Value / Low-Medium Effort

#### Numeric Computing

- [ ] **Advanced vector operations** - Additional vector functionality
  - `vnorm`, `vnormalize` - magnitude and unit vector
  - `cross` - cross product (3D vectors)
  - `vmean` - mean of elements
  - `vlinspace` - linearly spaced values

- [ ] **SIMD optimization** - Performance improvements for vector/matrix ops
  - Contiguous storage for homogeneous numeric lists
  - SIMD vectorization with `#pragma omp simd`
  - Optional BLAS/LAPACK integration

#### Extended Parallel Combinators

- [ ] **pfilter** - Parallel filter combinator
  - `[list] [predicate] pfilter -> [filtered]`
  - Similar implementation pattern to `pmap`

- [ ] **preduce** - Parallel tree reduction
  - `[list] [binary-op] preduce -> result`
  - For associative operations (sum, product, max, min)
  - Divide-and-conquer parallelism

### Priority 2: Medium Value / Medium Effort

#### Language Features

- [ ] **Local bindings** - Bind stack values to names within quotation
  - `10 20 [x y] let [x y + x y *]` -> `30 200`
  - Alternative: `10 20 [+ *] [x y] bindrec`
  - Reduces stack juggling for complex expressions

- [ ] **Pattern matching** - Match and destructure values
  - `value [[pattern1] [action1]] [[pattern2] [action2]] cases`
  - `[1 2 3] [[[x . xs]] [x xs]] match` -> `1 [2 3]`

- [ ] **Lazy sequences** - Infinite/deferred lists
  - `1 [1 +] iterate` -> lazy `[1 2 3 4 ...]`
  - `lazy-seq 10 take` -> `[1 2 3 4 5 6 7 8 9 10]`
  - Generators: `[yield-value] generator`

- [ ] **Hash tables** - Dictionary/map data structure
  - `{key1 val1 key2 val2}` literal syntax or `alist>hash`
  - `hash key get`, `hash key val put`, `hash keys`, `hash vals`

#### Data Formats

- [ ] **JSON support** - Parse and emit JSON
  - `"[1,2,3]" json>` -> `[1 2 3]`
  - `[1 2 3] >json` -> `"[1,2,3]"`
  - Maps to Joy lists/strings/numbers

### Priority 3: Nice to Have

#### Language Features

- [ ] **Regular expressions** - Pattern matching on strings
  - `"hello world" "w.*d" regex-match` -> `true`
  - `"hello world" "(\w+)" regex-find-all` -> `["hello" "world"]`

- [ ] **String interpolation** - Embed expressions in strings
  - `name "world" def "Hello ${name}!" interp` -> `"Hello world!"`

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
| `tests/test2/vector.joy` | Vector operations test suite |
| `tests/test2/matrix.joy` | Matrix operations test suite |
| `tests/parallel_benchmark.sh` | Wall-time benchmark script |
| `tests/parallel_benchmark.joy` | CPU-time benchmark (Joy) |
