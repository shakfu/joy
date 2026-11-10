# TODO

## Critical

## High

## Medium

- [ ] **Output bypasses I/O callbacks** - `putchars` and about 60 other writes in `src/builtin/{internal,io,session,profiler}.c` call `printf`/`putchar` directly, so an embedder's `write_char`/`write_string` never see them. `put` goes through `src/iolib.c`.

- [ ] **include in joy_eval_string** - An included file is read after the rest of the string: the string is in the pushback, which `getch` reads first. `joy_load_stdlib` and `joy_eval_file` are not affected.

- [ ] **Nested eval with DEFINE** - A definition in an eval run from an I/O callback calls `inimem1(env, 1)`, which resets the node heap under the outer statement that is still running.

- [ ] **Result codes** - A runtime error returns `JOY_ERROR_ABORT`; `JOY_ERROR_RUNTIME`, `JOY_ERROR_TYPE` and `JOY_ERROR_STACK_UNDERFLOW` are never returned.

- [ ] **LSP positions** - Columns are bytes, sent and received unchanged (`diagnostics.c:29`, `document.c:131, 242`). There is no `positionEncoding` negotiation, so non-ASCII lines are off.

- [ ] **Tree-sitter grammar** - Diverges from `scan.c`:
  - Unknown escapes: `scan.c` keeps the character; the grammar gives ERROR.

  - Numeric escapes: `scan.c` reads 3 decimal digits; the grammar expects 1-3 octal.

  - `1.5e3`: `scan.c` reads a float; the grammar reads float then symbol `e3`. Neither accepts `1e3`.

  - Hex integers: `scan.c` accepts them (`strtoll` base 0); the grammar does not.

  - `END` ends a definition in `scan.c:17`; the grammar reports MISSING ".".

  - `[IN]`: the grammar accepts it; the runtime rejects it.

- [ ] **Tree-sitter vendoring** - `tree-sitter-joy/src` and `ext/tree-sitter-joy` are identical copies. `CMakeLists.txt:146` builds only `ext/`, so a regenerated grammar is not used until copied.

- [ ] **CI lint** - The `lint` job in `.github/workflows/cmake.yml` cannot fail: `|| echo`, `continue-on-error`, and stderr to `/dev/null`. It checks only the first 20 files. There is no `.clang-format`, and `src/` does not pass the default style (374 diagnostics in `compiler.c` alone), so making it blocking needs a style file and a reformat. `ext/` and the generated parsers should be excluded.

- [ ] **argv.joy** - Not registered and never run: the runner includes tests from stdin, so `argv 0 at` is not the file name.

- [ ] **lib/ test outputs** - `lib/CMakeLists.txt` writes `jp-joytst`, `laztst`, `grmtst` and 13 others to the build directory at build time. Nothing checks them.

- [ ] **Shell-dependent output** - `tests/test4/scan1.joy` has no `.out`: `$ ls` lists a directory other tests write to, and `$ TECO`'s error differs between macOS `sh` and dash.

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

- [ ] **GC stress runtime** - Under `-DJOY_GC_STRESS=ON`, `test2_dict_gc` takes 900 s and `parallel_semantics` 250 s; everything else finishes in seconds. A CI stress job would need a longer timeout or to skip both.

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
