# Joy Language TODO

## Completed (January 2026)

Summary of completed work:
- **Parallel execution** - `pfork`/`pmap`/`pfilter`/`preduce` implemented. Not working: see Known defects.
- **Architecture** - Grouped builtins (229->29 files), Env sub-structs, inline accessors
- **Vector/Matrix ops** - Full suite with SIMD and optional BLAS support
- **Language features** - `let` bindings, pattern matching (`match`/`cases`), dictionaries, JSON, string interpolation
- **Persistent sessions** - Persistence with snapshots and merging. Dictionaries do not round-trip: see Known defects.
- **Regular expressions** - POSIX ERE with Perl shortcuts (`\d`, `\w`, `\s`, etc.): `regex-match`, `regex-find`, `regex-find-all`, `regex-split`, `regex-sub`, `regex-sub-all`, `regex-groups`

See `doc/` for detailed documentation on each feature.

## Known defects

- **Parallel** - `-DJOY_PARALLEL=ON` does not compile: `src/builtin/parallel.c` never includes `parallel.h`.
  Child contexts share the symbol table while running any builtin (data race).
  `parallel.c` holds result `Index`es unrooted across allocation (GC hazard).
- **Sessions** - Dictionaries are written as `{"k": v}`, but `{` parses as a set literal, so they cannot be reloaded.
- **Compile-to-C** - `tools/joy2c.py` duplicates `compiler.c` and maps to C symbols that do not exist.
- **Code formatter** - `tools/fmt_joy.py` drops `:` in `match`/`cases` patterns and splits `$"..."` tokens.
- **Doc generator** - `tools/gen_docs.py` misses `.`-terminated definitions (`map-reduce`) and reads definitions inside comments.
- **Libraries** - `flatten` is defined in both `lib/seqlib.joy` (recursive) and `lib/mapreduce.joy` (one level).
- **Embedding API** - `src/joy.c:76` sets `bottom_of_stack` to a static variable, so the conservative GC never scans the C stack of embedded contexts.
- **Sessions: GC** - `session-diff` and `session-merge` build result lists with `STRING_NEWNODE` in a loop without `ensure_capacity`; a GC moves the partial list.

### Unverified (in CI, not run locally)

- `JOY_BLAS` build: no BLAS headers on the dev machine.
- The `JOY_PARALLEL` job uses job-level `continue-on-error`; it is assumed, not confirmed, that `ci-success` then sees `needs.features.result == success`.
- Windows: `run_test.sh` and the session scripts need bash, so the Windows test step is still `continue-on-error`.

### Reported by the review, not yet re-verified

Interpreter and builtins:
- `regex.c`: `\D`/`\W`/`\S` inside `[...]` expand wrongly; `regerror` reads an uninitialized `regex_t` after a failed `regcomp`; zero-width matches give empty segments in `regex-split`.
- `casting_` (`type.c`) reads non-string values as NUL-terminated strings.
- `__dump_` (`internal.c`) pushes `0` instead of the dump.
- `fread_`/`fgets_` (`io.c`): int64-to-int truncation, no negative-count check, unbounded growth on newline-less input; failed `fopen` pushes a NULL `FILE_`.
- `strtol_`/`strtod_` accept partial conversions (`"12abc" strtol` gives 12) and overflow silently.
- `Compare` orders `FILE_`/`DICT_` pointers with `<` (UB); `UNMKTIME` dereferences a NULL `gmtime`/`localtime` result; `equal_aux` recursion is unbounded.
- `pmap`: the parallel path skips elements whose quotation empties the stack; the sequential path raises an error.
- Multiple `joy_create` contexts share global GC state; `joy_memory_max` returns `stats.avail`.
- `factor.c` literal parsing: unchecked `realloc` in `v[`/`m[[` readers.

Compile-to-C (`compiler.c`):
- NaN/Inf floats emit `nan`/`inf`, which is not valid C.
- `sanitize_name` collisions (`a-b` vs `a_b`) give duplicate functions.
- `cbuf_printf` truncates at 1024 bytes.
- Inlined `while` and `times` match user words with those names (`is_builtin_name` accepts `USR_`); inlined `while` differs from the runtime.

