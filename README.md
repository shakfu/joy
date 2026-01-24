# Joy

A parallel-capable implementation of the Manfred von Thun's Joy programming language.

This implementation is a friendly fork of Ruurd Wiersma's [Joy implementation](https://github.com/Wodan58/Joy), which I believe is based on Manfred von Thun's implementation.

Build|Linux|Windows|Coverity
---|---|---|---
status|[![GitHub CI build status](https://github.com/Wodan58/Joy/actions/workflows/cmake.yml/badge.svg)](https://github.com/Wodan58/Joy/actions/workflows/cmake.yml)|[![AppVeyor CI build status](https://ci.appveyor.com/api/projects/status/github/Wodan58/Joy?branch=master&svg=true)](https://ci.appveyor.com/project/Wodan58/Joy)|[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/14641.svg)](https://scan.coverity.com/projects/wodan58-joy)

## Overview

[Joy](http://www.complang.tuwien.ac.at/anton/euroforth/ef01/thun01.pdf) is a purely functional, stack-based, concatenative programming language. This implementation extends Joy with **parallel execution capabilities** using OpenMP, enabling multi-core utilization for data-parallel operations.

This is the NOBDW (No Boehm-Demers-Weiser) version with a custom conservative garbage collector that supports per-context isolation for thread-safe parallel execution.

## Parallel Execution

Joy now supports parallel combinators that leverage multiple CPU cores:

### `pmap` - Parallel Map

Apply a quotation to each element of a list in parallel:

```joy
[1 2 3 4 5 6 7 8] [dup *] pmap.
(* Result: [1 4 9 16 25 36 49 64] *)

["hello" "world" "test" "data"] [size] pmap.
(* Result: [5 5 4 4] *)
```

### `pfork` - Parallel Fork

Execute two quotations concurrently with the same input:

```joy
10 [2 *] [3 +] pfork.
(* Results: 20 13 - both computed in parallel *)

5 [dup *] [dup dup * *] pfork.
(* Results: 25 125 - square and cube computed concurrently *)
```

### `pfilter` - Parallel Filter

Filter elements where predicate returns true, evaluated in parallel:

```joy
[1 2 3 4 5 6 7 8] [2 rem 0 =] pfilter.
(* Result: [2 4 6 8] - keep even numbers *)

[3 1 4 1 5 9 2 6] [3 >] pfilter.
(* Result: [4 5 9 6] - keep numbers > 3 *)
```

### `preduce` - Parallel Tree Reduction

Reduce a list using an associative binary operation with divide-and-conquer parallelism:

```joy
[1 2 3 4 5 6 7 8] [+] preduce.
(* Result: 36 - parallel sum *)

[3 1 4 1 5 9 2 6] [max] preduce.
(* Result: 9 - parallel maximum *)
```

### Performance

`pmap` has thread overhead, so it needs substantial work per element to outperform `map`:

| Work per Element | Recommendation | Speedup |
|------------------|----------------|---------|
| Light (simple ops) | Use `map` | pmap has overhead |
| Medium (~1k iterations) | Use `map` | Similar performance |
| Heavy (~100k iterations) | Use `pmap` | 20-25% faster |
| Very Heavy (~1M iterations) | Use `pmap` | 35-40% faster |

**Benchmark results** (OMP_NUM_THREADS=8):

```
--- Heavy Work (100000 iterations) ---
4 elements : map=0.015s pmap=0.013s  (pmap 13% faster)
8 elements : map=0.026s pmap=0.020s  (pmap 23% faster)

--- Very Heavy Work (1000000 iterations) ---
4 elements : map=0.133s pmap=0.086s  (pmap 35% faster)
8 elements : map=0.247s pmap=0.155s  (pmap 37% faster)
```

**Rule of thumb:** Use `pmap` when each element takes >10ms to process.

Run benchmarks:
```bash
OMP_NUM_THREADS=8 bash tests/parallel_benchmark.sh
```

See [doc/parallel_performance.md](doc/parallel_performance.md) for detailed analysis.

## Vector and Matrix Operations

Joy supports vectorized operations on numeric lists and matrices (lists of lists):

### Vector Arithmetic

```joy
[1 2 3] [4 5 6] v+.    (* -> [5.0 7.0 9.0] *)
[1 2 3] [4 5 6] v-.    (* -> [-3.0 -3.0 -3.0] *)
[1 2 3] 10 vscale.     (* -> [10.0 20.0 30.0] *)
[1 2 3] [4 5 6] dot.   (* -> 32.0 - dot product *)
[1 0 0] [0 1 0] cross. (* -> [0.0 0.0 1.0] - cross product *)
```

### Vector Norms and Reductions

```joy
[3 4] vnorm.           (* -> 5.0 - Euclidean magnitude *)
[3 4] vnormalize.      (* -> [0.6 0.8] - unit vector *)
[1 2 3 4 5] vsum.      (* -> 15.0 *)
[1 2 3 4 5] vmean.     (* -> 3.0 - arithmetic mean *)
[5 2 8 1 9] vmax.      (* -> 9.0 *)
```

### Vector Creation

```joy
5 vzeros.              (* -> [0 0 0 0 0] *)
1 5 vrange.            (* -> [1 2 3 4 5] *)
0 1 5 vlinspace.       (* -> [0.0 0.25 0.5 0.75 1.0] *)
```

### Matrix Operations

```joy
[[1 2] [3 4]] [[5 6] [7 8]] m+.  (* -> [[6 8] [10 12]] *)
[[1 2] [3 4]] 10 mscale.         (* -> [[10 20] [30 40]] *)
```

### Matrix Linear Algebra

```joy
[[1 2] [3 4]] [[5 6] [7 8]] mm.  (* -> [[19 22] [43 50]] - matmul *)
[[1 2] [3 4]] [5 6] mv.          (* -> [17 39] - matrix-vector *)
[[1 2] [3 4]] transpose.         (* -> [[1 3] [2 4]] *)
[[1 2] [3 4]] det.               (* -> -2.0 - determinant *)
[[1 2] [3 4]] inv.               (* -> [[-2 1] [1.5 -0.5]] *)
[[1 2] [3 4]] trace.             (* -> 5.0 *)
3 meye.                          (* -> 3x3 identity matrix *)
```

### Native Contiguous Types

For performance-critical code, Joy provides native contiguous storage types that eliminate linked-list-to-array conversion overhead when using BLAS:

```joy
(* Native vector literal syntax *)
v[1 2 3 4 5].                    (* -> v[1.0 2.0 3.0 4.0 5.0] *)

(* Native matrix literal syntax (row-major) *)
m[[1 2] [3 4]].                  (* -> m[[1.0 2.0][3.0 4.0]] *)

(* Type predicates *)
v[1 2 3] vector?.                (* -> true *)
m[[1 0] [0 1]] matrix?.          (* -> true *)
[1 2 3] vector?.                 (* -> false - regular list *)
```

#### Conversion Operators

```joy
(* List to native *)
[1 2 3] >vec.                    (* -> v[1.0 2.0 3.0] *)
[[1 2] [3 4]] >mat.              (* -> m[[1.0 2.0][3.0 4.0]] *)

(* Native to list *)
v[1 2 3] >list.                  (* -> [1.0 2.0 3.0] *)
m[[1 2] [3 4]] >list.            (* -> [[1.0 2.0] [3.0 4.0]] *)
```

#### Native Creation Functions

```joy
(* Native vectors *)
5 nvzeros.                       (* -> v[0.0 0.0 0.0 0.0 0.0] *)
3 nvones.                        (* -> v[1.0 1.0 1.0] *)

(* Native matrices *)
2 3 nmzeros.                     (* -> 2x3 zero matrix *)
2 2 nmones.                      (* -> 2x2 ones matrix *)
3 nmeye.                         (* -> 3x3 identity matrix *)
```

#### Native BLAS Operations

These operations work directly on native types without conversion:

```joy
(* Dot product - uses cblas_ddot *)
v[1 2 3] v[4 5 6] ndot.          (* -> 32.0 *)

(* Matrix-vector multiply - uses cblas_dgemv *)
m[[1 0] [0 1]] v[3 4] nmv.       (* -> v[3.0 4.0] *)

(* Matrix multiply - uses cblas_dgemm *)
m[[1 2] [3 4]] m[[1 0] [0 1]] nmm.  (* -> m[[1.0 2.0][3.0 4.0]] *)
```

#### When to Use Native Types

| Scenario | Recommendation |
|----------|----------------|
| Small vectors (< 10 elements) | Use lists - simpler, overhead minimal |
| Large vectors/matrices | Use native types - eliminates conversion |
| Repeated BLAS operations | Use native types - significant speedup |
| Interop with Joy list ops | Use lists - more compatible |

The native types store elements contiguously in memory (matrices in row-major order), enabling direct BLAS calls without intermediate array allocation.

All operations include error handling for type mismatches and dimension errors.

## Local Bindings

The `let` combinator binds stack values to names within a quotation, making complex stack manipulation more readable:

```joy
(* Syntax: X1 X2 ... Xn [name1 name2 ... namen] [body] let -> result *)

(* Without let - confusing stack juggling *)
(* Calculate (a + b) * (a - b) for a=10, b=20 *)
10 20 dup2 + rolldown - *.    (* -> -300 *)

(* With let - clear and direct *)
10 20 [a b] [a b + a b - *] let.    (* -> -300 *)

(* Point distance: sqrt((x2-x1)^2 + (y2-y1)^2) *)
0 0 3 4 [x1 y1 x2 y2] [
    x2 x1 - dup *
    y2 y1 - dup * + sqrt
] let.    (* -> 5.0 *)

(* Using let in definitions *)
DEFINE pyth == [a b] [a a * b b * + sqrt] let.
3 4 pyth.    (* -> 5.0 *)
```

**Note:** Names must be user symbols, not builtins (`i` and `x` are reserved; other letters work fine).

## Pattern Matching

Pattern matching enables destructuring and dispatch based on value structure:

### `match` - Single Pattern

```joy
(* Syntax: value [pattern] [action] match -> result | false *)

(* Variable binding *)
5 [n] [n n *] match.           (* -> 25 *)

(* List destructuring with cons pattern *)
[1 2 3] [[h : t]] [h] match.   (* -> 1 *)
[1 2 3] [[h : t]] [t] match.   (* -> [2 3] *)

(* Exact list pattern *)
[1 2 3] [[a b c]] [a b + c +] match.   (* -> 6 *)

(* Wildcard *)
"anything" [_] [42] match.     (* -> 42 *)

(* Literal match *)
5 [5] [true] match.            (* -> true *)
5 [6] [true] match.            (* -> false - no match *)
```

### `cases` - Multi-Pattern Dispatch

```joy
(* Syntax: value [[pat1] [act1]] [[pat2] [act2]] ... cases -> result *)

(* Factorial with pattern matching *)
DEFINE fact ==
    [[[0] [1]]
     [[n] [n 1 - fact n *]]] cases.
5 fact.    (* -> 120 *)

(* Safe head with default *)
DEFINE safehead ==
    [[[[]] [0]]
     [[[h : _]] [h]]] cases.
[] safehead.       (* -> 0 *)
[5 6 7] safehead.  (* -> 5 *)

(* List length *)
DEFINE len ==
    [[[[]] [0]]
     [[[_ : t]] [t len 1 +]]] cases.
[1 2 3 4] len.    (* -> 4 *)
```

### Pattern Syntax

| Pattern | Matches | Binds |
|---------|---------|-------|
| `0`, `"hi"`, `true` | Literal value | nothing |
| `_` | Anything (wildcard) | nothing |
| `n` | Anything | `n` = matched value |
| `[h : t]` | Non-empty list | `h` = head, `t` = tail |
| `[a b c]` | Exactly 3 elements | `a`, `b`, `c` = elements |
| `[]` | Empty list | nothing |

**Note:** The `:` in cons patterns follows Haskell convention and disambiguates `[h : t]` (1+ elements) from `[h t]` (exactly 2 elements).

## Dictionaries

Dictionaries provide key-value data structures with O(1) average lookup:

```joy
(* Create and populate a dictionary *)
dempty "name" "Alice" dput "age" 30 dput.
(* -> {age: 30, name: "Alice"} *)

(* Create from association list *)
[["name" "Bob"] ["score" 100]] >dict.

(* Access operations *)
mydict "name" dget.         (* -> "Bob" *)
mydict "missing" 0 dgetd.   (* -> 0 - default value *)
mydict "name" dhas.         (* -> true *)

(* Introspection *)
mydict dkeys.               (* -> ["name" "score"] *)
mydict dvals.               (* -> ["Bob" 100] *)
mydict dsize.               (* -> 2 *)

(* Modification *)
mydict "score" ddel.        (* Remove key *)
dict1 dict2 dmerge.         (* Merge dictionaries *)
mydict dict>.               (* -> [["name" "Bob"] ["score" 100]] *)
```

### Dictionary Operators

| Operator | Signature | Description |
|----------|-----------|-------------|
| `dempty` | `-> dict` | Create empty dictionary |
| `dput` | `dict "key" value -> dict'` | Insert or update key |
| `dget` | `dict "key" -> value` | Get value (error if missing) |
| `dgetd` | `dict "key" default -> value` | Get with default |
| `dhas` | `dict "key" -> bool` | Check if key exists |
| `ddel` | `dict "key" -> dict'` | Delete key |
| `dkeys` | `dict -> [keys...]` | Get all keys |
| `dvals` | `dict -> [values...]` | Get all values |
| `dsize` | `dict -> int` | Number of entries |
| `>dict` | `[[k v]...] -> dict` | Create from assoc list |
| `dict>` | `dict -> [[k v]...]` | Convert to assoc list |
| `dmerge` | `dict1 dict2 -> dict'` | Merge (dict2 overwrites) |

## JSON Support

Parse and emit JSON with automatic type mapping:

```joy
(* Parse JSON to Joy values *)
"{\"name\": \"Alice\", \"scores\": [95, 87]}" json>.
(* -> dict with name="Alice", scores=[95 87] *)

"[1, 2, 3]" json>.           (* -> [1 2 3] *)
"true" json>.                (* -> true *)
"null" json>.                (* -> null *)

(* Emit Joy values as JSON *)
mydict >json.
(* -> "{\"name\":\"Alice\",\"scores\":[95,87]}" *)

[1 2 3] >json.               (* -> "[1,2,3]" *)
true >json.                  (* -> "true" *)
```

### Type Mapping

| JSON | Joy |
|------|-----|
| object | dictionary (DICT_) |
| array | list |
| string | string |
| number (int) | integer |
| number (float) | float |
| true/false | true/false |
| null | null |

## String Interpolation

Embed expressions in strings with `$"..."` syntax:

```joy
(* Basic interpolation *)
$"2+2=${2 2 +}".             (* -> "2+2=4" *)
$"pi=${3.14159}".            (* -> "pi=3.14159" *)

(* With defined symbols *)
world == "world".
$"Hello ${world}!".          (* -> "Hello world!" *)

(* Multiple interpolations *)
$"a=${1}, b=${2}".           (* -> "a=1, b=2" *)

(* Expressions at any position *)
$"${1}+${2}=${1 2 +}".       (* -> "1+2=3" *)

(* Literal $ preserved when not followed by { *)
$"Price: $100".              (* -> "Price: $100" *)
```

Expressions in `${...}` are evaluated and converted to strings using `unquoted`. Supports integers, floats, strings, booleans, and user-defined symbols.

## Persistent Sessions

Store and restore symbol definitions across Joy sessions using SQLite:

```joy
(* Open or create a persistent session *)
"myproject" session.
(* -> session: opened 'myproject' *)

(* Define symbols - automatically persisted *)
DEFINE square == dup *.
DEFINE cube == dup dup * *.

(* Quit and restart Joy - definitions survive *)
"myproject" session.
5 square.    (* -> 25 - definition was persisted *)
```

### Snapshots

Save and restore session state to named checkpoints:

```joy
"myproject" session.
DEFINE x == 1.
"v1" snapshot.        (* Save current state *)

DEFINE x == 999.      (* Change x *)
x.                    (* -> 999 *)

"v1" restore.         (* Restore to v1 *)
x.                    (* -> 1 - restored! *)

snapshots.            (* -> ["v1"] - list snapshots *)
```

### Session Management

```joy
(* List available sessions *)
sessions.             (* -> ["myproject" "other"] *)

(* Compare sessions *)
"other" session-diff.
(* -> {added: [...], modified: [...], removed: [...]} *)

(* Merge another session into current *)
"library" session-merge.
(* -> true if no conflicts, false with conflict list *)

(* Copy specific symbol from another session *)
"library" "useful-func" session-take.
```

### SQL Queries

Execute SQL directly on session data:

```joy
(* Query all symbols *)
"SELECT name, body FROM symbols ORDER BY name" [] sql.
(* -> [{name: "cube", body: "dup dup * *"}, ...] *)

(* Query with parameters *)
"SELECT * FROM symbols WHERE name LIKE ?" ["%square%"] sql.
```

### Session Operators

| Operator | Signature | Description |
|----------|-----------|-------------|
| `session` | `"name" ->` | Open/create persistent session |
| `session-close` | `->` | Close current session |
| `sessions` | `-> [names]` | List available sessions |
| `snapshot` | `"name" ->` | Save current state |
| `restore` | `"name" ->` | Restore to snapshot |
| `snapshots` | `-> [names]` | List snapshots in session |
| `session-merge` | `"source" -> B` | Merge source into current |
| `session-diff` | `"other" -> D` | Compare sessions (dict) |
| `session-take` | `"source" "symbol" ->` | Copy symbol from source |
| `sql` | `"query" [params] -> [results]` | Execute SQL on session |

**Note:** Requires building with `-DJOY_SESSION=ON` (requires SQLite3).

### String Conversion Operators

| Operator | Signature | Description |
|----------|-----------|-------------|
| `toString` | `value -> "string"` | Convert to quoted string representation |
| `unquoted` | `value -> "string"` | Convert to string (strings unquoted) |

### Performance

Vector and matrix operations use SIMD vectorization via `#pragma omp simd` directives, enabling automatic use of SSE/AVX instructions on modern CPUs. This is enabled by default with the `-fopenmp-simd` compiler flag.

For larger matrices (32x32 and above), optional BLAS support provides additional performance gains via highly-optimized vendor libraries (Apple Accelerate, OpenBLAS, Intel MKL).

See [doc/vector_impl.md](doc/vector_impl.md) for implementation details.

## Building

### Standard Build (without parallel support)

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Build with BLAS Support

For optimized matrix operations on larger matrices:
- **macOS**: Uses Apple Accelerate (built-in, no extra install needed)
- **Linux**: `apt install libopenblas-dev` or similar

```bash
mkdir build && cd build
cmake -DJOY_BLAS=ON ..
cmake --build .
```

### Build with Parallel Support

Requires OpenMP:
- **macOS**: `brew install libomp`
- **Linux**: OpenMP is typically built into GCC/Clang

```bash
mkdir build && cd build
cmake -DJOY_PARALLEL=ON ..
cmake --build .
```

### Build with Persistent Sessions

Requires SQLite3:
- **macOS**: Built-in (no extra install needed)
- **Linux**: `apt install libsqlite3-dev`

```bash
mkdir build && cd build
cmake -DJOY_SESSION=ON ..
cmake --build .
```

### Build with Debug/Tests

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
ctest  # Run all tests
```

### Windows (MSVC)

```bash
cmake --build . --config Release
copy Release\joy.exe
```

## Installation

```bash
cp ../lib/usrlib.joy ~
mkdir ~/usrlib
cp ../lib/* ~/usrlib
```

Then edit `~/usrlib.joy` to change the path from `"../lib"` to `"usrlib"`.

## Usage

```bash
joy -h              # Show options
joy program.joy     # Run a program
joy                 # Interactive REPL
```

### Example Session

```
$ ./joy
JOY - compiled ...
-> [1 2 3 4 5] [dup *] pmap.
[1 4 9 16 25]
-> 10 [2 *] [3 +] pfork + .
33
-> quit.
```

## Testing

```bash
cd build
ctest                    # Run all tests
ctest -R parallel        # Run parallel tests only
```

Or manually:
```bash
./joy tests/parallel_test.joy
./joy tests/parallel_benchmark.joy
./joy tests/test2/match.joy      # Pattern matching tests
./joy tests/test2/cases.joy      # Multi-pattern dispatch tests
./joy tests/test2/let.joy        # Local bindings tests
./joy tests/test2/dict.joy       # Dictionary tests
./joy tests/test2/json.joy       # JSON tests
./joy tests/test2/strinterp.joy  # String interpolation tests
./joy tests/test2/vector_native.joy  # Native vector/matrix tests
# Session tests require -DJOY_SESSION=ON build
```

## Architecture

The parallel implementation is built on five phases of GC context isolation:

1. **Phase 1**: NOBDW copying GC per-context state
2. **Phase 2**: Conservative GC context-aware API
3. **Phase 3**: Per-Joy-context GC integration
4. **Phase 4**: Parallel combinator infrastructure
5. **Phase 5**: OpenMP parallel execution

Each parallel task executes with:
- Cloned environment with isolated GC context
- Deep-copied input values and quotations
- Independent error handling
- Results copied back to parent context

See [doc/parallel.md](doc/parallel.md) for detailed design documentation.

## Documentation

| Resource | Description |
|----------|-------------|
| [doc/parallel.md](doc/parallel.md) | Parallel execution user guide and examples |
| [doc/parallel_performance.md](doc/parallel_performance.md) | Benchmark results and performance guide |
| [doc/parallel_fixes.md](doc/parallel_fixes.md) | Technical documentation of parallel fixes |
| [doc/vector.md](doc/vector.md) | Vector operations design document |
| [doc/vector_impl.md](doc/vector_impl.md) | Vector operations implementation guide |
| [doc/persistent_session_design.md](doc/persistent_session_design.md) | Persistent sessions design document |
| [CHANGELOG.md](CHANGELOG.md) | Version history and changes |
| [Legacy Docs](https://wodan58.github.io) | Original Joy documentation |
| [User Manual](https://wodan58.github.io/j09imp.html) | Joy language reference |
| [Main Manual (PDF)](https://github.com/Wodan58/G3/blob/master/JOP.pdf) | Comprehensive Joy manual |

## Related Implementations

| Implementation | Dependencies |
|----------------|--------------|
| [42minjoy](https://github.com/Wodan58/42minjoy) | Minimal Joy |
| [joy0](https://github.com/Wodan58/joy0) | Original Joy |
| [joy1](https://github.com/Wodan58/joy1) | [BDW garbage collector](https://github.com/ivmai/bdwgc) |
| [Foy](https://github.com/Wodan58/Foy) | [BDW garbage collector](https://github.com/ivmai/bdwgc) |
| [Moy](https://github.com/Wodan58/Moy) | [BDW garbage collector](https://github.com/ivmai/bdwgc) and [Lex & Yacc](https://sourceforge.net/projects/winflexbison/files/win_flex_bison-latest.zip) |

## History

Joy was created by [Manfred von Thun](http://fogus.me/important/von-thun/). According to the [bibliography](https://wodan58.github.io/joybibl.html), the first papers about Joy date back to 1994, and the C implementation was started in 1995 per this [interview](http://archive.vector.org.uk/art10000350).

## License

See the original Joy distribution for license terms.
