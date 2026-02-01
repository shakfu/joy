# Joy Debugger and Stepper

This document describes Joy's interactive debugging facilities for tracing execution and stepping through programs.

## Overview

Joy provides three debugging modes:

1. **Tracing** - View execution step-by-step without interaction
2. **Stepping** - Interactive single-step execution with user control
3. **Breakpoints** - Pause execution when specific symbols are encountered

## Quick Start

```joy
(* Trace execution of a quotation *)
[1 2 + 3 *] debug-trace.
(*
 : 1 2 + 3 *
1 : 2 + 3 *
1 2 : + 3 *
3 : 3 *
3 3 : *
9
*)

(* Interactive stepping *)
[1 2 + 3 *] debug-step.
(* Prompts for each step: [s]tep [c]ontinue [q]uit> *)
```

## Tracing with debug-trace

The `debug-trace` combinator executes a quotation while displaying the stack and pending operations at each step.

### Syntax

```
[P] debug-trace  ->  ...
```

Executes quotation P with full execution tracing enabled.

### Output Format

Each line shows:
```
<stack contents> : <remaining program>
```

The stack is displayed left-to-right (bottom to top), and the program shows what will be executed next.

### Examples

```joy
(* Simple arithmetic *)
[2 3 + 4 *] debug-trace.
(*
 : 2 3 + 4 *
2 : 3 + 4 *
2 3 : + 4 *
5 : 4 *
5 4 : *
20
*)

(* With user-defined functions *)
double == 2 *.
[3 double 1 +] debug-trace.
(*
 : 3 double 1 +
3 : double 1 +
3 : 2 * 1 +
3 2 : * 1 +
6 : 1 +
6 1 : +
7
*)

(* Nested quotations *)
[[1 2] [3 4] concat] debug-trace.
(*
 : [1 2] [3 4] concat
[1 2] : [3 4] concat
[1 2] [3 4] : concat
[1 2 3 4]
*)
```

### Use Cases

- Understanding how a Joy program executes
- Debugging unexpected results
- Learning Joy's evaluation model
- Documenting execution for teaching

## Interactive Stepping with debug-step

The `debug-step` combinator provides interactive control over execution, pausing before each operation.

### Syntax

```
[P] debug-step  ->  ...
```

Executes quotation P in interactive stepping mode.

### Commands

At each step, the debugger prompts for a command:

| Key | Action |
|-----|--------|
| `s` or Enter | **Step** - Execute one operation and pause |
| `c` | **Continue** - Run until next breakpoint or completion |
| `q` | **Quit** - Abort execution immediately |

### Example Session

```
$ joy
[1 2 + 3 *] debug-step
 : 1 2 + 3 *
[s]tep [c]ontinue [q]uit> s
1 : 2 + 3 *
[s]tep [c]ontinue [q]uit> s
1 2 : + 3 *
[s]tep [c]ontinue [q]uit> s
3 : 3 *
[s]tep [c]ontinue [q]uit> c
9
```

### Use Cases

- Examining stack state at specific points
- Understanding control flow in conditionals
- Debugging recursive functions
- Interactive exploration of unfamiliar code

## Breakpoints

Breakpoints allow you to run code normally until a specific symbol is about to execute.

### Setting Breakpoints

```
"name" breakpoint  ->
```

Sets a breakpoint on the named symbol. When that symbol is about to execute, the debugger pauses and enters stepping mode.

```joy
(* Set breakpoint on user-defined symbol *)
double == 2 *.
"double" breakpoint

(* Now any execution that calls double will pause *)
[3 double 1 +] i
(*
 : double 1 +
[break: double]
[s]tep [c]ontinue [q]uit>
*)
```

### Managing Breakpoints

```joy
(* Show all breakpoints *)
show-breakpoints.          (* -> ["double" "foo" ...] *)

(* Clear all breakpoints *)
clear-breakpoints.

(* Verify cleared *)
show-breakpoints.          (* -> [] *)
```

### Breakpoint Behavior

When a breakpoint is hit:

1. The current stack and pending program are displayed
2. A `[break: name]` message identifies which breakpoint triggered
3. The debugger enters stepping mode automatically
4. Use `s` to step, `c` to continue to next breakpoint, or `q` to quit

### Example: Debugging a Recursive Function

```joy
(* Factorial with breakpoint *)
fact == [0 =] [pop 1] [dup 1 - fact *] ifte.
"fact" breakpoint

5 fact.
(*
5 : fact
[break: fact]
[s]tep [c]ontinue [q]uit> s
5 : [0 =] [pop 1] [dup 1 - fact *] ifte
...
[s]tep [c]ontinue [q]uit> c
4 : fact
[break: fact]
[s]tep [c]ontinue [q]uit> c
...
120
*)
```

## Combining Debugging Tools

### Trace with Breakpoints

Breakpoints work with normal execution. To trace while using breakpoints:

```joy
"myfunction" breakpoint
[... code that calls myfunction ...] debug-trace.
```

When `myfunction` is encountered during tracing, execution will pause for interactive control.

### Debugging Complex Programs

```joy
(* Define functions *)
process == [filter] dip map.
transform == dup reverse concat.

(* Set strategic breakpoints *)
"process" breakpoint
"transform" breakpoint

(* Run with tracing to see everything *)
[[1 2 3 4 5] [2 >] [transform]] debug-trace.
```

## Reference

### Debugger Builtins

| Name | Stack Effect | Description |
|------|--------------|-------------|
| `debug-trace` | `[P] -> ...` | Execute P with full execution tracing |
| `debug-step` | `[P] -> ...` | Execute P with interactive stepping |
| `breakpoint` | `"name" ->` | Set breakpoint on named symbol |
| `clear-breakpoints` | `->` | Remove all breakpoints |
| `show-breakpoints` | `-> L` | Push list of breakpoint names |

### Tips

1. **Start simple** - Use `debug-trace` first to understand execution flow
2. **Set focused breakpoints** - Target specific functions rather than stepping through everything
3. **Use continue wisely** - Press `c` to skip past understood code
4. **Quit early** - Press `q` if you've found the issue

### Limitations

- Stepping through builtins shows only their invocation, not internal implementation
- Breakpoints work on user-defined symbols and some builtins
- Interactive stepping requires a terminal (not available in pipes/scripts)
- Step-over (skipping into function calls) is not yet implemented

## Implementation Notes

The debugger state is stored in `EnvConfig`:

```c
unsigned char stepping;       /* 0=off, 1=step-into, 3=continue */
int step_depth;               /* for future step-over support */
vector(int)* breakpoints;     /* symbol table indices */
```

The interpreter checks for breakpoints and stepping mode in `exec_term()` before executing each term. When stepping is active, `debugger_prompt()` handles user input.
