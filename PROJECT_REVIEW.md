# PROJECT_REVIEW.md

Comprehensive code, architecture, and feature review of the Joy interpreter.

## Executive Summary

The Joy interpreter is a well-engineered C implementation of the Joy programming language, a stack-based concatenative functional language. The codebase comprises approximately 12,000 lines of C code with 253 builtin primitives, a custom garbage collector, and a module system with scoping. The architecture is mature and demonstrates solid embedded systems principles, though several areas warrant attention for robustness and maintainability.

**Key Metrics:**
- Core interpreter: ~4,000 LOC across 14 modules
- Builtin primitives: 253 operations in individual source files
- Test suite: 225 core tests + 18 regression tests
- Platforms: Linux, macOS (GCC/Clang), Windows (MSVC)

---

## 1. Architecture Overview

### 1.1 Interpreter Pipeline

```
Input (stdin/file)
    |
    v
Scanner (scan.c) --> Tokenization
    |
    v
Parser (factor.c) --> AST Construction (list-based)
    |
    v
Evaluator (interp.c) --> Stack-based execution
    |
    v
Output (write.c, print.c)
```

### 1.2 Core Modules

| Module | Responsibility | LOC |
|--------|---------------|-----|
| `main.c` | Entry point, CLI, initialization | ~600 |
| `interp.c` | Term execution, dispatch | ~250 |
| `scan.c` | Lexical analysis, include handling | ~700 |
| `factor.c` | Parsing, AST construction | ~400 |
| `repl.c` | Read-eval-print loop | ~100 |
| `symbol.c` | Symbol table, definitions | ~150 |
| `module.c` | Module/scope management | ~200 |
| `gc.c` | Garbage collection (BDW mode) | ~430 |
| `utils.c` | Memory management (NOBDW mode) | ~400 |
| `optable.c` | Primitive dispatch table | ~250 |
| `error.c` | Error reporting | ~60 |
| `write.c` | Output formatting | ~200 |

### 1.3 Data Flow

1. **Scanner** (`getsym()`) tokenizes input into symbols (identifiers, numbers, strings, operators)
2. **Parser** (`readterm()`, `readfactor()`) builds linked-list AST on the stack
3. **Evaluator** (`exeterm()`) dispatches on node type:
   - Literals: pushed onto stack
   - User symbols: body executed recursively
   - Builtins: C function called via dispatch table
4. **Symbol Table** provides name resolution with module scoping

---

## 2. Memory Management

### 2.1 Dual GC Architecture

The interpreter supports two memory management modes:

**NOBDW Mode (Custom GC):**
- Two-space copying collector in `utils.c`
- Nodes stored in contiguous array indexed by `Index` (unsigned int)
- Strings stored inline across multiple nodes
- Explicit GC trigger on allocation failure
- Adaptive sizing: shrinks at <10% occupancy, expands at >90%

**BDW Mode (Boehm GC):**
- Conservative mark-and-sweep in `gc.c`
- Pointer-based indexing (`Index` = `Node*`)
- Automatic collection via allocation tracking
- Uses khash for block registry

### 2.2 Node Structure

```c
typedef struct Node {
    unsigned op : 4, len : 28;  // Type + string length (NOBDW)
    Index next;                  // Next node in list
    Types u;                     // Union: num/dbl/str/lis/fil/proc
} Node;
```

**Supported Types:** USR_, ANON_FUNCT_, BOOLEAN_, CHAR_, INTEGER_, SET_, LIST_, FLOAT_, FILE_, STRING_, BIGNUM_

### 2.3 Issues Identified

| Issue | Severity | Location |
|-------|----------|----------|
| No allocation error checks on non-MSVC | CRITICAL | utils.c:40,210,258,301 |
| String strdup() not tracked by GC | HIGH | utils.c:343 |
| Recursive mark_ptr risks stack overflow | MEDIUM | gc.c:197 |
| No thread safety on global state | HIGH | globals.h |

---

## 3. Symbol Table and Scoping

### 3.1 Dual-Storage Design

- **Vector (`symtab`):** Linear array of Entry structures, index used at runtime
- **Hash Table (`hash`):** khash string->int map for O(1) lookup during parsing

### 3.2 Four-Level Scope Hierarchy

1. **Global:** Unqualified names visible everywhere
2. **Module:** Names prefixed with `modulename.` (via MODULE keyword)
3. **Private/Hidden:** Names prefixed with numeric ID (via HIDE/PRIVATE)
4. **Immediate:** Compile-time constants (via CONST)

### 3.3 Lookup Algorithm (`qualify()`)

```
1. If name contains '.': direct hash lookup (qualified)
2. Search hide stack (innermost to module boundary)
3. Search active module scope
4. Fall back to global scope
```

**Performance:** O(1) typical, O(depth) worst case where depth <= 10

### 3.4 Issues Identified

| Issue | Severity | Notes |
|-------|----------|-------|
| Fixed nesting limit (DISPLAYMAX=10) | LOW | Stack overflow if exceeded |
| Ghost symbols accumulate in vector | LOW | Memory not reclaimed after module exit |
| Numeric prefix collision risk | LOW | Hide ID could theoretically collide with symbol names |

