# Calling C functions

`ffi-call` and `ffi` call C functions in shared libraries, through libffi.

Build with `-DJOY_FFI=ON`. Install libffi first: `brew install libffi` or `apt install libffi-dev`. Without it, both builtins raise an error. MSVC builds are not supported.

## Usage

```joy
(* args  library  symbol  [argument types]  [return type] *)
0.0  "libm.so.6" "sin" [double] [double] ffi-call.   (* -> 0.0 *)

(* An empty library name searches the program and what it has loaded *)
"abc" "" "strlen" [string] [size_t] ffi-call.        (* -> 3 *)

(* Name a C function like any other word *)
DEFINE csin == "" "sin" [double] [double] ffi-call.
[0.0 1.0] [csin] map.

(* ffi resolves now, so a bad name fails here, and gives a quotation *)
"" "sin" [double] [double] ffi.   (* -> ["" "sin" [double] [double] ffi-call] *)
```

The last C argument is on top of the stack. A `[]` return type means void, and pushes nothing. Library file names differ by platform: `libm.so.6` on Linux, `libm.dylib` on macOS.

## Types

| Type | Argument | Result |
|------|----------|--------|
| `int` `uint` `long` `ulong` `int64` `uint64` `size_t` | integer in range of the C type | integer |
| `char` | character | character |
| `bool` | logical | logical |
| `float` `double` | float or integer | float |
| `string` | string, passed as a temporary copy | string, copied |
| `pointer` | integer; 0 is NULL | integer |

These raise an error, and the first two do so before the call:

- A wrong argument type or count.
- An integer out of range of its C type.
- A NULL `string` result. Use `pointer` where NULL is expected.
- A `uint64`, `ulong` or `size_t` result above `2^63 - 1`.

## Rules for foreign code

Joy cannot check the signature against the C function. A wrong signature, a bad pointer or a crash in the function ends the process.

- Do not declare a variadic function such as `printf`. Apple arm64 passes variadic arguments differently, so a fixed signature passes garbage.
- A `string` argument is freed after the call. A function must not keep the pointer.
- A `string` result is not freed. A function that returns allocated memory leaks unless it is declared `pointer` and freed with `"" "free" [pointer] [] ffi-call`.
- Libraries stay loaded until the process ends.

Calling a library has the same trust level as `system`: only run programs you trust.

## Not supported

- Callbacks. Re-entering the interpreter during a call could run a GC and move nodes. Without callbacks, no node moves during a call.
- Structs by value, varargs, and reading or writing memory through a `pointer`.
- More than 16 arguments.

## Other features

- **Parallel combinators**: a quotation with an FFI call runs sequentially.
- **Sessions**: an FFI word persists as its quotation and resolves again on first call.
- **`compile-to-c`**: works when the program links libffi.
