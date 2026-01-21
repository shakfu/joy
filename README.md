# Joy

A parallel-capable implementation of the Joy programming language.

This implementation is a friendly fork of Ruurd Wiersma's [Joy implementation](https://github.com/Wodan58/Joy).

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

### Performance

Benchmark with 8 elements and heavy computation:

| Mode | Time | CPU Usage | Speedup |
|------|------|-----------|---------|
| `pmap` (parallel) | 0.023s | 448% | **2.8x faster** |
| `map` (sequential) | 0.065s | 97% | baseline |

The 448% CPU usage confirms multiple cores are being utilized.

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
ctest  # Run all 178 tests
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

See [PARALLEL.md](PARALLEL.md) for detailed design documentation.

## Documentation

| Resource | Description |
|----------|-------------|
| [PARALLEL.md](PARALLEL.md) | Parallel execution design and implementation |
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