---

## 4. Builtin Primitives System

### 4.1 Organization

- **253 primitives** in `src/builtin/`, one file per operation
- **26 macro headers** for reusable patterns
- **Code generation:** `gen_builtins.py` and `gen_table.py` produce dispatch glue

### 4.2 Primitive Categories

| Category | Count | Examples |
|----------|-------|----------|
| Stack Operations | 25+ | dup, swap, pop, rotate, pick |
| Arithmetic | 30+ | plus, minus, mul, div, sin, cos, sqrt |
| List/Aggregate | 40+ | map, filter, fold, cons, first, rest |
| Control Flow | 30+ | ifte, while, linrec, binrec, genrec |
| Type Operations | 25+ | type, typeof, integer, string, casting |
| I/O | 20+ | put, get, fopen, fclose, fread |
| Logical | 10+ | and, or, xor, not, less, greater |
| Higher-Order | 20+ | app1, app2, unary, binary, dip |

### 4.3 Implementation Patterns

**Macro-Based (40%):** Delegates to high-level macros
```c
UFLOAT(sin_, "sin", sin)
PUSH(true_, BOOLEAN_NEWNODE, 1)
```

**Direct Stack Manipulation (30%):** Uses NULLARY/UNARY/BINARY macros
```c
void swap_(pEnv env) {
    TWOPARAMS("swap");
    SAVESTACK;
    GBINARY(SAVED1);
    GNULLARY(SAVED2);
    POP(env->dump);
}
```

**Higher-Order (20%):** Manages quotation execution via `exeterm()`

**Composite (5%):** Composes simpler primitives

### 4.4 Issues Identified

| Issue | Severity | Notes |
|-------|----------|-------|
| Aggregate operation code duplication | MEDIUM | map/filter/split repeat switch/case logic |
| Deep macro nesting obscures errors | LOW | Difficult debugging |
| Limited recursion depth (5 dump levels) | MEDIUM | Deep recursion fails |
| Generic error messages | LOW | Limited context for users |

---

## 5. Error Handling

### 5.1 Mechanism

Uses `setjmp`/`longjmp` for non-local exception handling:

```c
// Main recovery point
if (setjmp(begin) == ABORT_QUIT)
    goto einde;

// Error signaling
void execerror(pEnv env, char* message, char* op) {
    fprintf(stderr, "run time error: %s needed for %s\n", message, op);
    abortexecution_(ABORT_RETRY);
}
```

**Abort Codes:**
- `ABORT_NONE (0)`: No action
- `ABORT_RETRY (1)`: Return to REPL
- `ABORT_QUIT (2)`: Exit application

### 5.2 Validation Macros

Comprehensive parameter checking in `runtime.h`:
- `ONEPARAM`, `TWOPARAMS`, ... `FIVEPARAMS`: Stack depth
- `STRING`, `INTEGER`, `FLOAT`, `CHARACTER`: Type checking
- `ONEQUOTE`, `TWOQUOTES`, `THREEQUOTES`: Quotation validation
- `CHECKZERO`, `CHECKDIVISOR`: Domain validation

### 5.3 Issues Identified

| Issue | Severity | Location |
|-------|----------|----------|
| No recursion depth limit | CRITICAL | interp.c:77 |
| Memory errors only checked on MSVC | CRITICAL | utils.c multiple |
| Missing argument in abortexecution_() | HIGH | symbol.c:114 |
| Memory leaks on error paths | HIGH | symbol.c:39,119 |
| File descriptor leaks after longjmp | MEDIUM | scan.c |
| No stack trace in error messages | LOW | error.c |

---

## 6. Testing Infrastructure

### 6.1 Test Organization