LSP (`src/lsp/`):
- `initialized`/`shutdown_requested` are never read; `main` exits 0 regardless of shutdown.
- Transport: case-variant, missing or >255-char `Content-Length` ends the server.
- Positions are byte columns, not UTF-16 code units.
- `didChange` accepts incremental changes against full-sync; duplicate `didOpen` leaks a key.
- Grammar diverges from `scan.c` (unknown escapes, exponent floats, `END`-terminated definitions, keywords in quotations); it is vendored twice (`tree-sitter-joy/src`, `ext/tree-sitter-joy`).

Tooling and tests:
- `gen_builtins.py` regex matches macro calls inside comments.
- Committed `*.out`/`*.old` files under `tests/` and `lib/` are stale and never diffed.
- The CI lint job is a no-op (`continue-on-error`, first 20 files only).
- `tests/test2/argv.joy` is not registered: it must run as the main file, and the runner includes tests from stdin.

---

## TODO (Prioritized)

### Priority 1: High Value / Medium Effort

- [x] **Lazy sequences** - Infinite/deferred lists
  - `1 [1 +] iterate` -> lazy `[1 2 3 4 ...]`
  - `lazy-seq 10 take` -> `[1 2 3 4 5 6 7 8 9 10]`
  - Generators: `[yield-value] generator`
  - Important functional programming primitive

- [x] **Stepper/debugger** - Interactive debugging
  - Step through execution one operation at a time
  - Inspect stack, dump, symbol table at each step
  - Breakpoints on symbols
  - Essential for debugging non-trivial programs

- [x] **LSP server** - Editor integration
  - Autocompletion, go-to-definition, hover docs, diagnostics, document symbols
  - VSCode, Neovim, Emacs integration via `joy-lsp` binary
  - Build: `make joy-lsp`

### Priority 3: Medium Value

- [x] **Profiler** - Performance analysis
  - Time spent in each user-defined symbol
  - Call counts and cumulative times
  - `profile [code]` combinator

- [ ] **HTTP client** - Basic HTTP requests
  - `"https://api.example.com" http-get` -> response
  - `url headers body http-post` -> response
  - Enables web integration

- [ ] **FFI (Foreign Function Interface)** - Call C functions
  - `"libm.so" "sin" [double] [double] ffi` -> callable
  - Would enable integration with native libraries

- [ ] **Persistent sessions: Lazy loading** - Load symbols on demand
  - Add `heap` table for chunked storage of large structures
  - Modify symbol lookup: check cache, then SQLite on miss
  - LRU cache eviction when over limit

### Priority 4: Nice to Have

- [x] **Doc generator** - Generate documentation
  - Extract `(* ... *)` comments from library files
  - Generate markdown/HTML documentation
  - Include stack effects and examples

- [ ] **Code formatter** - Auto-format Joy code (exists but corrupts code; see Known defects)
  - Consistent indentation and spacing
  - `python tools/fmt_joy.py file.joy`

- [ ] **Futures/async** - Asynchronous computation
  - `[expensive-computation] future` -> future-handle
  - `future-handle await` -> result (blocks until ready)

- [ ] **String interning** - Deduplicate strings
  - Identical strings share storage
  - Faster string comparison (pointer equality)

- [ ] **Persistent sessions: Utilities** - Additional operators
  - `session-keep`, `session-info`, `session-gc`, `session-export`, `session-clone`

### Future Ideas (Research Required)

- [ ] **Channels/CSP** - Communicating Sequential Processes
  - `chan` creates channel, `send`/`recv` for message passing

- [ ] **Effect system** - Algebraic effects
  - Track side effects in types
  - Enable effect handlers

---

## Related Files

| File | Description |
|------|-------------|
| `doc/parallel.md` | Parallel execution user guide |
| `doc/vector.md` | Vector/matrix operations design |
| `doc/architecture.md` | Architecture review |
| `doc/persistent_session_design.md` | Persistent sessions design |
| `doc/persistent_session_impl.md` | Persistent sessions implementation |
