/*
 *  module  : session.c
 *  version : 1.0
 *  date    : 01/24/26
 *
 *  Persistent session operations for Joy using SQLite.
 *  Guarded by JOY_SESSION compile-time option.
 */
#ifdef JOY_SESSION

#include "globals.h"
#include "runtime.h"
#include "builtin_macros.h"

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Schema SQL */
static const char* SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS symbols ("
    "    name TEXT PRIMARY KEY,"
    "    body TEXT NOT NULL,"
    "    type TEXT,"
    "    size INTEGER,"
    "    modified_at INTEGER,"
    "    created_at INTEGER"
    ");"
    "CREATE TABLE IF NOT EXISTS snapshots ("
    "    name TEXT,"
    "    symbol_name TEXT,"
    "    body TEXT,"
    "    created_at INTEGER,"
    "    PRIMARY KEY (name, symbol_name)"
    ");"
    "CREATE TABLE IF NOT EXISTS meta ("
    "    key TEXT PRIMARY KEY,"
    "    value TEXT"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_snapshots_name ON snapshots(name);";

/* Forward declarations */
static void ensure_schema(Session* s);
static void prepare_statements(Session* s);
static void load_symbols_from_db(pEnv env);
static char* serialize_value(pEnv env, Index body);
static Index deserialize_value(pEnv env, const char* str);
static void session_close_internal(pEnv env);
static void invalidate_cache(Session* s, const char* name);
static Index reverse_list(pEnv env, Index head);

/* Helper: initialize schema */
static void ensure_schema(Session* s)
{
    char* errmsg = NULL;
    /* Enable WAL mode for better concurrency */
    sqlite3_exec(s->db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    /* Create tables */
    if (sqlite3_exec(s->db, SCHEMA_SQL, NULL, NULL, &errmsg) != SQLITE_OK) {
        fprintf(stderr, "session: schema error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
}

/* Helper: prepare cached statements */
static void prepare_statements(Session* s)
{
    sqlite3_prepare_v2(s->db,
        "INSERT OR REPLACE INTO symbols (name, body, modified_at, created_at) "
        "VALUES (?, ?, ?, COALESCE((SELECT created_at FROM symbols WHERE name = ?), ?))",
        -1, &s->insert_symbol, NULL);

    sqlite3_prepare_v2(s->db,
        "SELECT body FROM symbols WHERE name = ?",
        -1, &s->select_symbol, NULL);

    sqlite3_prepare_v2(s->db,
        "DELETE FROM symbols WHERE name = ?",
        -1, &s->delete_symbol, NULL);

    sqlite3_prepare_v2(s->db,
        "SELECT name, body FROM symbols ORDER BY name",
        -1, &s->list_symbols, NULL);
}

/* Helper: serialize a Joy value to a string using writeterm */
static char* serialize_value(pEnv env, Index body)
{
    char* buf = NULL;
    size_t size = 0;
    FILE* stream = open_memstream(&buf, &size);
    if (!stream)
        return NULL;
    writeterm(env, body, stream);
    fclose(stream);
    return buf;
}

/* Helper: deserialize a string to a Joy value
 * For now, handles simple cases without full parser to avoid scanner corruption.
 * Complex values are stored as strings and parsed lazily.
 */
static Index deserialize_value(pEnv env, const char* str)
{
    if (!str || !*str)
        return 0;

    /* Skip leading whitespace */
    while (*str == ' ' || *str == '\t' || *str == '\n')
        str++;

    if (!*str)
        return 0;

    /* Try to parse as simple integer */
    char* end;
    int64_t num = strtoll(str, &end, 10);
    /* Skip trailing whitespace */
    while (*end == ' ' || *end == '\t' || *end == '\n')
        end++;
    if (*end == '\0') {
        /* It's a simple integer */
        return INTEGER_NEWNODE(num, 0);
    }

    /* Try to parse as simple float */
    double dbl = strtod(str, &end);
    while (*end == ' ' || *end == '\t' || *end == '\n')
        end++;
    if (*end == '\0') {
        /* It's a simple float */
        return FLOAT_NEWNODE(dbl, 0);
    }

    /* For now, store as string and parse lazily later */
    /* This avoids scanner corruption during load */
    return STRING_NEWNODE(GC_strdup(str), 0);
}

/* Helper: reverse a list */
static Index reverse_list(pEnv env, Index head)
{
    Index prev = 0;
    Index curr = head;
    Index next;

    while (curr) {
        next = nextnode1(curr);
#ifdef NOBDW
        env->memory[curr].next = prev;
#else
        curr->next = prev;
#endif
        prev = curr;
        curr = next;
    }
    return prev;
}

/* Helper: close session internals */
static void session_close_internal(pEnv env)
{
    Session* s = env->session;
    if (!s)
        return;

    if (s->insert_symbol)
        sqlite3_finalize(s->insert_symbol);
    if (s->select_symbol)
        sqlite3_finalize(s->select_symbol);
    if (s->delete_symbol)
        sqlite3_finalize(s->delete_symbol);
    if (s->list_symbols)
        sqlite3_finalize(s->list_symbols);

    if (s->cache)
        kh_destroy(Cache, s->cache);

    if (s->db)
        sqlite3_close(s->db);

    env->session = NULL;
}

/* Helper: invalidate cache entry */
static void invalidate_cache(Session* s, const char* name)
{
    if (!s || !s->cache)
        return;
    khint_t k = kh_get(Cache, s->cache, name);
    if (k != kh_end(s->cache)) {
        kh_del(Cache, s->cache, k);
        s->cache_size--;
    }
}

/* Helper: load all symbols from database into symbol table */
static void load_symbols_from_db(pEnv env)
{
    Session* s = env->session;
    if (!s || !s->db)
        return;

    sqlite3_reset(s->list_symbols);

    while (sqlite3_step(s->list_symbols) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(s->list_symbols, 0);
        const char* body_str = (const char*)sqlite3_column_text(s->list_symbols, 1);

        if (!name || !body_str)
            continue;

        /* Deserialize the body */
        Index body = deserialize_value(env, body_str);

        /* Add to symbol table */
        int index = lookup(env, (char*)name);
        if (index > 0) {
            Entry ent = vec_at(env->symtab, index);
            ent.is_user = 1;
            ent.u.body = body;
            vec_at(env->symtab, index) = ent;
        }
    }
}

/* Persist a symbol to the database - called from symbol.c */
void session_persist_symbol(pEnv env, const char* name, Index body)
{
    Session* s = env->session;
    if (!s || !s->persistent || !s->db)
        return;

    char* serialized = serialize_value(env, body);
    if (!serialized)
        return;

    int64_t now = (int64_t)time(NULL);

    sqlite3_reset(s->insert_symbol);
    sqlite3_bind_text(s->insert_symbol, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(s->insert_symbol, 2, serialized, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s->insert_symbol, 3, now);
    sqlite3_bind_text(s->insert_symbol, 4, name, -1, SQLITE_STATIC);
    sqlite3_bind_int64(s->insert_symbol, 5, now);

    if (sqlite3_step(s->insert_symbol) != SQLITE_DONE) {
        fprintf(stderr, "session: failed to persist '%s': %s\n",
                name, sqlite3_errmsg(s->db));
    }

    free(serialized);

    /* Invalidate cache for this symbol */
    invalidate_cache(s, name);
}

/**
Q0  OK  3200  session  :  "name"  ->
Opens or creates a persistent session.
All subsequent DEFINE operations will be persisted to disk.
[SESSION]
*/
void session_(pEnv env)
{
    char path[PATH_MAX];
    char* name;

    ONEPARAM("session");
    STRING("session");
    name = GC_strdup(GETSTRING(env->stck));
    POP(env->stck);

    /* Close existing session if any */
    if (env->session && env->session->persistent) {
        session_close_internal(env);
    }

    /* Create session */
    env->session = GC_malloc(sizeof(Session));
    memset(env->session, 0, sizeof(Session));
    env->session->name = GC_strdup(name);
    env->session->persistent = 1;
    env->session->autosave = 1;
    env->session->cache = kh_init(Cache);
    env->session->cache_limit = 1000;

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

/**
Q0  OK  3201  session-close\0session_close  :  ->
Closes the current persistent session.
[SESSION]
*/
void session_close_(pEnv env)
{
    if (!env->session) {
        printf("session: no active session\n");
        return;
    }

    printf("session: closed '%s'\n", env->session->name);
    session_close_internal(env);
}

/**
Q0  OK  3202  sessions  :  ->  [names]
Lists all available sessions (*.joy.db files in current directory).
[SESSION]
*/
void sessions_(pEnv env)
{
    Index result = 0;
    DIR* dir = opendir(".");
    struct dirent* entry;
    int count = 0;

    if (!dir) {
        NULLARY(LIST_NEWNODE, 0);
        return;
    }

    /* First pass: count entries for ensure_capacity */
    while ((entry = readdir(dir)) != NULL) {
        char* name = entry->d_name;
        size_t len = strlen(name);
        if (len > 7 && strcmp(name + len - 7, ".joy.db") == 0)
            count++;
    }

#ifdef NOBDW
    ensure_capacity(env, count);
#endif

    /* Second pass: collect session names */
    rewinddir(dir);
    while ((entry = readdir(dir)) != NULL) {
        char* name = entry->d_name;
        size_t len = strlen(name);

        /* Check for .joy.db suffix */
        if (len > 7 && strcmp(name + len - 7, ".joy.db") == 0) {
            /* Extract session name (remove suffix) */
            char* session_name = GC_malloc(len - 6);
            memcpy(session_name, name, len - 7);
            session_name[len - 7] = '\0';

            result = STRING_NEWNODE(session_name, result);
        }
    }
    closedir(dir);

    result = reverse_list(env, result);
    NULLARY(LIST_NEWNODE, result);
}

/**
Q0  OK  3210  snapshot  :  "name"  ->
Saves current session state as named snapshot.
[SESSION]
*/
void snapshot_(pEnv env)
{
    char* name;
    Session* s = env->session;
    char* sql;

    ONEPARAM("snapshot");
    STRING("snapshot");
    name = GETSTRING(env->stck);
    POP(env->stck);

    if (!s || !s->persistent) {
        execerror(env, "no persistent session", "snapshot");
        return;
    }

    /* Delete existing snapshot with same name */
    sql = sqlite3_mprintf("DELETE FROM snapshots WHERE name = %Q", name);
    sqlite3_exec(s->db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    /* Copy current symbols into snapshot */
    sql = sqlite3_mprintf(
        "INSERT INTO snapshots (name, symbol_name, body, created_at) "
        "SELECT %Q, name, body, %lld FROM symbols",
        name, (long long)time(NULL));
    sqlite3_exec(s->db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    printf("session: snapshot '%s' created\n", name);
}

/**
Q0  OK  3211  restore  :  "name"  ->
Restores session state from named snapshot.
[SESSION]
*/
void restore_(pEnv env)
{
    char* name;
    Session* s = env->session;
    char* sql;
    sqlite3_stmt* stmt;
    int count = 0;

    ONEPARAM("restore");
    STRING("restore");
    name = GETSTRING(env->stck);
    POP(env->stck);

    if (!s || !s->persistent) {
        execerror(env, "no persistent session", "restore");
        return;
    }

    /* Check snapshot exists */
    sql = sqlite3_mprintf(
        "SELECT COUNT(*) FROM snapshots WHERE name = %Q", name);
    sqlite3_prepare_v2(s->db, sql, -1, &stmt, NULL);
    sqlite3_free(sql);

    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (count == 0) {
        execerror(env, "snapshot not found", name);
        return;
    }

    /* Clear current symbols */
    sqlite3_exec(s->db, "DELETE FROM symbols", NULL, NULL, NULL);

    /* Restore from snapshot */
    sql = sqlite3_mprintf(
        "INSERT INTO symbols (name, body, modified_at, created_at) "
        "SELECT symbol_name, body, %lld, %lld FROM snapshots WHERE name = %Q",
        (long long)time(NULL), (long long)time(NULL), name);
    sqlite3_exec(s->db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    /* Clear cache and reload */
    kh_clear(Cache, s->cache);
    s->cache_size = 0;
    load_symbols_from_db(env);

    printf("session: restored to '%s'\n", name);
}

/**
Q0  OK  3212  snapshots  :  ->  [names]
Lists all available snapshots in current session.
[SESSION]
*/
void snapshots_(pEnv env)
{
    Session* s = env->session;
    Index result = 0;
    sqlite3_stmt* stmt;
    int count = 0;

    if (!s || !s->persistent) {
        NULLARY(LIST_NEWNODE, 0);
        return;
    }

    /* First pass: count entries */
    sqlite3_prepare_v2(s->db,
        "SELECT COUNT(DISTINCT name) FROM snapshots",
        -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

#ifdef NOBDW
    if (count > 0)
        ensure_capacity(env, count);
#endif

    /* Collect snapshot names */
    sqlite3_prepare_v2(s->db,
        "SELECT DISTINCT name FROM snapshots ORDER BY created_at DESC",
        -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        result = STRING_NEWNODE(GC_strdup(name), result);
    }
    sqlite3_finalize(stmt);

    result = reverse_list(env, result);
    NULLARY(LIST_NEWNODE, result);
}

/**
Q0  OK  3230  session-merge\0session_merge  :  "source"  ->  B
Merges source session into current. Returns false if conflicts exist.
[SESSION]
*/
void session_merge_(pEnv env)
{
    char* source_name;
    Session* target = env->session;
    char path[PATH_MAX];
    char* attach_sql;
    sqlite3_stmt* stmt;
    Index conflicts = 0;
    int conflict_count = 0;

    ONEPARAM("session-merge");
    STRING("session-merge");
    source_name = GETSTRING(env->stck);
    POP(env->stck);

    if (!target || !target->persistent) {
        execerror(env, "no persistent session", "session-merge");
        return;
    }

    /* Attach source database */
    snprintf(path, sizeof(path), "%s.joy.db", source_name);
    attach_sql = sqlite3_mprintf("ATTACH DATABASE %Q AS source", path);
    if (sqlite3_exec(target->db, attach_sql, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_free(attach_sql);
        execerror(env, "cannot open source session", source_name);
        return;
    }
    sqlite3_free(attach_sql);

    /* Find conflicts: same name, different body */
    sqlite3_prepare_v2(target->db,
        "SELECT s.name FROM source.symbols s "
        "JOIN symbols t ON s.name = t.name "
        "WHERE s.body != t.body",
        -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        conflicts = STRING_NEWNODE(GC_strdup(name), conflicts);
        conflict_count++;
    }
    sqlite3_finalize(stmt);

    if (conflict_count > 0) {
        /* Report conflicts, return false */
        printf("session-merge: conflicts on:");
        for (Index p = conflicts; p; p = nextnode1(p)) {
            printf(" %s", GETSTRING(p));
        }
        printf("\n");
        sqlite3_exec(target->db, "DETACH DATABASE source", NULL, NULL, NULL);
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

    /* Detach source */
    sqlite3_exec(target->db, "DETACH DATABASE source", NULL, NULL, NULL);

    /* Reload symbols */
    load_symbols_from_db(env);

    printf("session-merge: merged '%s' successfully\n", source_name);
    NULLARY(BOOLEAN_NEWNODE, 1);
}

/**
Q0  OK  3231  session-diff\0session_diff  :  "other"  ->  D
Shows differences between current session and another.
Returns dict with keys: added, modified, removed.
[SESSION]
*/
void session_diff_(pEnv env)
{
    char* other_name;
    Session* current = env->session;
    char path[PATH_MAX];
    char* attach_sql;
    sqlite3_stmt* stmt;
    Index added = 0, modified = 0, removed = 0;
    khash_t(Dict)* dict;
    khint_t k;
    int ret;

    ONEPARAM("session-diff");
    STRING("session-diff");
    other_name = GETSTRING(env->stck);
    POP(env->stck);

    if (!current || !current->persistent) {
        execerror(env, "no persistent session", "session-diff");
        return;
    }

    /* Attach other session */
    snprintf(path, sizeof(path), "%s.joy.db", other_name);
    attach_sql = sqlite3_mprintf("ATTACH DATABASE %Q AS other", path);
    if (sqlite3_exec(current->db, attach_sql, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_free(attach_sql);
        execerror(env, "cannot open other session", other_name);
        return;
    }
    sqlite3_free(attach_sql);

    /* Added: in other but not in current */
    sqlite3_prepare_v2(current->db,
        "SELECT name FROM other.symbols WHERE name NOT IN (SELECT name FROM symbols)",
        -1, &stmt, NULL);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        added = STRING_NEWNODE(GC_strdup((const char*)sqlite3_column_text(stmt, 0)), added);
    }
    sqlite3_finalize(stmt);

    /* Modified: in both but different */
    sqlite3_prepare_v2(current->db,
        "SELECT o.name FROM other.symbols o "
        "JOIN symbols s ON o.name = s.name WHERE o.body != s.body",
        -1, &stmt, NULL);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        modified = STRING_NEWNODE(GC_strdup((const char*)sqlite3_column_text(stmt, 0)), modified);
    }
    sqlite3_finalize(stmt);

    /* Removed: in current but not in other */
    sqlite3_prepare_v2(current->db,
        "SELECT name FROM symbols WHERE name NOT IN (SELECT name FROM other.symbols)",
        -1, &stmt, NULL);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        removed = STRING_NEWNODE(GC_strdup((const char*)sqlite3_column_text(stmt, 0)), removed);
    }
    sqlite3_finalize(stmt);

    sqlite3_exec(current->db, "DETACH DATABASE other", NULL, NULL, NULL);

    /* Build result dict */
    dict = kh_init(Dict);

    k = kh_put(Dict, dict, GC_strdup("added"), &ret);
    kh_val(dict, k) = LIST_NEWNODE(reverse_list(env, added), 0);

    k = kh_put(Dict, dict, GC_strdup("modified"), &ret);
    kh_val(dict, k) = LIST_NEWNODE(reverse_list(env, modified), 0);

    k = kh_put(Dict, dict, GC_strdup("removed"), &ret);
    kh_val(dict, k) = LIST_NEWNODE(reverse_list(env, removed), 0);

    NULLARY(DICT_NEWNODE, dict);
}

/**
Q0  OK  3232  session-take\0session_take  :  "source" "symbol"  ->
Takes a symbol from source session, overwriting current.
[SESSION]
*/
void session_take_(pEnv env)
{
    char* source_name;
    char* symbol_name;
    Session* current = env->session;
    char path[PATH_MAX];
    char* attach_sql;
    char* sql;

    TWOPARAMS("session-take");
    STRING("session-take");
    symbol_name = GC_strdup(GETSTRING(env->stck));
    POP(env->stck);

    STRING("session-take");
    source_name = GETSTRING(env->stck);
    POP(env->stck);

    if (!current || !current->persistent) {
        execerror(env, "no persistent session", "session-take");
        return;
    }

    /* Attach source */
    snprintf(path, sizeof(path), "%s.joy.db", source_name);
    attach_sql = sqlite3_mprintf("ATTACH DATABASE %Q AS source", path);
    if (sqlite3_exec(current->db, attach_sql, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_free(attach_sql);
        execerror(env, "cannot open source session", source_name);
        return;
    }
    sqlite3_free(attach_sql);

    /* Replace symbol */
    sql = sqlite3_mprintf(
        "INSERT OR REPLACE INTO symbols (name, body, type, size, created_at, modified_at) "
        "SELECT name, body, type, size, created_at, %lld FROM source.symbols WHERE name = %Q",
        (long long)time(NULL), symbol_name);
    sqlite3_exec(current->db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    sqlite3_exec(current->db, "DETACH DATABASE source", NULL, NULL, NULL);

    /* Reload that symbol */
    invalidate_cache(current, symbol_name);

    /* Reload the symbol from database */
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(current->db,
        "SELECT body FROM symbols WHERE name = ?",
        -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, symbol_name, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* body_str = (const char*)sqlite3_column_text(stmt, 0);
        Index body = deserialize_value(env, body_str);
        int index = lookup(env, symbol_name);
        if (index > 0) {
            Entry ent = vec_at(env->symtab, index);
            ent.is_user = 1;
            ent.u.body = body;
            vec_at(env->symtab, index) = ent;
        }
    }
    sqlite3_finalize(stmt);

    printf("session-take: took '%s' from '%s'\n", symbol_name, source_name);
}

/**
Q0  OK  3220  sql  :  "query" [params]  ->  [results]
Execute SQL query on session data.
Each result row becomes a dictionary in the returned list.
[SESSION]
*/
void sql_(pEnv env)
{
    char* query;
    Index params;
    Session* s = env->session;
    sqlite3_stmt* stmt;
    Index results = 0;
    int col_count;
    int param_idx;
    Index p;

    TWOPARAMS("sql");
    LIST("sql");
    params = nodevalue(env->stck).lis;
    POP(env->stck);

    STRING("sql");
    query = GETSTRING(env->stck);
    POP(env->stck);

    if (!s || !s->persistent) {
        execerror(env, "no persistent session", "sql");
        return;
    }

    if (sqlite3_prepare_v2(s->db, query, -1, &stmt, NULL) != SQLITE_OK) {
        execerror(env, (char*)sqlite3_errmsg(s->db), "sql");
        return;
    }

    /* Bind parameters */
    param_idx = 1;
    for (p = params; p; p = nextnode1(p), param_idx++) {
        switch (nodetype(p)) {
        case INTEGER_:
        case BOOLEAN_:
        case CHAR_:
            sqlite3_bind_int64(stmt, param_idx, nodevalue(p).num);
            break;
        case FLOAT_:
            sqlite3_bind_double(stmt, param_idx, nodevalue(p).dbl);
            break;
        case STRING_:
            sqlite3_bind_text(stmt, param_idx, GETSTRING(p), -1, SQLITE_TRANSIENT);
            break;
        default:
            /* Serialize other types as text */
            {
                char* serialized = serialize_value(env, p);
                sqlite3_bind_text(stmt, param_idx, serialized ? serialized : "", -1, SQLITE_TRANSIENT);
                if (serialized) free(serialized);
            }
            break;
        }
    }

    /* Collect results */
    col_count = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        /* Each row becomes a dict */
        khash_t(Dict)* dict = kh_init(Dict);
        int c;

        for (c = 0; c < col_count; c++) {
            const char* col_name = sqlite3_column_name(stmt, c);
            int type = sqlite3_column_type(stmt, c);
            Index val = 0;
            khint_t k;
            int ret;

            switch (type) {
            case SQLITE_INTEGER:
                val = INTEGER_NEWNODE(sqlite3_column_int64(stmt, c), 0);
                break;
            case SQLITE_FLOAT:
                val = FLOAT_NEWNODE(sqlite3_column_double(stmt, c), 0);
                break;
            case SQLITE_TEXT:
                val = STRING_NEWNODE(GC_strdup((const char*)sqlite3_column_text(stmt, c)), 0);
                break;
            case SQLITE_BLOB:
                /* Treat blob as string for now */
                val = STRING_NEWNODE(GC_strdup((const char*)sqlite3_column_blob(stmt, c)), 0);
                break;
            case SQLITE_NULL:
            default:
                val = LIST_NEWNODE(0, 0);  /* nil for NULL */
                break;
            }

            k = kh_put(Dict, dict, GC_strdup(col_name), &ret);
            kh_val(dict, k) = val;
        }

        results = DICT_NEWNODE(dict, results);
    }
    sqlite3_finalize(stmt);

    results = reverse_list(env, results);
    NULLARY(LIST_NEWNODE, results);
}

#endif /* JOY_SESSION */
