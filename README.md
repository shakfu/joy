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

All operations include error handling for type mismatches and dimension errors.

### Performance

Vector and matrix operations use SIMD vectorization via `#pragma omp simd` directives, enabling automatic use of SSE/AVX instructions on modern CPUs. This is enabled by default with the `-fopenmp-simd` compiler flag and works independently of the full parallel execution feature (`-DJOY_PARALLEL`).

See [doc/vector_impl.md](doc/vector_impl.md) for implementation details.

## Building

### Standard Build (without parallel support)

```bash
mkdir build && cd build
cmake ..
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

### Build with Debug/Tests

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
ctest  # Run all 182 tests
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
