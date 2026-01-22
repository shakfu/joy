# Persistent Joy Design

Deep integration of SQLite into Joy - not just as a module, but as a language persistence system underlying builtins like dictionaries, lists, etc.

## Levels of Integration

### Level 1: Module (simple API)
```joy
db "SELECT * FROM users" [] db-query.
```
SQLite is just another library. See `sqlite_design.md`.

### Level 2: Persistent Data Structures
```joy
"mydata.db" persist-open.
pdict users.                    (* persistent dict *)
users "alice" {age: 30} dput.   (* writes to SQLite *)
persist-close.
(* ... restart Joy ... *)
"mydata.db" persist-open.
users "alice" dget.             (* -> {age: 30} *)
```
Specific types backed by SQLite.

### Level 3: Persistent Symbol Table
```joy
"session.db" session-open.
DEFINE mydata == [1 2 3 4 5].
DEFINE myfunc == [dup *].
session-save.
(* ... restart Joy ... *)
"session.db" session-open.
mydata myfunc map.              (* -> [1 4 9 16 25] *)
```
User definitions survive sessions.

### Level 4: Persistent Heap (deep integration)
```joy
"world.db" world-open.
(* ALL Joy values live in SQLite *)
(* GC writes survivors to DB *)
(* Large structures paged in/out *)
world-checkpoint.
```
SQLite becomes Joy's memory model.

---

## Design Exploration: Persistent Heap

What if SQLite *is* the heap?

```
+------------------+     +-------------------+
|   Joy Runtime    |     |      SQLite       |
+------------------+     +-------------------+
| Stack (in-memory)|     | nodes table       |
| GC roots         |---->| symbols table     |
| Hot cache        |     | strings table     |
+------------------+     | blobs table       |
                         +-------------------+
```

### Schema

```sql
-- All Joy nodes stored here
CREATE TABLE nodes (
    id INTEGER PRIMARY KEY,
    type INTEGER NOT NULL,      -- INTEGER_, LIST_, DICT_, etc.
    int_val INTEGER,
    float_val REAL,
    str_id INTEGER REFERENCES strings(id),
    next_id INTEGER REFERENCES nodes(id),  -- for lists
    dict_id INTEGER,            -- for dicts
    gen INTEGER DEFAULT 0       -- GC generation
);

-- Interned strings
CREATE TABLE strings (
    id INTEGER PRIMARY KEY,
    hash INTEGER NOT NULL,
    value TEXT NOT NULL,
    UNIQUE(hash, value)
);

-- Symbol table (persistent definitions)
CREATE TABLE symbols (
    id INTEGER PRIMARY KEY,
    name TEXT UNIQUE NOT NULL,
    is_user BOOLEAN,
    body_id INTEGER REFERENCES nodes(id),  -- for user definitions
    flags INTEGER
);

-- Named roots (persistent variables)
CREATE TABLE roots (
    name TEXT PRIMARY KEY,
    node_id INTEGER REFERENCES nodes(id)
);

-- Indexes for performance
CREATE INDEX idx_nodes_type ON nodes(type);
CREATE INDEX idx_nodes_gen ON nodes(gen);
CREATE INDEX idx_strings_hash ON strings(hash);
```

### Node Operations

```c
/* Node creation - writes to SQLite */
Index newnode_persistent(pEnv env, int type, Types val, Index next) {
    sqlite3_stmt* stmt = env->db.insert_node;
    sqlite3_bind_int(stmt, 1, type);

    switch (type) {
    case INTEGER_:
        sqlite3_bind_int64(stmt, 2, val.num);
        break;
    case FLOAT_:
        sqlite3_bind_double(stmt, 3, val.dbl);
        break;
    case STRING_:
        int64_t str_id = intern_string(env, val.str);
        sqlite3_bind_int64(stmt, 4, str_id);
        break;
    // ...
    }

    sqlite3_bind_int64(stmt, 5, next);  /* next pointer */
    sqlite3_step(stmt);

    return sqlite3_last_insert_rowid(env->db.handle);
}

/* Reading a node - cached */
Node* fetch_node(pEnv env, Index id) {
    /* Check hot cache first */
    Node* cached = cache_lookup(env, id);
    if (cached) return cached;

    /* Load from SQLite */
    sqlite3_stmt* stmt = env->db.select_node;
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_step(stmt);

    Node* node = cache_alloc(env);
    node->id = id;
    node->type = sqlite3_column_int(stmt, 0);
    // ... populate fields ...

    cache_insert(env, node);
    return node;
}
```

