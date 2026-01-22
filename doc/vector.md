# Vectors and Matrices in Joy

This document explores how vector/matrix operations and vectorization could fit into Joy's functional concatenative paradigm.

## Why Vectors/Matrices Fit Joy

There's a natural alignment between Joy's paradigm and array programming:

1. **Concatenative composition** - chaining transformations like `normalize transpose scale`
2. **Stack-based parameter passing** - matrices and vectors flow through naturally
3. **Higher-order combinators** - `map`, `fold` generalize to array operations

## Design Principle: No New Syntax

Rather than introducing new bracket syntax (like `#(1 2 3)`), vectors use the existing list syntax `[1 2 3]`. Vectorization is achieved through:

1. **Vectorized operators** - new operators that work on numeric lists
2. **Internal optimization** - lists of numbers stored contiguously when beneficial
3. **Transparent behavior** - vectors are lists, lists of numbers can be vectors

This keeps Joy simple while enabling high-performance numeric operations.

## Approaches

### Approach A: Explicit vectorized operators

Add `v`-prefixed operators that expect numeric lists:

```joy
[1 2 3] [4 5 6] v+.      (* -> [5 7 9] element-wise add *)
[1 2 3] [4 5 6] v*.      (* -> [4 10 18] element-wise multiply *)
[1 2 3] 2 vscale.        (* -> [2 4 6] scalar multiply *)
[1 2 3] [4 5 6] dot.     (* -> 32 dot product *)
```

**Pros**: Explicit, no magic, backward compatible
**Cons**: More operators to remember

### Approach B: Automatic vectorization (APL-style)

Standard operators detect numeric lists and vectorize automatically:

```joy
[1 2 3] [4 5 6] +.       (* -> [5 7 9] detects lists, vectorizes *)
[1 2 3] 10 +.            (* -> [11 12 13] scalar broadcast *)
[1 2 3] 10 *.            (* -> [10 20 30] scalar broadcast *)
[[1 2] [3 4]] 10 +.      (* -> [[11 12] [13 14]] deep broadcast *)
```

**Pros**: Minimal syntax, powerful
**Cons**: Changes existing semantics, potential surprises

### Approach C: Internal optimization only

Keep existing syntax and semantics, but optimize internally:

```joy
[1 2 3] [4 5 6] [+] map2.   (* same syntax, SIMD internally *)
[1 2 3] 0 [+] fold.         (* same syntax, vectorized sum *)
```

**Pros**: Fully backward compatible
**Cons**: Limited optimization opportunities, still need map2/fold

### Recommended: Approach A with optional B

Start with explicit vectorized operators (A), optionally add automatic vectorization (B) later behind a flag or for specific operators.

## Proposed Vector/Matrix Vocabulary

### Vectors (1D)

```joy
(* Element-wise operations *)
[1 2 3] [4 5 6] v+.          (* -> [5 7 9] *)
[1 2 3] [4 5 6] v-.          (* -> [-3 -3 -3] *)
[1 2 3] [4 5 6] v*.          (* -> [4 10 18] *)
[1 2 3] [4 5 6] v/.          (* -> [0.25 0.4 0.5] *)

(* Scalar operations *)
[1 2 3] 10 vscale.           (* -> [10 20 30] *)
[1 2 3] 10 v+s.              (* -> [11 12 13] scalar add *)

(* Linear algebra *)
[1 2 3] [4 5 6] dot.         (* -> 32 dot product *)
[1 2 3] vnorm.               (* -> 3.74... magnitude *)
[1 2 3] vnormalize.          (* -> [0.27 0.53 0.80] unit vector *)
[1 0 0] [0 1 0] cross.       (* -> [0 0 1] cross product *)

(* Reductions *)
[1 2 3 4 5] vsum.            (* -> 15 *)
[1 2 3 4 5] vprod.           (* -> 120 *)
[1 2 3 4 5] vmin.            (* -> 1 *)
[1 2 3 4 5] vmax.            (* -> 5 *)
[1 2 3 4 5] vmean.           (* -> 3 *)

(* Creation *)
5 vzeros.                    (* -> [0 0 0 0 0] *)
5 vones.                     (* -> [1 1 1 1 1] *)
1 5 vrange.                  (* -> [1 2 3 4 5] *)
0 1 5 vlinspace.             (* -> [0 0.25 0.5 0.75 1] *)
```

### Matrices (2D - lists of lists)

