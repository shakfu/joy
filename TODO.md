# TODO

## Critical

## High

- [ ] **GC stress** - With `-DJOY_GC_STRESS=ON -DJOY_SESSION=ON`, all four session tests fail (`session_persist` reports `bad node` on reopen); HEAD fails the same way, and they pass without stress. The non-session tests pass under stress, but `test2_dict_gc` takes 900 s and `parallel_semantics` 250 s. The earlier `test2_regex` and `size_errors` failures no longer reproduce.

## Medium

- [ ] **fgets** - `fgets_` (`io.c:76-86`) doubles its buffer without a cap on newline-free input. The BDW `GC_realloc` path is unchecked.

- [ ] **__dump** - `__dump_` (`internal.c:100`) pushes integer `0`; its doc says it pushes the dump as a list.

- [ ] **joy_memory_max** - Returns `stats.avail` (`joy.c:506`), the minimum count of free nodes. The header documents "maximum memory size"; `env->memorymax` matches that.

- [ ] **Shared GC state** - Under NOBDW each `joy_create` context has its own node `gc_ctx` (`joy.c:84`). `GC_INIT` and the legacy `GC_malloc` API use one static `default_ctx` (`gc.c:72`), which kvec and builtins share. Under BDW the whole heap is shared. The `joy.c:57` comment overstates the sharing for node memory.

- [ ] **factor.c** - `values = realloc(values, ...)` is unchecked in the `v[`/`m[[` readers (`factor.c:180, 238`). On OOM it leaks the old block and writes through NULL. The `strdup` at `factor.c:53` is also unchecked.

- [ ] **LSP lifecycle** - `initialized`/`shutdown_requested` (`server.c:20`) are written, never read. Requests before `initialize` are handled. `main.c:55` returns 0; the spec requires 1 when `exit` arrives without `shutdown`.

- [ ] **LSP positions** - Columns are bytes, sent and received unchanged (`diagnostics.c:29`, `document.c:131, 242`). There is no `positionEncoding` negotiation, so non-ASCII lines are off.

- [ ] **LSP documents** - Duplicate `didOpen` leaks the `strdup`'d key (`document.c:165`); the comment at line 175 claims it is freed. `didChange` ignores `range` (`server.c:144`); the server advertises full sync, so only non-compliant clients corrupt the document.

- [ ] **Tree-sitter grammar** - Diverges from `scan.c`:
  - Unknown escapes: `scan.c` keeps the character; the grammar gives ERROR.

  - Numeric escapes: `scan.c` reads 3 decimal digits; the grammar expects 1-3 octal.

  - `1.5e3`: `scan.c` reads a float; the grammar reads float then symbol `e3`. Neither accepts `1e3`.

  - Hex integers: `scan.c` accepts them (`strtoll` base 0); the grammar does not.

  - `END` ends a definition in `scan.c:17`; the grammar reports MISSING ".".

  - `[IN]`: the grammar accepts it; the runtime rejects it.

- [ ] **Tree-sitter vendoring** - `tree-sitter-joy/src` and `ext/tree-sitter-joy` are identical copies. `CMakeLists.txt:146` builds only `ext/`, so a regenerated grammar is not used until copied.

- [ ] **Stale `.out` files** - About 228 `tests/test2/*.out` and 17 `tests/test4/*.out` are committed but never compared; `run_test.sh` only greps for `false`/errors. 18 test2 and 5 test4 files differ from a local build, some from clock/random output. `let`, `match`, `vector`, `regex`, `lazy`, `cases`, `debugger` have none. Either diff them in the runner or delete them.

- [ ] **CI lint** - The `lint` job in `.github/workflows/cmake.yml` cannot fail: `|| echo`, `continue-on-error`, and stderr to `/dev/null`. It checks only the first 20 files. `ci-success` does not check `needs.lint.result` or `build-windows`.

- [ ] **argv.joy** - Not registered (`tests/test2/CMakeLists.txt:443`): the runner includes tests from stdin, so `argv 0 at` is not the file name. `exe9(argv)` generates unchecked output.

- [ ] **HTTP client** - Basic HTTP requests
  - `"https://api.example.com" http-get` -> response

  - `url headers body http-post` -> response

  - Enables web integration

- [ ] **Persistent sessions: Lazy loading** - Load symbols on demand
  - Add `heap` table for chunked storage of large structures

  - Modify symbol lookup: check cache, then SQLite on miss

  - LRU cache eviction when over limit

- [ ] **Futures/async** - Asynchronous computation
  - `[expensive-computation] future` -> future-handle

  - `future-handle await` -> result (blocks until ready)

- [ ] **String interning** - Deduplicate strings
  - Identical strings share storage

  - Faster string comparison (pointer equality)

- [ ] **Persistent sessions: Utilities** - Additional operators
  - `session-keep`, `session-info`, `session-gc`, `session-export`, `session-clone`

## Low

- [ ] **Compare** - Orders `FILE_`/`DICT_` pointers with `<` (`builtin_macros.h:280-289`). UB by C11 6.5.8p5, benign on flat-memory targets; compare via `uintptr_t`.

- [ ] **equal** - `equal_aux` (`comparison.c:34-51`) recurses on depth and list length. Not reproduced in isolation: other paths overflow first at 100k depth.

- [ ] **fopen** - A failed `fopen` pushes a NULL `FILE_` (documented, `io.c:117`). `ISFILE` rejects it, but compiles to nothing under `NCHECK`, so the NULL is dereferenced.

- [ ] **gen_builtins.py** - The regexes at `tools/gen_builtins.py:64, 69` do not skip comments or `#if 0`. No current source triggers it; a match adds an unused prototype, not a builtin. `gen_table.py` not checked.

### Unverified (in CI, not run locally)

- [ ] `JOY_BLAS` build: no BLAS headers on the dev machine.

- [ ] The `JOY_PARALLEL` job now blocks CI. It passes locally with clang and with GCC 16 on macOS, not yet on `ubuntu-latest`.

- [ ] Windows: `run_test.sh` and the session scripts need bash, so the Windows test step is still `continue-on-error`.

### Research Needed

- [ ] **Channels/CSP** - Communicating Sequential Processes
  - `chan` creates channel, `send`/`recv` for message passing

- [ ] **Effect system** - Algebraic effects
  - Track side effects in types

  - Enable effect handlers
