# Persistent Session Design

Session-based persistence for Joy - no special syntax, just a mode switch.

## Core Concept

```joy
(* Transient session - current Joy behavior, nothing persists *)
joy

(* Persistent session - everything persists transparently *)
"myproject" session.

DEFINE square == dup *.
DEFINE data == [1 2 3 4 5].

(* ... close Joy, days pass ... *)

"myproject" session.
data square map.   (* -> [1 4 9 16 25] - it's all there *)
```

No `PERSIST`. No `pcreate`. Just Joy.

---

## Two Modes

| | Transient | Persistent |
|---|-----------|------------|
| Start | `joy` | `"name" session.` |
| DEFINE | Gone on exit | Survives |
| Data | In memory only | Backed by SQLite |
| Large data | Limited by RAM | Lazy-loaded from disk |
| Queries | Joy only | Joy + SQL |

---

## State Snapshots

```joy
"myproject" session.

DEFINE model == train-expensive-model.
"before-tuning" snapshot.

(* experiment... *)
DEFINE model == model tweak-params.
"after-tuning-v1" snapshot.

(* oops, that was worse *)
"before-tuning" restore.

(* list available snapshots *)
snapshots.   (* -> ["before-tuning" "after-tuning-v1"] *)
```

---

## Session Merging

Sessions can be merged like git branches - combine work from multiple sessions if there are no conflicts.

```joy
(* Work on feature in separate session *)
"experiments" session.
DEFINE new-algo == [...].
DEFINE helper == [...].

(* Switch to main session *)
"main" session.

(* Merge experiments into main *)
"experiments" session-merge.   (* succeeds if no conflicts *)

(* Now main has new-algo and helper *)
```

### Conflict Handling

```joy
(* If both sessions define 'helper' differently: *)
"experiments" session-merge.
(* -> error: conflict on 'helper' - defined differently in both sessions *)

(* See what's different *)
"experiments" session-diff.
(* -> {added: ["new-algo"], modified: ["helper"], removed: []} *)

(* Resolve by taking their version *)
"experiments" "helper" session-take.

(* Or keep ours *)
"helper" session-keep.

(* Now merge succeeds *)
"experiments" session-merge.
```

### Merge Semantics

| Situation | Result |
|-----------|--------|
| Symbol only in source | Added to target |
| Symbol only in target | Kept |
| Same symbol, same body | No-op |
| Same symbol, different body | **Conflict** |

---

## How It Works

The session wraps the symbol table and heap with SQLite:

```
Transient Mode              Persistent Mode
+--------------+            +------------------+
| Symbol Table |            | Symbol Table     |
| (in memory)  |            | (SQLite-backed)  |
+--------------+            +------------------+
| Heap         |            | Heap             |
| (in memory)  |            | (hybrid: hot cache + SQLite) |
+--------------+            +------------------+
                            | Auto-checkpoint  |
                            +------------------+
```

When you `DEFINE` in a persistent session:
1. Definition goes into symbol table (as usual)
2. Symbol table write-through to SQLite
3. Value serialized and stored

