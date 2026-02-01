# Joy Language TODO

## Completed (January 2026)

Summary of completed work:
- **Parallel execution** - Stack overflow fixes, all combinators working, `pfilter`/`preduce`
- **Architecture** - Grouped builtins (229->18 files), Env sub-structs, inline accessors
- **Vector/Matrix ops** - Full suite with SIMD and optional BLAS support
- **Language features** - `let` bindings, pattern matching (`match`/`cases`), dictionaries, JSON, string interpolation
- **Persistent sessions** - Full persistence with snapshots, merging, and complete deserialization of all value types
- **Regular expressions** - POSIX ERE with Perl shortcuts (`\d`, `\w`, `\s`, etc.): `regex-match`, `regex-find`, `regex-find-all`, `regex-split`, `regex-sub`, `regex-sub-all`, `regex-groups`

See `doc/` for detailed documentation on each feature.

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

- [ ] **Bytecode compiler** - Faster execution
  - Compile Joy to bytecode instead of interpreting AST
  - Would significantly speed up tight loops

- [ ] **LSP server** - Editor integration
  - Autocompletion, go-to-definition, hover docs
  - VSCode, Neovim, Emacs integration
  - High developer experience impact; would make Joy practical for larger projects

### Priority 3: Medium Value

- [ ] **Profiler** - Performance analysis
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

- [ ] **Doc generator** - Generate documentation
  - Extract `(* ... *)` comments from library files
  - Generate markdown/HTML documentation
  - Include stack effects and examples

- [ ] **Code formatter** - Auto-format Joy code
  - Consistent indentation and spacing
  - `joy --fmt file.joy`

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