### GC Integration

```c
/* Mark phase - traverse from roots, mark reachable */
void gc_mark_persistent(pEnv env) {
    sqlite3_exec(env->db.handle,
        "UPDATE nodes SET gen = gen + 1 WHERE id IN ("
        "  WITH RECURSIVE reachable(id) AS ("
        "    SELECT node_id FROM roots"
        "    UNION"
        "    SELECT next_id FROM nodes n JOIN reachable r ON n.id = r.id"
        "    UNION"
        "    SELECT body_id FROM symbols WHERE body_id IS NOT NULL"
        "  ) SELECT id FROM reachable"
        ")", NULL, NULL, NULL);
}

/* Sweep phase - delete unmarked */
void gc_sweep_persistent(pEnv env) {
    int current_gen = env->gc_generation;
    sqlite3_exec(env->db.handle,
        "DELETE FROM nodes WHERE gen < ?",
        /* bind current_gen */);
}
```

---

## Hybrid Approach (Practical)

Full persistent heap has overhead. A hybrid might be better:

```
Working Memory (fast)          Persistent Store (durable)
+-------------------+          +-------------------+
| Hot objects       |  <---->  | Cold objects      |
| Recent allocations|  sync    | Named roots       |
| Stack             |          | Definitions       |
| Current computation|         | Large structures  |
+-------------------+          +-------------------+
```

### Joy API

```joy
(* Open persistent world *)
"myworld.db" world-open.

(* Explicit persistence *)
[1 2 3 4 5] "mylist" persist.     (* save to DB *)
"mylist" recall.                   (* -> [1 2 3 4 5] *)

(* Persistent definitions *)
PERSIST myfunc == [dup *].         (* survives restart *)

(* Checkpoint *)
world-checkpoint.                  (* flush all changes *)

(* Large data - automatically paged *)
1000000 vrange "bigdata" persist.  (* doesn't bloat memory *)
"bigdata" recall first.            (* loads just what's needed *)
```

---

## Query Integration

With SQLite underneath, we get query power:

```joy
(* Structured data *)
[{name: "Alice", age: 30, dept: "Engineering"}
 {name: "Bob", age: 25, dept: "Sales"}
 {name: "Carol", age: 35, dept: "Engineering"}] "employees" persist.

(* SQL-like queries via Joy *)
"employees" recall
    [dept dget "Engineering" =] filter
    [age dget] map
    vsum.    (* -> 65 *)

(* Or expose SQL directly on persistent data *)
"SELECT AVG(age) FROM employees WHERE dept = ?" ["Engineering"] query.
```

---

## Persistent Collections

Explicit persistent types that look like regular Joy collections:

### Persistent Dict

```joy
(* Create persistent dict *)
"users.db" pdict-open users.

(* Use like regular dict *)
users "alice" {name: "Alice", age: 30} dput.
users "bob" {name: "Bob", age: 25} dput.
users "alice" dget.   (* -> {name: "Alice", age: 30} *)

(* Backed by SQLite table *)
(* CREATE TABLE users (key TEXT PRIMARY KEY, value BLOB) *)

users pdict-close.
```

### Persistent List (Append-only log)

```joy
(* Event log *)
"events.db" plist-open events.

events {type: "login", user: "alice", time: 1234567890} pappend.
events {type: "click", user: "alice", page: "/home"} pappend.

(* Query recent events *)
events 10 ptake.     (* last 10 events *)
events [type dget "login" =] pfilter size.  (* count logins *)
```

### Implementation

```c
typedef struct PersistentDict {
    sqlite3* db;
    sqlite3_stmt* get_stmt;
    sqlite3_stmt* put_stmt;
    sqlite3_stmt* del_stmt;
    sqlite3_stmt* keys_stmt;
} PersistentDict;

void pdict_put(pEnv env) {
    /* dict key value -> dict */
    Index value = env->stck;
    Index key = nextnode1(env->stck);
    PersistentDict* pd = get_pdict(env, nextnode2(env->stck));

    /* Serialize value to blob */
    size_t blob_size;
    void* blob = serialize_node(env, value, &blob_size);

    sqlite3_bind_text(pd->put_stmt, 1, nodevalue(key).str, -1, SQLITE_STATIC);
    sqlite3_bind_blob(pd->put_stmt, 2, blob, blob_size, SQLITE_STATIC);
    sqlite3_step(pd->put_stmt);
    sqlite3_reset(pd->put_stmt);

    /* Pop key and value, leave dict */
    env->stck = nextnode2(env->stck);
}
```