- **test2/**: 225 core functionality tests (one per feature)
- **test4/**: 18 regression tests for previously caught bugs
- **Format:** Self-asserting Joy programs using `=.` operator

### 6.2 Test Execution

```bash
cd test2
for i in *.joy; do ../build/joy "$i" >"$i.out"; done
grep -l false *.out  # Find failures
```

### 6.3 CMake Integration

Tests enabled in Debug builds via `RUN_TESTS=ON`. CMake creates custom targets for each test file.

### 6.4 Issues Identified

| Issue | Severity | Notes |
|-------|----------|-------|
| No C-level unit tests | MEDIUM | Primitives tested only via Joy |
| Manual test execution required | MEDIUM | Not fully automated in CI |
| No test coverage metrics | LOW | Cannot measure completeness |
| Library setup required | MEDIUM | Tests need ~/usrlib configured |

---

## 7. Build System

### 7.1 CMake Configuration

- **Minimum:** CMake 3.29
- **C Standard:** C11
- **Compiler Flags:** `-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-unused-result`
- **Targets:** `joycore_static`, `joycore_shared`, `joy` executable

### 7.2 Code Generation Pipeline

1. `gen_builtins.py`: Generates `builtin.c` (includes) and `builtin.h` (declarations)
2. `gen_table.py`: Parses documentation blocks, generates `table.c` (dispatch table)

### 7.3 Makefile Wrapper

```makefile
make joy              # Build interpreter
make clean            # Remove build/
make clang-format     # Apply formatting
make clang-format-check # Verify formatting
make clang-tidy       # Static analysis
```

---

## 8. Code Quality Assessment

### 8.1 Strengths

1. **Clean separation of concerns:** Scanner, parser, interpreter well-isolated
2. **Tail-call optimization:** Prevents stack explosion for recursive definitions
3. **Pluggable memory models:** Supports both custom and Boehm GC
4. **Comprehensive primitive validation:** Macro-based parameter/type checking
5. **Code generation for consistency:** Dispatch table auto-generated from source
6. **Strict compilation:** Warnings as errors enforced
7. **Multi-platform support:** Linux, macOS, Windows

### 8.2 Weaknesses

1. **Platform-specific error handling:** Memory errors only checked on MSVC
2. **No thread safety:** Global state prevents concurrent use
3. **Duplicated memory model code:** NOBDW vs BDW implementations diverge
4. **Limited error context:** No stack traces, generic messages
5. **Fixed limits without guards:** DISPLAYMAX, INPSTACKMAX can overflow silently
6. **Testing gaps:** No unit tests, manual execution required

### 8.3 Technical Debt

| Area | Debt | Recommendation |
|------|------|----------------|
| Memory safety | Allocation errors unchecked | Add universal ALLOC_CHECK macro |
| Recursion | No depth limit | Add configurable MAX_DEPTH |
| Error handling | Platform-specific | Unify fatal() across platforms |
| Testing | Manual only | Add CTest integration |
| Documentation | Minimal inline | Add protocol diagrams |

---

## 9. Feature Completeness

### 9.1 Joy Language Features Implemented

- Stack operations (full set)
- Arithmetic and transcendental functions
- List/aggregate operations (map, filter, fold, etc.)
- Recursion combinators (linrec, binrec, genrec, primrec, tailrec)
- Control flow (ifte, while, cond, case, branch)
- Type system with checking and conversion
- File I/O operations
- Module system with HIDE/PRIVATE/PUBLIC
- String operations
- Set operations (64-bit limit)

### 9.2 Notable Limitations

| Feature | Limitation |
|---------|------------|
| Sets | Maximum 64 elements (SETSIZE) |
| Modules | Maximum 10 nesting levels |
| Include files | Maximum 10 deep |
| Recursion | Limited by C call stack |
| Strings | Stored inline, multi-node for long strings |

---

## 10. Security Considerations

### 10.1 Shell Escape

The `$` shell escape feature executes system commands:
```c
if (command_is_safe(command))
    system(command);
```

**Mitigations:**
- `command_is_safe()` validation
- Warning on rejected commands
- Disabled on Windows

**Risks:**
- No return value check on `system()`
- Relative command paths could be hijacked

### 10.2 Buffer Handling

- Fixed-size buffers: `linebuf[INPLINEMAX+1]` (256 bytes)
- Long lines silently truncated
- Path construction uses fixed buffers

### 10.3 Input Validation

- Escape sequences validated in strings
- Numeric range checking for sets (0-63)
- No validation of quotation contents before execution

---

## 11. Recommendations

### 11.1 Critical (Should Fix)

1. **Add universal allocation error checking:**
   ```c
   #define ALLOC_CHECK(ptr) if (!(ptr)) fatal("memory exhausted")
   ```

2. **Implement recursion depth limit:**
   ```c
   if (++env->call_depth > MAX_RECURSION_DEPTH)
       execerror(env, "recursion depth exceeded", "exeterm");
   ```

3. **Fix symbol.c:114:** Add argument to `abortexecution_()`

4. **Add resource cleanup on error paths:** Close files, free temporaries before longjmp

### 11.2 High Priority

1. **Unify memory model code:** Abstract NOBDW/BDW differences behind common interface
2. **Automate test execution:** Integrate with CTest for CI
3. **Add C-level unit tests:** Test primitives directly
4. **Document thread-safety limitations:** Clear warning in README

### 11.3 Medium Priority

1. **Refactor aggregate operations:** Extract common map/filter/split logic
2. **Enhance error messages:** Include source line numbers, stack depth
3. **Add depth limits:** Guard DISPLAYMAX, INPSTACKMAX with proper errors
4. **Implement dynamic sets:** Remove 64-element limitation

### 11.4 Low Priority

1. **Add stack trace generation:** Improve debugging experience
2. **Profile and optimize:** Identify hot paths, consider inlining
3. **Document memory layout:** Add diagrams for GC phases
4. **Add configuration options:** Expose limits as compile-time settings

---

## 12. Conclusion

The Joy interpreter is a mature, well-structured implementation demonstrating solid engineering practices for a stack-based language interpreter. The dual-mode garbage collection, comprehensive primitive library, and module system are notable strengths. Primary concerns center on platform-specific error handling, lack of recursion safeguards, and testing automation. With targeted improvements to memory safety and testing infrastructure, this would be a highly robust interpreter suitable for educational and research purposes.

**Overall Assessment:** Production-ready for single-threaded interactive use; requires attention to memory safety and testing for deployment in critical contexts.

---

*Review conducted: 2026-01-01*
*Codebase version: NOBDW, master branch*
