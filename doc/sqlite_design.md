# SQLite Integration Design

SQLite is a natural fit for Joy - it's self-contained (single C file), has no dependencies, and provides persistent storage and query capabilities.

## Design Options

### Option 1: As a Plugin (using the plugin API)

```c
// plugins/sqlite_plugin.c
#include <joy/plugin.h>
#include <sqlite3.h>
```

Pros: Optional, doesn't bloat core Joy
Cons: Requires plugin system to be built first

### Option 2: As Core Builtins

Add `src/builtin/sqlite.c` with conditional compilation (`-DJOY_SQLITE=ON`)

Pros: Simpler, immediately available
Cons: Adds dependency (though sqlite3.c is just one file)

---

## Proposed Joy API

```joy
(* Open/close database *)
"mydb.sqlite" db-open.          (* -> db-handle *)
db db-close.                     (* close handle *)

(* Execute statements *)
db "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)" [] db-exec.

(* Parameterized queries - safe from injection *)
db "INSERT INTO users (name) VALUES (?)" ["Alice"] db-exec.
db "INSERT INTO users (name) VALUES (?)" ["Bob"] db-exec.

(* Query - returns list of rows (each row is a dict) *)
db "SELECT * FROM users" [] db-query.
(* -> [{id: 1, name: "Alice"} {id: 2, name: "Bob"}] *)

(* Query with parameters *)
db "SELECT * FROM users WHERE id = ?" [1] db-query.
(* -> [{id: 1, name: "Alice"}] *)

(* Transactions *)
db db-begin.
db "UPDATE users SET name = ? WHERE id = ?" ["Alicia" 1] db-exec.
db db-commit.       (* or db-rollback *)

(* Get last insert ID *)
db db-last-id.      (* -> 2 *)

(* Get rows affected *)
db db-changes.      (* -> 1 *)
```

---

## Implementation (`src/builtin/sqlite.c`)

