# TODO: Parallel Execution Improvements

## Investigate Combinators for Parallel Compatibility

The following combinators may have issues when used inside `pmap` or `pfork` quotations. The root cause is that recursive/iterative combinators may reference nodes in parent memory that aren't properly copied to the child execution context.

### Known Issues

- [ ] **`times`** - Crashes or hangs when used in parallel quotations
  - Example: `[1 2 3 4 5 6 7 8] [1000 [dup * 2 mod] times] pmap` fails
  - Likely cause: The `times` combinator's internal recursion references parent memory

### Combinators to Investigate

#### Iterative Combinators
- [ ] `times` - Execute P N times
- [ ] `while` - While B yields true, execute D
- [ ] `step` - Execute P for each member of aggregate

#### Recursive Combinators
- [ ] `linrec` - Linear recursion
- [ ] `tailrec` - Tail recursion
- [ ] `binrec` - Binary recursion
- [ ] `genrec` - General recursion
- [ ] `primrec` - Primitive recursion
- [ ] `condlinrec` - Conditional linear recursion
- [ ] `condnestrec` - Conditional nested recursion

#### Tree Combinators
- [ ] `treestep` - Tree traversal
- [ ] `treerec` - Tree recursion
- [ ] `treegenrec` - General tree recursion

### Technical Background

The parallel execution model clones the environment for each parallel task:
- Child environments have their own `memory` array (NOBDW mode)
- Symbol tables are shared (read-only)
- Symbol bodies point to nodes in the parent's memory

The fix in `interp.c` (`copy_body_from_parent`) handles direct symbol execution, but nested combinators may bypass this path and access parent memory indices directly.

### Potential Solutions

1. **Eager expansion**: Expand all user-defined symbols in quotations before parallel execution
2. **Memory sharing**: Use a read-only reference to parent memory for all node access (complex)
3. **Combinator-specific fixes**: Modify each combinator to handle parallel context
4. **Documentation**: Document which combinators are safe for parallel use

### Test Cases Needed

```joy
(* Test each combinator in parallel context *)
[1 2 3 4] [10 [dup +] times] pmap.           (* times *)
[1 2 3 4] [[0 >] [1 -] while] pmap.          (* while *)
[1 2 3 4] [[0 =] [1] [dup 1 -] [*] linrec] pmap.  (* linrec - factorial *)
```

### Related Files

- `src/interp.c` - Main interpreter, `copy_body_from_parent` function
- `include/parallel.h` - Parallel execution infrastructure
- `include/globals.h` - Env structure with `parent_memory` field
- `src/builtin/pmap.c` - Parallel map implementation
- `src/builtin/pfork.c` - Parallel fork implementation