---

## Persistent Symbol Table

Definitions that survive sessions:

```joy
(* Start session *)
"session.db" session-open.

(* Define functions - automatically persisted *)
DEFINE square == dup *.
DEFINE cube == dup dup * *.
DEFINE hypotenuse == [a b] [a square b square + sqrt] let.

(* Define data *)
DEFINE constants == {pi: 3.14159, e: 2.71828}.

(* Close and reopen *)
session-close.
"session.db" session-open.

(* Definitions still available *)
3 4 hypotenuse.   (* -> 5.0 *)
constants pi dget.  (* -> 3.14159 *)
```

### Schema

```sql
CREATE TABLE definitions (
    name TEXT PRIMARY KEY,
    body TEXT NOT NULL,        -- serialized Joy quotation
    created_at INTEGER,
    modified_at INTEGER
);

CREATE TABLE session_state (
    key TEXT PRIMARY KEY,
    value BLOB
);
```

### Implementation

```c
/* Save definition to SQLite */
void persist_definition(pEnv env, const char* name, Index body) {
    char* serialized = serialize_quotation(env, body);

    sqlite3_stmt* stmt = env->session.insert_def;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, serialized, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, time(NULL));
    sqlite3_step(stmt);
    sqlite3_reset(stmt);
}

/* Load all definitions on session open */
void session_open(pEnv env, const char* path) {
    sqlite3_open(path, &env->session.db);

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(env->session.db,
        "SELECT name, body FROM definitions", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        const char* body = (const char*)sqlite3_column_text(stmt, 1);

        /* Parse and register definition */
        Index parsed = parse_quotation(env, body);
        define_symbol(env, name, parsed);
    }

    sqlite3_finalize(stmt);
}
```

---

## World Persistence (Smalltalk-style)

Save entire interpreter state:

```joy
(* Work on a project *)
"project.world" world-open.

(* Do work... define things, compute values *)
DEFINE data == load-csv "big-data.csv".
DEFINE model == data train-model.
DEFINE results == model evaluate.

(* Save everything *)
world-save.

(* Later, restore exactly where you left off *)
"project.world" world-open.
results.   (* -> immediately available *)
```

### What Gets Saved

- Symbol table (all definitions)
- Named persistent roots
- Configuration state
- Optionally: stack contents

### What Doesn't Get Saved

- File handles
- Network connections
- Transient computation state

---

## Trade-offs

| Aspect | In-Memory | Persistent Heap |
|--------|-----------|-----------------|
| Speed | Fast | Slower (disk I/O) |
| Capacity | Limited by RAM | Limited by disk |
| Durability | Lost on exit | Survives crashes |
| Sharing | Single process | Multiple processes |
| Debugging | Inspect in memory | Query with SQL |
| Complexity | Simple | More complex |

---

## Implementation Phases

### Phase 1: Session Persistence
- Persist DEFINE'd symbols to SQLite
- Reload on session open
- Simple, high value

### Phase 2: Explicit Persistent Collections
- `pdict`, `plist` types
- Backed by SQLite tables
- Opt-in persistence

### Phase 3: Named Roots
- `persist` / `recall` for any value
- Serialization layer
- Checkpoint support

### Phase 4: Lazy Loading (optional)
- Large structures paged from disk
- LRU cache for hot objects
- Transparent to user code

---

## Questions to Consider

1. **Transparency** - Should persistence be automatic or explicit?
2. **Granularity** - Persist individual values or entire state?
3. **Consistency** - When are writes flushed? Transaction boundaries?
4. **Lazy loading** - Load large structures on demand?
5. **Schema evolution** - What happens when Joy types change?
6. **Parallel safety** - How does this interact with `pmap`?
7. **Serialization format** - Binary blob vs JSON vs custom?

---

## Related Work

- **Smalltalk images** - Save entire VM state
- **ZODB (Python)** - Transparent object persistence
- **Datomic** - Immutable database with time travel
- **SQLite as application file format** - Single-file databases
- **Redis** - In-memory with persistence options
- **LevelDB/RocksDB** - Key-value with LSM trees