```joy
(* Element-wise operations *)
[[1 2] [3 4]] [[5 6] [7 8]] m+.    (* -> [[6 8] [10 12]] *)
[[1 2] [3 4]] [[5 6] [7 8]] m*.    (* -> [[5 12] [21 32]] *)
[[1 2] [3 4]] 10 mscale.           (* -> [[10 20] [30 40]] *)

(* Matrix operations *)
[[1 2] [3 4]] [[5 6] [7 8]] mm.    (* -> [[19 22] [43 50]] matmul *)
[[1 2] [3 4]] [5 6] mv.            (* -> [17 39] matrix-vector *)
[[1 2] [3 4]] transpose.           (* -> [[1 3] [2 4]] *)

(* Linear algebra *)
[[1 2] [3 4]] det.                 (* -> -2 determinant *)
[[1 2] [3 4]] inv.                 (* -> [[-2 1] [1.5 -0.5]] inverse *)
[[1 2] [3 4]] trace.               (* -> 5 *)
3 meye.                            (* -> [[1 0 0] [0 1 0] [0 0 1]] *)

(* Reductions *)
[[1 2] [3 4]] msum.                (* -> 10 total sum *)
[[1 2] [3 4]] 0 msuma.             (* -> [4 6] sum along axis 0 *)
[[1 2] [3 4]] 1 msuma.             (* -> [3 7] sum along axis 1 *)

(* Creation *)
2 3 mzeros.                        (* -> [[0 0 0] [0 0 0]] *)
3 meye.                            (* -> 3x3 identity *)
```

### Mapping and combining

```joy
(* Apply function to each element *)
[1 2 3 4] [dup *] vmap.            (* -> [1 4 9 16] *)
[[1 2] [3 4]] [dup *] mmap.        (* -> [[1 4] [9 16]] *)

(* Apply function to rows/columns *)
[[1 2] [3 4]] [vsum] map.          (* -> [3 7] row sums *)
[[1 2] [3 4]] [vsum] mapcols.      (* -> [4 6] column sums *)
```

## Why It Fits Well

### 1. Point-free composition

```joy
(* Normalize, rotate, scale - reads left to right *)
vnormalize rotation-matrix mv 2 vscale
```

### 2. Stack discipline matches linear algebra

```joy
A B mm C mm          (* (A * B) * C - natural order *)
v A mv B mv          (* B * (A * v) - transforms compose *)
```

### 3. Combinators generalize

```joy
matrix [row-op] map           (* apply to each row *)
matrix [col-op] mapcols       (* apply to each column *)
matrices [mm] fold            (* chain multiply *)
```

### 4. Parallelism is natural

```joy
(* Process matrix rows in parallel *)
large-matrix [expensive-row-op] pmap

(* Element-wise operations parallelize trivially *)
[...million elements...] [...million elements...] v+.
```

## Implementation Considerations

### Internal representation

Lists of numbers could be represented two ways:

1. **Linked list** (current) - standard Joy nodes
2. **Contiguous array** - cache-friendly, SIMD-ready

The interpreter could:
- Detect homogeneous numeric lists
- Convert to contiguous storage lazily
- Fall back to linked list for mixed types

### Type checking

Vectorized operators should provide clear errors:

```joy
[1 2 3] ["a" "b"] v+.
(* Error: v+ requires numeric lists *)

[1 2 3] [4 5] v+.
(* Error: v+ requires lists of equal length *)
```

### SIMD optimization

With contiguous storage, operations can use SIMD:

```c
// Internal implementation of v+
void vadd(double* a, double* b, double* result, size_t n) {
    #pragma omp simd
    for (size_t i = 0; i < n; i++) {
        result[i] = a[i] + b[i];
    }
}
```

### Backend options

For heavy linear algebra, could optionally link:
- **BLAS/LAPACK** - industry standard, highly optimized
- **OpenBLAS** - open source BLAS
- **Apple Accelerate** - macOS optimized

## Related Languages

| Language | Approach |
|----------|----------|
| **APL/J** | Automatic vectorization, rank polymorphism |
| **NumPy** | Explicit array type, broadcasting |
| **Factor** | `math.vectors` vocabulary |
| **MATLAB** | Matrix as fundamental type |

## Getting Started

A minimal implementation:

1. Add vectorized operators: `v+`, `v-`, `v*`, `vscale`, `dot`, `vsum`
2. Implement with simple loops initially (no SIMD)
3. Add dimension checking with clear error messages
4. Test with existing parallel infrastructure (`pmap`)

Later enhancements:
- Contiguous storage optimization
- SIMD vectorization
- BLAS integration for matrix operations