```c
/*
 *  module  : sqlite.c
 *  SQLite database integration
 */
#include "globals.h"

#ifdef JOY_SQLITE
#include <sqlite3.h>

/* Store db handles in a node type or as opaque integer index */
#define MAX_DB_HANDLES 16
static sqlite3* db_handles[MAX_DB_HANDLES];
static int db_count = 0;

/**
Q0  OK  3100  db-open  :  "path"  ->  db
Opens SQLite database at path, returns handle.
*/
void db_open_(pEnv env)
{
    const char* path;
    sqlite3* db;
    int rc;

    ONEPARAM("db-open");
    STRING("db-open");
    path = nodevalue(env->stck).str;

    rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK) {
        execerror(env, sqlite3_errmsg(db), "db-open");
        sqlite3_close(db);
        return;
    }

    if (db_count >= MAX_DB_HANDLES) {
        execerror(env, "too many open databases", "db-open");
        sqlite3_close(db);
        return;
    }

    db_handles[db_count] = db;
    UNARY(INTEGER_NEWNODE, db_count);
    db_count++;
}

/**
Q0  OK  3101  db-close  :  db  ->
Closes database handle.
*/
void db_close_(pEnv env)
{
    int64_t handle;

    ONEPARAM("db-close");
    INTEGER("db-close");
    handle = nodevalue(env->stck).num;

    if (handle < 0 || handle >= db_count || !db_handles[handle]) {
        execerror(env, "invalid database handle", "db-close");
        return;
    }

    sqlite3_close(db_handles[handle]);
    db_handles[handle] = NULL;
    POP(env->stck);
}

/**
Q0  OK  3102  db-exec  :  db "sql" [params]  ->
Executes SQL statement with parameters.
*/
void db_exec_(pEnv env)
{
    int64_t handle;
    const char* sql;
    Index params;
    sqlite3* db;
    sqlite3_stmt* stmt;
    int rc, i;

    THREEPARAMS("db-exec");
    LIST("db-exec");
    params = nodevalue(env->stck).lis;
    POP(env->stck);

    STRING("db-exec");
    sql = nodevalue(env->stck).str;
    POP(env->stck);

    INTEGER("db-exec");
    handle = nodevalue(env->stck).num;
    POP(env->stck);

    db = db_handles[handle];
    if (!db) {
        execerror(env, "invalid database handle", "db-exec");
        return;
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        execerror(env, sqlite3_errmsg(db), "db-exec");
        return;
    }

    /* Bind parameters */
    i = 1;
    for (Index p = params; p; p = nextnode1(p), i++) {
        switch (nodetype(p)) {
        case INTEGER_:
            sqlite3_bind_int64(stmt, i, nodevalue(p).num);
            break;
        case FLOAT_:
            sqlite3_bind_double(stmt, i, nodevalue(p).dbl);
            break;
        case STRING_:
            sqlite3_bind_text(stmt, i, nodevalue(p).str, -1, SQLITE_TRANSIENT);
            break;
        case USR_:
            if (nodevalue(p).ent == 0) /* null */
                sqlite3_bind_null(stmt, i);
            break;
        default:
            sqlite3_finalize(stmt);
            execerror(env, "unsupported parameter type", "db-exec");
            return;
        }
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        execerror(env, sqlite3_errmsg(db), "db-exec");
    }
}

/**
Q0  OK  3103  db-query  :  db "sql" [params]  ->  [rows]
Executes query, returns list of dictionaries.
*/
void db_query_(pEnv env)
{
    int64_t handle;
    const char* sql;
    Index params, result, row_list;
    sqlite3* db;
    sqlite3_stmt* stmt;
    int rc, i, col_count;

    THREEPARAMS("db-query");
    LIST("db-query");
    params = nodevalue(env->stck).lis;
    POP(env->stck);

    STRING("db-query");
    sql = nodevalue(env->stck).str;
    POP(env->stck);

    INTEGER("db-query");
    handle = nodevalue(env->stck).num;
    POP(env->stck);

    db = db_handles[handle];
    if (!db) {
        execerror(env, "invalid database handle", "db-query");
        return;
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        execerror(env, sqlite3_errmsg(db), "db-query");
        return;
    }

    /* Bind parameters */
    i = 1;
    for (Index p = params; p; p = nextnode1(p), i++) {
        switch (nodetype(p)) {
        case INTEGER_:
            sqlite3_bind_int64(stmt, i, nodevalue(p).num);
            break;
        case FLOAT_:
            sqlite3_bind_double(stmt, i, nodevalue(p).dbl);
            break;
        case STRING_:
            sqlite3_bind_text(stmt, i, nodevalue(p).str, -1, SQLITE_TRANSIENT);
            break;
        default:
            sqlite3_bind_null(stmt, i);
            break;
        }
    }

    col_count = sqlite3_column_count(stmt);
    result = NULL;  /* Will build list of rows */

    /* Collect rows */
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        /* Create dict for this row */
        khash_t(Dict)* dict = kh_init(Dict);

        for (i = 0; i < col_count; i++) {
            const char* col_name = sqlite3_column_name(stmt, i);
            int type = sqlite3_column_type(stmt, i);
            Index val = NULL;

            switch (type) {
            case SQLITE_INTEGER:
                env->bucket.num = sqlite3_column_int64(stmt, i);
                val = newnode(env, INTEGER_, env->bucket, NULL);
                break;
            case SQLITE_FLOAT:
                env->bucket.dbl = sqlite3_column_double(stmt, i);
                val = newnode(env, FLOAT_, env->bucket, NULL);
                break;
            case SQLITE_TEXT:
                env->bucket.str = GC_strdup((const char*)sqlite3_column_text(stmt, i));
                val = newnode(env, STRING_, env->bucket, NULL);
                break;
            case SQLITE_NULL:
            default:
                /* Store null */
                env->bucket.num = 0;
                val = newnode(env, USR_, env->bucket, NULL);
                break;
            }

            /* Add to dict */
            int ret;
            khint_t k = kh_put(Dict, dict, GC_strdup(col_name), &ret);
            kh_val(dict, k) = val;
        }

        /* Add dict to result list */
        env->bucket.dict = dict;
        result = newnode(env, DICT_, env->bucket, result);
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        execerror(env, sqlite3_errmsg(db), "db-query");
        return;
    }

    /* Reverse list to maintain order, then push */
    row_list = NULL;
    for (Index p = result; p; p = nextnode1(p)) {
        row_list = newnode2(env, p, row_list);
    }

    env->bucket.lis = row_list;
    env->stck = newnode(env, LIST_, env->bucket, env->stck);
}

/**
Q0  OK  3104  db-begin  :  db  ->
Begins a transaction.
*/
void db_begin_(pEnv env)
{
    int64_t handle;
    sqlite3* db;

    ONEPARAM("db-begin");
    INTEGER("db-begin");
    handle = nodevalue(env->stck).num;

    db = db_handles[handle];
    if (!db) {
        execerror(env, "invalid database handle", "db-begin");
        return;
    }

    if (sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL) != SQLITE_OK) {
        execerror(env, sqlite3_errmsg(db), "db-begin");
    }
    /* Leave handle on stack for chaining */
}

/**
Q0  OK  3105  db-commit  :  db  ->
Commits current transaction.
*/
void db_commit_(pEnv env)
{
    int64_t handle;
    sqlite3* db;

    ONEPARAM("db-commit");
    INTEGER("db-commit");
    handle = nodevalue(env->stck).num;

    db = db_handles[handle];
    if (!db) {
        execerror(env, "invalid database handle", "db-commit");
        return;
    }

    if (sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        execerror(env, sqlite3_errmsg(db), "db-commit");
    }
    POP(env->stck);
}

/**
Q0  OK  3106  db-rollback  :  db  ->
Rolls back current transaction.
*/
void db_rollback_(pEnv env)
{
    int64_t handle;
    sqlite3* db;

    ONEPARAM("db-rollback");
    INTEGER("db-rollback");
    handle = nodevalue(env->stck).num;

    db = db_handles[handle];
    if (!db) {
        execerror(env, "invalid database handle", "db-rollback");
        return;
    }

    if (sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL) != SQLITE_OK) {
        execerror(env, sqlite3_errmsg(db), "db-rollback");
    }
    POP(env->stck);
}

/**
Q0  OK  3107  db-last-id  :  db  ->  int
Returns the rowid of the last INSERT.
*/
void db_last_id_(pEnv env)
{
    int64_t handle;
    sqlite3* db;

    ONEPARAM("db-last-id");
    INTEGER("db-last-id");
    handle = nodevalue(env->stck).num;

    db = db_handles[handle];
    if (!db) {
        execerror(env, "invalid database handle", "db-last-id");
        return;
    }

    UNARY(INTEGER_NEWNODE, sqlite3_last_insert_rowid(db));
}

/**
Q0  OK  3108  db-changes  :  db  ->  int
Returns number of rows affected by last statement.
*/
void db_changes_(pEnv env)
{
    int64_t handle;
    sqlite3* db;

    ONEPARAM("db-changes");
    INTEGER("db-changes");
    handle = nodevalue(env->stck).num;

    db = db_handles[handle];
    if (!db) {
        execerror(env, "invalid database handle", "db-changes");
        return;
    }

    UNARY(INTEGER_NEWNODE, sqlite3_changes(db));
}

#endif /* JOY_SQLITE */
```