When you reference a symbol:
1. Check in-memory cache
2. If miss, load from SQLite
3. Large structures loaded lazily (only what's accessed)

---

## Schema

```sql
-- Core schema for persistent sessions
CREATE TABLE IF NOT EXISTS symbols (
    name TEXT PRIMARY KEY,
    body TEXT NOT NULL,           -- serialized Joy expression
    type TEXT,                    -- 'quotation', 'value', 'list', 'dict'
    size INTEGER,                 -- for lazy loading decisions
    modified_at INTEGER,
    created_at INTEGER
);

-- For lazy-loaded large structures
CREATE TABLE IF NOT EXISTS heap (
    id INTEGER PRIMARY KEY,
    symbol_name TEXT,             -- which symbol owns this
    chunk_index INTEGER,          -- for large lists/dicts
    data BLOB,                    -- serialized chunk
    FOREIGN KEY (symbol_name) REFERENCES symbols(name)
);

-- State snapshots
CREATE TABLE IF NOT EXISTS snapshots (
    name TEXT,
    symbol_name TEXT,
    body TEXT,
    created_at INTEGER,
    PRIMARY KEY (name, symbol_name)
);

-- Session metadata
CREATE TABLE IF NOT EXISTS meta (
    key TEXT PRIMARY KEY,
    value TEXT
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_heap_symbol ON heap(symbol_name);
CREATE INDEX IF NOT EXISTS idx_snapshots_name ON snapshots(name);
```

---

## Implementation

### Session Structure

```c
typedef struct Session {
    sqlite3* db;
    char* name;
    char* path;
    int persistent;              /* 0 = transient, 1 = persistent */
    int autosave;                /* auto-checkpoint on changes */

    /* Hot cache for frequently accessed values */
    khash_t(Cache)* cache;
    size_t cache_size;
    size_t cache_limit;          /* LRU eviction threshold */

    /* Prepared statements */
    sqlite3_stmt* insert_symbol;
    sqlite3_stmt* select_symbol;
    sqlite3_stmt* delete_symbol;
    sqlite3_stmt* insert_chunk;
    sqlite3_stmt* select_chunks;
} Session;
```

### Session Open

```c
/**
Q0  OK  3200  session  :  "name"  ->
Opens or creates a persistent session.
*/
void session_(pEnv env)
{
    const char* name;
    char path[PATH_MAX];

    ONEPARAM("session");
    STRING("session");
    name = nodevalue(env->stck).str;
    POP(env->stck);

    /* Close existing session if any */
    if (env->session && env->session->persistent) {
        session_close_internal(env);
    }

    /* Create session */
    env->session = GC_alloc(sizeof(Session));
    memset(env->session, 0, sizeof(Session));
    env->session->name = GC_strdup(name);
    env->session->persistent = 1;
    env->session->autosave = 1;
    env->session->cache = kh_init(Cache);
    env->session->cache_limit = 1000;  /* entries */

    /* Open database */
    snprintf(path, sizeof(path), "%s.joy.db", name);
    env->session->path = GC_strdup(path);

    if (sqlite3_open(path, &env->session->db) != SQLITE_OK) {
        execerror(env, "cannot open session database", name);
        return;
    }

    /* Initialize schema */
    ensure_schema(env->session);
    prepare_statements(env->session);

    /* Load existing definitions */
    load_symbols_from_db(env);

    printf("session: opened '%s'\n", name);
}
```

### Symbol Definition (Write-Through)

```c
/* Modified define to write-through in persistent mode */
void define_symbol(pEnv env, const char* name, Index body)
{
    /* Normal symbol table update */
    Entry ent;
    memset(&ent, 0, sizeof(ent));
    ent.name = GC_strdup(name);
    ent.is_user = 1;
    ent.u.body = body;

    int index = vec_size(env->symtab);
    addsymbol(env, ent, index);
    vec_push(env->symtab, ent);

    /* If persistent session, write to SQLite */
    if (env->session && env->session->persistent) {
        persist_symbol(env, name, body);
    }
}

static void persist_symbol(pEnv env, const char* name, Index body)
{
    Session* s = env->session;

    /* Serialize the body */
    char* serialized = serialize_value(env, body);
    size_t size = serialized ? strlen(serialized) : 0;

    /* Determine if this needs chunked storage */
    if (size > CHUNK_THRESHOLD) {
        persist_large_value(env, name, body);
        serialized = "@chunked";  /* marker */
    }

    /* Insert/update symbol */
    sqlite3_reset(s->insert_symbol);
    sqlite3_bind_text(s->insert_symbol, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(s->insert_symbol, 2, serialized, -1, SQLITE_STATIC);
    sqlite3_bind_int64(s->insert_symbol, 3, time(NULL));

    if (sqlite3_step(s->insert_symbol) != SQLITE_DONE) {
        fprintf(stderr, "session: failed to persist '%s'\n", name);
    }
}
```

### Symbol Lookup (Lazy Load)

```c
/* Modified lookup to load from SQLite if needed */
Index lookup_user_symbol(pEnv env, const char* name)
{
    /* Check in-memory symbol table first */
    Entry* ent = find_in_symtab(env, name);
    if (ent && ent->u.body) {
        return ent->u.body;
    }

    /* If persistent session, try loading from SQLite */
    if (env->session && env->session->persistent) {
        return load_symbol_from_db(env, name);
    }

    return NULL;  /* not found */
}

static Index load_symbol_from_db(pEnv env, const char* name)
{
    Session* s = env->session;

    /* Check cache first */
    khint_t k = kh_get(Cache, s->cache, name);
    if (k != kh_end(s->cache)) {
        return kh_val(s->cache, k);
    }

    /* Query SQLite */
    sqlite3_reset(s->select_symbol);
    sqlite3_bind_text(s->select_symbol, 1, name, -1, SQLITE_STATIC);

    if (sqlite3_step(s->select_symbol) != SQLITE_ROW) {
        return NULL;
    }

    const char* body = (const char*)sqlite3_column_text(s->select_symbol, 0);

    /* Handle chunked storage */
    Index value;
    if (strcmp(body, "@chunked") == 0) {
        value = load_chunked_value(env, name);
    } else {
        value = deserialize_value(env, body);
    }

    /* Cache it */
    cache_insert(env, name, value);

    return value;
}
```

### Snapshots

```c
/**
Q0  OK  3210  snapshot  :  "name"  ->
Saves current state as named snapshot.
*/
void snapshot_(pEnv env)
{
    const char* name;
    Session* s = env->session;

    ONEPARAM("snapshot");
    STRING("snapshot");
    name = nodevalue(env->stck).str;
    POP(env->stck);

    if (!s || !s->persistent) {
        execerror(env, "no persistent session", "snapshot");
        return;
    }

    /* Delete existing snapshot with same name */
    sqlite3_exec(s->db,
        "DELETE FROM snapshots WHERE name = ?",
        /* bind name */);

    /* Copy current symbols into snapshot */
    char* sql = sqlite3_mprintf(
        "INSERT INTO snapshots (name, symbol_name, body, created_at) "
        "SELECT %Q, name, body, %lld FROM symbols",
        name, (long long)time(NULL));

    sqlite3_exec(s->db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    printf("session: snapshot '%s' created\n", name);
}

/**
Q0  OK  3211  restore  :  "name"  ->
Restores state from named snapshot.
*/
void restore_(pEnv env)
{
    const char* name;
    Session* s = env->session;

    ONEPARAM("restore");
    STRING("restore");
    name = nodevalue(env->stck).str;
    POP(env->stck);

    if (!s || !s->persistent) {
        execerror(env, "no persistent session", "restore");
        return;
    }

    /* Check snapshot exists */
    char* sql = sqlite3_mprintf(
        "SELECT COUNT(*) FROM snapshots WHERE name = %Q", name);
    /* ... verify count > 0 ... */

    /* Clear current symbols */
    sqlite3_exec(s->db, "DELETE FROM symbols", NULL, NULL, NULL);

    /* Restore from snapshot */
    sql = sqlite3_mprintf(
        "INSERT INTO symbols (name, body, modified_at) "
        "SELECT symbol_name, body, %lld FROM snapshots WHERE name = %Q",
        (long long)time(NULL), name);
    sqlite3_exec(s->db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    /* Clear in-memory state and reload */
    clear_user_symbols(env);
    kh_clear(Cache, s->cache);
    load_symbols_from_db(env);

    printf("session: restored to '%s'\n", name);
}

/**
Q0  OK  3212  snapshots  :  ->  [names]
Lists all available snapshots.
*/
void snapshots_(pEnv env)
{
    Session* s = env->session;
    Index result = NULL;

    if (!s || !s->persistent) {
        NULLARY(LIST_NEWNODE, NULL);
        return;
    }

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(s->db,
        "SELECT DISTINCT name FROM snapshots ORDER BY created_at DESC",
        -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        env->bucket.str = GC_strdup(name);
        result = newnode(env, STRING_, env->bucket, result);
    }
    sqlite3_finalize(stmt);

    /* Reverse to maintain order */
    result = reverse_list(env, result);
    NULLARY(LIST_NEWNODE, result);
}
```

### SQL Queries

```c
/**
Q0  OK  3220  sql  :  "query" [params]  ->  [results]
Execute SQL query on session data.
*/
void sql_(pEnv env)
{
    const char* query;
    Index params;
    Session* s = env->session;

    TWOPARAMS("sql");
    LIST("sql");
    params = nodevalue(env->stck).lis;
    POP(env->stck);

    STRING("sql");
    query = nodevalue(env->stck).str;
    POP(env->stck);

    if (!s || !s->persistent) {
        execerror(env, "no persistent session", "sql");
        return;
    }

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(s->db, query, -1, &stmt, NULL) != SQLITE_OK) {
        execerror(env, sqlite3_errmsg(s->db), "sql");
        return;
    }

    /* Bind parameters */
    int i = 1;
    for (Index p = params; p; p = nextnode1(p), i++) {
        bind_joy_value(stmt, i, p);
    }

    /* Collect results */
    Index results = NULL;
    int col_count = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        /* Each row becomes a dict */
        khash_t(Dict)* dict = kh_init(Dict);

        for (int c = 0; c < col_count; c++) {
            const char* col_name = sqlite3_column_name(stmt, c);
            Index val = sqlite_to_joy(env, stmt, c);
            dict_put(dict, col_name, val);
        }

        env->bucket.dict = dict;
        results = newnode(env, DICT_, env->bucket, results);
    }
    sqlite3_finalize(stmt);

    results = reverse_list(env, results);
    NULLARY(LIST_NEWNODE, results);
}
```

### Session Merging

```c
/**
Q0  OK  3230  session-merge  :  "source"  ->  bool
Merges source session into current. Returns false if conflicts exist.
*/
void session_merge_(pEnv env)
{
    const char* source_name;
    Session* target = env->session;
    sqlite3* source_db;
    char path[PATH_MAX];

    ONEPARAM("session-merge");
    STRING("session-merge");
    source_name = nodevalue(env->stck).str;
    POP(env->stck);

    if (!target || !target->persistent) {
        execerror(env, "no persistent session", "session-merge");
        return;
    }

    /* Open source session read-only */
    snprintf(path, sizeof(path), "%s.joy.db", source_name);
    if (sqlite3_open_v2(path, &source_db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        execerror(env, "cannot open source session", source_name);
        return;
    }

    /* Attach source to target for comparison */
    char* attach_sql = sqlite3_mprintf("ATTACH DATABASE %Q AS source", path);
    sqlite3_exec(target->db, attach_sql, NULL, NULL, NULL);
    sqlite3_free(attach_sql);

    /* Find conflicts: same name, different body */
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(target->db,
        "SELECT s.name FROM source.symbols s "
        "JOIN symbols t ON s.name = t.name "
        "WHERE s.body != t.body",
        -1, &stmt, NULL);

    Index conflicts = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        env->bucket.str = GC_strdup(name);
        conflicts = newnode(env, STRING_, env->bucket, conflicts);
    }
    sqlite3_finalize(stmt);

    if (conflicts) {
        /* Report conflicts, return false */
        printf("session-merge: conflicts on:");
        for (Index p = conflicts; p; p = nextnode1(p)) {
            printf(" %s", nodevalue(p).str);
        }
        printf("\n");
        sqlite3_exec(target->db, "DETACH DATABASE source", NULL, NULL, NULL);
        sqlite3_close(source_db);
        NULLARY(BOOLEAN_NEWNODE, 0);
        return;
    }

    /* No conflicts - merge symbols that only exist in source */
    sqlite3_exec(target->db,
        "INSERT INTO symbols (name, body, type, size, created_at, modified_at) "
        "SELECT s.name, s.body, s.type, s.size, s.created_at, s.modified_at "
        "FROM source.symbols s "
        "WHERE s.name NOT IN (SELECT name FROM symbols)",
        NULL, NULL, NULL);

    /* Detach and close */
    sqlite3_exec(target->db, "DETACH DATABASE source", NULL, NULL, NULL);
    sqlite3_close(source_db);

    /* Reload symbols */
    load_symbols_from_db(env);

    printf("session-merge: merged '%s' successfully\n", source_name);
    NULLARY(BOOLEAN_NEWNODE, 1);
}

/**
Q0  OK  3231  session-diff  :  "other"  ->  dict
Shows differences between current session and another.
Returns dict with keys: added, modified, removed.
*/
void session_diff_(pEnv env)
{
    const char* other_name;
    Session* current = env->session;
    char path[PATH_MAX];

    ONEPARAM("session-diff");
    STRING("session-diff");
    other_name = nodevalue(env->stck).str;
    POP(env->stck);

    if (!current || !current->persistent) {
        execerror(env, "no persistent session", "session-diff");
        return;
    }

    /* Attach other session */
    snprintf(path, sizeof(path), "%s.joy.db", other_name);
    char* attach_sql = sqlite3_mprintf("ATTACH DATABASE %Q AS other", path);
    sqlite3_exec(current->db, attach_sql, NULL, NULL, NULL);
    sqlite3_free(attach_sql);

    sqlite3_stmt* stmt;
    Index added = NULL, modified = NULL, removed = NULL;

    /* Added: in other but not in current */
    sqlite3_prepare_v2(current->db,
        "SELECT name FROM other.symbols WHERE name NOT IN (SELECT name FROM symbols)",
        -1, &stmt, NULL);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        env->bucket.str = GC_strdup((const char*)sqlite3_column_text(stmt, 0));
        added = newnode(env, STRING_, env->bucket, added);
    }
    sqlite3_finalize(stmt);

    /* Modified: in both but different */
    sqlite3_prepare_v2(current->db,
        "SELECT o.name FROM other.symbols o "
        "JOIN symbols s ON o.name = s.name WHERE o.body != s.body",
        -1, &stmt, NULL);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        env->bucket.str = GC_strdup((const char*)sqlite3_column_text(stmt, 0));
        modified = newnode(env, STRING_, env->bucket, modified);
    }
    sqlite3_finalize(stmt);

    /* Removed: in current but not in other */
    sqlite3_prepare_v2(current->db,
        "SELECT name FROM symbols WHERE name NOT IN (SELECT name FROM other.symbols)",
        -1, &stmt, NULL);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        env->bucket.str = GC_strdup((const char*)sqlite3_column_text(stmt, 0));
        removed = newnode(env, STRING_, env->bucket, removed);
    }
    sqlite3_finalize(stmt);

    sqlite3_exec(current->db, "DETACH DATABASE other", NULL, NULL, NULL);

    /* Build result dict */
    khash_t(Dict)* dict = kh_init(Dict);
    int ret;
    khint_t k;

    k = kh_put(Dict, dict, GC_strdup("added"), &ret);
    env->bucket.lis = reverse_list(env, added);
    kh_val(dict, k) = newnode(env, LIST_, env->bucket, NULL);

    k = kh_put(Dict, dict, GC_strdup("modified"), &ret);
    env->bucket.lis = reverse_list(env, modified);
    kh_val(dict, k) = newnode(env, LIST_, env->bucket, NULL);

    k = kh_put(Dict, dict, GC_strdup("removed"), &ret);
    env->bucket.lis = reverse_list(env, removed);
    kh_val(dict, k) = newnode(env, LIST_, env->bucket, NULL);

    env->bucket.dict = dict;
    env->stck = newnode(env, DICT_, env->bucket, env->stck);
}

/**
Q0  OK  3232  session-take  :  "source" "symbol"  ->
Takes a symbol from source session, overwriting current.
*/
void session_take_(pEnv env)
{
    const char* source_name;
    const char* symbol_name;
    Session* current = env->session;
    char path[PATH_MAX];

    TWOPARAMS("session-take");
    STRING("session-take");
    symbol_name = nodevalue(env->stck).str;
    POP(env->stck);

    STRING("session-take");
    source_name = nodevalue(env->stck).str;
    POP(env->stck);

    if (!current || !current->persistent) {
        execerror(env, "no persistent session", "session-take");
        return;
    }

    /* Attach source */
    snprintf(path, sizeof(path), "%s.joy.db", source_name);
    char* attach_sql = sqlite3_mprintf("ATTACH DATABASE %Q AS source", path);
    sqlite3_exec(current->db, attach_sql, NULL, NULL, NULL);
    sqlite3_free(attach_sql);

    /* Replace symbol */
    char* sql = sqlite3_mprintf(
        "INSERT OR REPLACE INTO symbols (name, body, type, size, created_at, modified_at) "
        "SELECT name, body, type, size, created_at, %lld FROM source.symbols WHERE name = %Q",
        (long long)time(NULL), symbol_name);
    sqlite3_exec(current->db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    sqlite3_exec(current->db, "DETACH DATABASE source", NULL, NULL, NULL);

    /* Reload that symbol */
    invalidate_cache(current, symbol_name);
    printf("session-take: took '%s' from '%s'\n", symbol_name, source_name);
}

/**
Q0  OK  3233  sessions  :  ->  [names]
Lists all available sessions (*.joy.db files in current directory).
*/
void sessions_(pEnv env)
{
    Index result = NULL;
    DIR* dir = opendir(".");
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        char* name = entry->d_name;
        size_t len = strlen(name);

        /* Check for .joy.db suffix */
        if (len > 7 && strcmp(name + len - 7, ".joy.db") == 0) {
            /* Extract session name (remove suffix) */
            char* session_name = GC_alloc(len - 6);
            strncpy(session_name, name, len - 7);
            session_name[len - 7] = '\0';

            env->bucket.str = session_name;
            result = newnode(env, STRING_, env->bucket, result);
        }
    }
    closedir(dir);

    result = reverse_list(env, result);
    NULLARY(LIST_NEWNODE, result);
}
```

---

## Lazy Loading for Large Structures

Large lists and dicts are stored in chunks:

```c
#define CHUNK_THRESHOLD 4096    /* bytes */
#define CHUNK_SIZE 100          /* elements per chunk */

typedef struct LazyList {
    char* symbol_name;
    int64_t total_size;
    int64_t loaded_start;
    int64_t loaded_count;
    Index* loaded_items;
} LazyList;

/* Access element - loads chunk on demand */
Index lazy_list_nth(pEnv env, LazyList* ll, int64_t index)
{
    /* Check if already loaded */
    if (index >= ll->loaded_start &&
        index < ll->loaded_start + ll->loaded_count) {
        return ll->loaded_items[index - ll->loaded_start];
    }

    /* Load chunk containing index */
    int64_t chunk_num = index / CHUNK_SIZE;
    load_chunk(env, ll, chunk_num);

    return ll->loaded_items[index - ll->loaded_start];
}
```

---

## Operator Reference

| Operator | Stack Effect | Description |
|----------|--------------|-------------|
| **Session** | | |
| `session` | `"name" ->` | Open/create persistent session |
| `session-close` | `->` | Close session (optional) |
| `sessions` | `-> [names]` | List all available sessions |
| **Snapshots** | | |
| `snapshot` | `"name" ->` | Save current state |
| `restore` | `"name" ->` | Restore to snapshot |
| `snapshots` | `-> [names]` | List available snapshots |
| **Merging** | | |
| `session-merge` | `"source" -> bool` | Merge source into current |
| `session-diff` | `"other" -> dict` | Compare sessions |
| `session-take` | `"source" "symbol" ->` | Take symbol from source |
| **Query** | | |
| `sql` | `"query" [params] -> [results]` | Query session data |

Everything else is just normal Joy - the persistence is transparent.

---

## Example Session

```joy
(* First run *)
"myproject" session.

DEFINE fib == [dup 2 <] [1] [dup 1 - fib swap 2 - fib +] ifte.
DEFINE cached_fibs == 1 40 vrange [fib] map.

"computed-fibs" snapshot.

(* ... quit Joy ... *)

(* Later *)
"myproject" session.

cached_fibs.   (* -> instantly available, loaded from SQLite *)
45 fib.        (* -> fib is still defined *)

(* Experiment with changes *)
DEFINE fib == [dup 2 <] [] [dup 1 - fib swap 2 - fib + *] ifte.  (* oops, bug *)

"computed-fibs" restore.   (* back to working version *)
```

---

## Example: Branching Workflow

```joy
(* Main development session *)
"main" session.
DEFINE core-algo == [...].
DEFINE utils == [...].

(* Start experimental work in separate session *)
"experiment" session.
DEFINE core-algo == [...].      (* inherited or redefined *)
DEFINE new-feature == [...].    (* new work *)
DEFINE utils == [... improved ...].  (* modified *)

(* Check what changed *)
"main" session.
"experiment" session-diff.
(* -> {added: ["new-feature"], modified: ["utils"], removed: []} *)

(* Merge the new feature (no conflict) *)
"experiment" session-merge.
(* -> conflict on 'utils' *)

(* Resolve: take their improved utils *)
"experiment" "utils" session-take.

(* Now merge succeeds *)
"experiment" session-merge.
(* -> merged successfully, now have new-feature *)

(* List all sessions *)
sessions.   (* -> ["main" "experiment"] *)
```

---

## Design Decisions

1. **Explicit session start** - `"name" session.` makes it clear you're in persistent mode
2. **Transparent thereafter** - No special syntax for definitions or data
3. **Write-through** - Changes persist immediately (with optional batching)
4. **Lazy loading** - Large structures don't bloat memory
5. **Snapshots** - Named checkpoints for experimentation
6. **SQL access** - Query your data with full SQL power

---

## Future Extensions

- **Session info** - `session-info.` to show stats (size, symbol count, etc.)
- **Garbage collection** - `session-gc.` to remove unreferenced heap chunks
- **Export/import** - `session-export.` to dump as Joy source file
- **Diff snapshots** - `"a" "b" snapshot-diff.` to see changes between snapshots
- **Session clone** - `"source" "dest" session-clone.` to duplicate a session
- **Three-way merge** - `"base" "ours" "theirs" session-merge3.` for complex merges
