# Persistent Session Implementation Notes

This document captures the differences between the design (`doc/persistent_session_design.md`) and the current implementation.

## Implementation Status

### Fully Implemented

| Feature | Description |
|---------|-------------|
| `session` | Opens/creates persistent session |
| `session-close` | Closes current session |
| `sessions` | Lists available sessions (*.joy.db files) |
| `snapshot` | Saves current state to named checkpoint |
| `restore` | Restores state from named checkpoint |
| `snapshots` | Lists available snapshots |
| `session-merge` | Merges source session into current |
| `session-diff` | Compares sessions, returns dict with added/modified/removed |
| `session-take` | Copies specific symbol from another session |
| `sql` | Executes SQL query on session data |
| SQLite WAL mode | Better concurrency |
| Write-through on DEFINE | Automatic persistence |
| Session struct with cache | In-memory caching |
| Prepared statements | Performance optimization |
| `[SESSION]` marker | Conditional compilation in generators |

### Schema Implementation

```sql
-- Implemented tables
CREATE TABLE symbols (name, body, type, size, modified_at, created_at);
CREATE TABLE snapshots (name, symbol_name, body, created_at);
CREATE TABLE meta (key, value);
CREATE INDEX idx_snapshots_name ON snapshots(name);
```

## Not Implemented (Deviations from Design)

### 1. `session-keep` Operator

**Design:** `"helper" session-keep.` to resolve conflicts by keeping current version during merge.

**Status:** Not implemented. Users must manually resolve conflicts using `session-take` or by not merging.

**Priority:** Low - workaround exists.

### 2. Lazy Loading / Chunked Storage

**Design:** Large structures stored in chunks, loaded on demand:
- `heap` table for chunked storage
- `@chunked` marker in symbols table
- `CHUNK_THRESHOLD` (4096 bytes)
- `CHUNK_SIZE` (100 elements per chunk)
- `LazyList` struct for on-demand loading
- `insert_chunk` / `select_chunks` prepared statements

**Status:** Not implemented. All symbols loaded eagerly at session open.

**Impact:** Large sessions may use excessive memory.

**Priority:** Medium - affects scalability for large sessions.

### 3. Lazy Symbol Lookup

**Design:** Modified `lookup_user_symbol()` to:
1. Check in-memory symbol table
2. On miss, check cache
3. On cache miss, load from SQLite

**Status:** Not implemented. All symbols loaded at session open instead.

**Impact:** Slower session startup for large sessions, but faster symbol access after load.

**Priority:** Medium - tied to lazy loading implementation.

### 4. Full Parser Deserialization

**Design:** Use `readterm()` to deserialize stored Joy expressions, supporting all value types.

**Status:** Simplified deserializer handles only:
- Integers (parsed with `strtoll`)
- Floats (parsed with `strtod`)
- Complex values stored as strings (not parsed)

**Reason:** Using the Joy parser (`readterm`) during session load corrupted the scanner state because the main input stream was still being parsed. The scanner has shared state (line buffer, current symbol, file pointer) that gets overwritten.

**Impact:** Complex values (lists, quotations, dicts) are stored but not properly restored. They come back as strings.

**Priority:** High - core functionality limitation.

## Architecture Differences

### Design: Lazy-Loading Hybrid

```
Session Load:
1. Open SQLite
2. Initialize empty cache
3. Symbols loaded on-demand when referenced

Symbol Access:
1. Check in-memory symbol table
2. If not found, check cache
3. If cache miss, load from SQLite
4. Cache the result
```

### Implementation: Eager Loading

```
Session Load:
1. Open SQLite
2. Query all symbols
3. Deserialize and load into symbol table
4. Session ready (all symbols in memory)

Symbol Access:
1. Normal symbol table lookup (already loaded)
```

## Files Modified/Created

| File | Change |
|------|--------|
| `CMakeLists.txt` | Added `JOY_SESSION` option, SQLite3 detection |
| `include/globals.h` | Added `Session` struct, `env->session` field |
| `src/builtin/session.c` | **New** - all session operators |
| `src/symbol.c` | Write-through hook in `definition()` |
| `tools/gen_builtins.py` | `[SESSION]` marker detection |
| `tools/gen_table.py` | `[SESSION]` marker detection |

## Future Work

In order of priority:

### Priority 1: Fix Complex Value Deserialization

The current limitation where lists/quotations/dicts aren't properly restored is the most significant gap. Options:

1. **Separate parser context** - Create isolated scanner state for deserialization
2. **Lazy deserialization** - Store serialized form, parse on first use
3. **Binary serialization** - Avoid parser entirely with custom format

### Priority 2: Implement Lazy Loading

For large sessions, eager loading is inefficient. Implement:

1. `heap` table for chunked storage
2. Modified symbol lookup with cache-miss handling
3. LRU cache eviction

### Priority 3: Add `session-keep` Operator

Simple addition for conflict resolution:

```c
void session_keep_(pEnv env) {
    // Mark symbol as "keep current" in merge conflict resolution
}
```

### Priority 4: Session Utilities

From design's "Future Extensions":
- `session-info` - Show stats (size, symbol count)
- `session-gc` - Remove unreferenced heap chunks
- `session-export` - Dump as Joy source file
- `session-clone` - Duplicate a session

## Testing

```bash
# Build with sessions enabled
cmake -DJOY_SESSION=ON ..
make

# Test basic persistence
echo '"test" session. DEFINE foo == 42.' | ./joy
echo '"test" session. foo.' | ./joy  # Should print 42

# Test snapshots
./joy <<'EOF'
"test" session.
DEFINE x == 1.
"v1" snapshot.
DEFINE x == 2.
x.
"v1" restore.
x.
EOF
# Should print: 2, then 1
```

## Known Limitations

1. **Simple values only** - Only integers and floats persist correctly; complex values stored as strings
2. **Eager loading** - All symbols loaded at session open
3. **No chunking** - Large values not split for efficient storage
4. **POSIX only** - Uses `open_memstream`/`fmemopen` (needs Windows alternatives)
5. **Current directory** - Sessions stored in current working directory only