---

## CMake Integration

```cmake
option(JOY_SQLITE "Enable SQLite support" OFF)

if(JOY_SQLITE)
    # Option 1: Use system SQLite
    find_package(SQLite3 REQUIRED)
    target_link_libraries(joycore_static PRIVATE SQLite::SQLite3)

    # Option 2: Bundle sqlite3.c (single-file amalgamation)
    # add_library(sqlite3 STATIC vendor/sqlite3.c)
    # target_link_libraries(joycore_static PRIVATE sqlite3)

    target_compile_definitions(joycore_static PRIVATE JOY_SQLITE)
endif()
```

---

## Full Example Session

```joy
(* Create and populate a database *)
":memory:" db-open.   (* in-memory db for testing *)
dup "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT, price REAL)" [] db-exec.
dup "INSERT INTO items (name, price) VALUES (?, ?)" ["Apple" 1.50] db-exec.
dup "INSERT INTO items (name, price) VALUES (?, ?)" ["Banana" 0.75] db-exec.
dup "INSERT INTO items (name, price) VALUES (?, ?)" ["Cherry" 3.00] db-exec.

(* Query *)
dup "SELECT * FROM items WHERE price < ?" [2.0] db-query.
(* -> [{id: 1, name: "Apple", price: 1.5} {id: 2, name: "Banana", price: 0.75}] *)

(* Use Joy's list operations on results *)
[price dget] map vsum.   (* total price of cheap items -> 2.25 *)

db-close.
```

---

## Operator Reference

| Operator | Stack Effect | Description |
|----------|--------------|-------------|
| `db-open` | `"path" -> db` | Open database |
| `db-close` | `db ->` | Close database |
| `db-exec` | `db "sql" [params] ->` | Execute statement |
| `db-query` | `db "sql" [params] -> [rows]` | Query, returns list of dicts |
| `db-begin` | `db ->` | Begin transaction |
| `db-commit` | `db ->` | Commit transaction |
| `db-rollback` | `db ->` | Rollback transaction |
| `db-last-id` | `db -> int` | Last insert rowid |
| `db-changes` | `db -> int` | Rows affected |

---

## Type Mapping

| SQLite Type | Joy Type |
|-------------|----------|
| INTEGER | integer |
| REAL | float |
| TEXT | string |
| BLOB | string (raw bytes) |
| NULL | null |

---

## Design Notes

1. **Parameterized queries** - All queries use `?` placeholders to prevent SQL injection
2. **Dict results** - Query results use Joy's dict type with column names as keys
3. **Handle-based** - Database connections are integer handles (index into static array)
4. **Transaction support** - Explicit begin/commit/rollback for atomic operations
5. **Integration with Joy** - Results work naturally with `map`, `filter`, dict operations

## Future Extensions

- `db-tables` - List all tables
- `db-schema` - Get table schema
- `db-prepare` / `db-bind` / `db-step` - Lower-level prepared statement API
- Blob support with separate read/write
- In-memory database sharing between connections
