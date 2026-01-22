# Parallel Execution Performance Guide

This document describes when to use `pmap` vs `map` for optimal performance.

## Summary

| Work per Element | Recommendation | Speedup |
|------------------|----------------|---------|
| Light (simple ops) | Use `map` | pmap is slower |
| Medium (~1000 iterations) | Use `map` | Similar or pmap slower |
| Heavy (~100000 iterations) | Use `pmap` | ~20-25% faster |
| Very Heavy (~1000000 iterations) | Use `pmap` | ~35-40% faster |

## Benchmark Results

Tested on macOS with OMP_NUM_THREADS=8:

```
--- Light Work (dup *) ---
4 elements : map=0.003s pmap=0.004s
8 elements : map=0.003s pmap=0.004s

--- Medium Work (1000 iterations) ---
4 elements : map=0.003s pmap=0.004s
8 elements : map=0.004s pmap=0.004s

--- Heavy Work (100000 iterations) ---
4 elements : map=0.015s pmap=0.013s  (pmap 13% faster)
8 elements : map=0.026s pmap=0.020s  (pmap 23% faster)

--- Very Heavy Work (1000000 iterations) ---
4 elements : map=0.133s pmap=0.086s  (pmap 35% faster)
8 elements : map=0.247s pmap=0.155s  (pmap 37% faster)

--- Recursive: Sum 1..N (linrec) ---
sum to 5000 : map=0.005s pmap=0.005s
sum to 10000: map=0.007s pmap=0.005s  (pmap 29% faster)
```

## When to Use pmap

**Use `pmap` when:**
- Each element requires substantial computation (>10,000 operations)
- Processing recursive algorithms with deep recursion
- Work per element takes more than ~10ms
- You have multiple CPU cores available

**Use `map` when:**
- Work per element is trivial (simple arithmetic)
- Processing very short lists (< 4 elements)
- Memory is constrained (pmap clones environments)
- Deterministic ordering is critical

## Why pmap Has Overhead

`pmap` incurs costs that `map` doesn't:

1. **Thread creation/management** - OpenMP spawns worker threads
2. **Environment cloning** - Each parallel task gets its own Joy context
3. **Memory copying** - Input data copied to child contexts, results copied back
4. **GC isolation** - Each context has independent garbage collection

For light work, this overhead dominates. For heavy work, parallelism wins.

## Crossover Point

The crossover point where `pmap` becomes faster depends on:

- Number of CPU cores (more cores = lower crossover)
- Work complexity (more work = lower crossover)
- List size (more elements = better parallelism)

**Rule of thumb:** If each element takes >10ms to process, use `pmap`.

## Running Benchmarks

```bash
# Run the shell benchmark (measures wall time)
OMP_NUM_THREADS=8 bash tests/parallel_benchmark.sh

# Run the Joy benchmark (measures CPU ticks)
./build/joy tests/parallel_benchmark.joy
```

## Configuration

Control thread count with environment variable:
```bash
export OMP_NUM_THREADS=4  # Use 4 threads
```

Default is typically the number of CPU cores.
