/*
 *  module  : dict.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Dictionary operations for Joy.
 *  Dictionaries are hash maps with string keys and arbitrary Joy values.
 */
#include "globals.h"
#include "runtime.h"
#include "builtin_macros.h"

/* Helper to check if top of stack is a dictionary */
#define DICT(NAME)                                                            \
    if (nodetype(env->stck) != DICT_) {                                       \
        execerror(env, "dictionary", NAME);                                   \
        return;                                                               \
    }

#define DICT2(NAME)                                                           \
    if (nodetype(nextnode1(env->stck)) != DICT_) {                            \
        execerror(env, "dictionary as second parameter", NAME);               \
        return;                                                               \
    }

#define DICT3(NAME)                                                           \
    if (nodetype(nextnode2(env->stck)) != DICT_) {                            \
        execerror(env, "dictionary as third parameter", NAME);                \
        return;                                                               \
    }

/* Helper to create a new empty dictionary */
static khash_t(Dict)* dict_new(void)
{
    return kh_init(Dict);
}

/* Helper to copy a dictionary (shallow copy - values are shared) */
static khash_t(Dict)* dict_copy(pEnv env, khash_t(Dict)* src)
{
    khash_t(Dict)* dst;
    khint_t k;
    int ret;

    (void)env;
    dst = kh_init(Dict);
    if (!src)
        return dst;

    for (k = kh_begin(src); k != kh_end(src); ++k) {
        if (kh_exist(src, k)) {
            khint_t key = kh_put(Dict, dst, GC_strdup(kh_key(src, k)), &ret);
            kh_value(dst, key) = kh_value(src, k);
        }
    }
    return dst;
}

/**
Q0  OK  3800  dempty  :  ->  D
D is an empty dictionary.
*/
void dempty_(pEnv env)
{
    khash_t(Dict)* d = dict_new();
    NULLARY(DICT_NEWNODE, d);
}

/**
Q0  OK  3801  dput  :  D K V  ->  D'
D' is dictionary D with key K (a string) set to value V.
*/
void dput_(pEnv env)
{
    khash_t(Dict) *d, *d2;
    char* key;
    Index value;
    khint_t k;
    int ret;

    THREEPARAMS("dput");
    DICT3("dput");
    STRING2("dput");

    value = env->stck;
    key = GC_strdup(GETSTRING(nextnode1(env->stck)));
    d = (khash_t(Dict)*)nodevalue(nextnode2(env->stck)).dict;

    /* Create a copy for immutable semantics */
    d2 = dict_copy(env, d);

    k = kh_put(Dict, d2, key, &ret);
    kh_value(d2, k) = newnode2(env, value, 0);

    env->stck = DICT_NEWNODE(d2, nextnode3(env->stck));
}

/**
Q0  OK  3802  dget  :  D K  ->  V
V is the value associated with key K in dictionary D.
Error if key is not present.
*/
void dget_(pEnv env)
{
    khash_t(Dict)* d;
    char* key;
    khint_t k;

    TWOPARAMS("dget");
    STRING("dget");
    DICT2("dget");

    key = GETSTRING(env->stck);
    d = (khash_t(Dict)*)nodevalue(nextnode1(env->stck)).dict;

    if (!d) {
        execerror(env, "key not found in dictionary", "dget");
        return;
    }

    k = kh_get(Dict, d, key);
    if (k == kh_end(d)) {
        execerror(env, "key not found in dictionary", "dget");
        return;
    }

    GBINARY(kh_value(d, k));
}

/**
Q0  OK  3803  dhas  :  D K  ->  B
B is true if dictionary D contains key K, false otherwise.
*/
void dhas_(pEnv env)
{
    khash_t(Dict)* d;
    char* key;
    khint_t k;
    int found;

    TWOPARAMS("dhas");
    STRING("dhas");
    DICT2("dhas");

    key = GETSTRING(env->stck);
    d = (khash_t(Dict)*)nodevalue(nextnode1(env->stck)).dict;

    if (!d) {
        found = 0;
    } else {
        k = kh_get(Dict, d, key);
        found = (k != kh_end(d));
    }

    BINARY(BOOLEAN_NEWNODE, found);
}

/**
Q0  OK  3804  ddel  :  D K  ->  D'
D' is dictionary D with key K removed.
No error if key was not present.
*/
void ddel_(pEnv env)
{
    khash_t(Dict) *d, *d2;
    char* key;
    khint_t k;

    TWOPARAMS("ddel");
    STRING("ddel");
    DICT2("ddel");

    key = GETSTRING(env->stck);
    d = (khash_t(Dict)*)nodevalue(nextnode1(env->stck)).dict;

    /* Create a copy for immutable semantics */
    d2 = dict_copy(env, d);

    k = kh_get(Dict, d2, key);
    if (k != kh_end(d2)) {
        kh_del(Dict, d2, k);
    }

    BINARY(DICT_NEWNODE, d2);
}

/**
Q0  OK  3805  dkeys  :  D  ->  L
L is a list of all keys in dictionary D.
*/
void dkeys_(pEnv env)
{
    khash_t(Dict)* d;
    khint_t k;
    Index head = 0, tail = 0, node;
    int count = 0;

    ONEPARAM("dkeys");
    DICT("dkeys");

    d = (khash_t(Dict)*)nodevalue(env->stck).dict;

    if (d) {
        /* Count keys first for ensure_capacity */
        for (k = kh_begin(d); k != kh_end(d); ++k)
            if (kh_exist(d, k))
                count++;

        if (count > 0) {
#ifdef NOBDW
            ensure_capacity(env, count);
#endif
            for (k = kh_begin(d); k != kh_end(d); ++k) {
                if (kh_exist(d, k)) {
                    node = STRING_NEWNODE(GC_strdup(kh_key(d, k)), 0);
                    if (!head) {
                        head = node;
                        tail = node;
                    } else {
                        nextnode1(tail) = node;
                        tail = node;
                    }
                }
            }
        }
    }

    UNARY(LIST_NEWNODE, head);
}

/**
Q0  OK  3806  dvals  :  D  ->  L
L is a list of all values in dictionary D.
*/
void dvals_(pEnv env)
{
    khash_t(Dict)* d;
    khint_t k;
    Index head = 0, tail = 0, node;
    int count = 0;

    ONEPARAM("dvals");
    DICT("dvals");

    d = (khash_t(Dict)*)nodevalue(env->stck).dict;

    if (d) {
        /* Count values first for ensure_capacity */
        for (k = kh_begin(d); k != kh_end(d); ++k)
            if (kh_exist(d, k))
                count++;

        if (count > 0) {
#ifdef NOBDW
            ensure_capacity(env, count);
#endif
            for (k = kh_begin(d); k != kh_end(d); ++k) {
                if (kh_exist(d, k)) {
                    node = newnode2(env, kh_value(d, k), 0);
                    if (!head) {
                        head = node;
                        tail = node;
                    } else {
                        nextnode1(tail) = node;
                        tail = node;
                    }
                }
            }
        }
    }

    UNARY(LIST_NEWNODE, head);
}

/**
Q0  OK  3807  dsize  :  D  ->  I
I is the number of key-value pairs in dictionary D.
*/
void dsize_(pEnv env)
{
    khash_t(Dict)* d;
    int64_t size = 0;

    ONEPARAM("dsize");
    DICT("dsize");

    d = (khash_t(Dict)*)nodevalue(env->stck).dict;
    if (d)
        size = kh_size(d);

    UNARY(INTEGER_NEWNODE, size);
}

/**
Q0  OK  3808  >dict\0todict  :  L  ->  D
D is a dictionary created from association list L.
L should be a list of [key value] pairs where key is a string.
*/
void todict_(pEnv env)
{
    khash_t(Dict)* d;
    Index lis, pair, key_node, val_node;
    char* key;
    khint_t k;
    int ret;

    ONEPARAM(">dict");
    ONEQUOTE(">dict");

    d = dict_new();
    lis = nodevalue(env->stck).lis;

    while (lis) {
        if (nodetype(lis) != LIST_) {
            execerror(env, "list of [key value] pairs", ">dict");
            return;
        }
        pair = nodevalue(lis).lis;
        if (!pair || !nextnode1(pair)) {
            execerror(env, "[key value] pair with two elements", ">dict");
            return;
        }
        key_node = pair;
        val_node = nextnode1(pair);

        if (nodetype(key_node) != STRING_) {
            execerror(env, "string as key in [key value] pair", ">dict");
            return;
        }

        key = GC_strdup(GETSTRING(key_node));
        k = kh_put(Dict, d, key, &ret);
        kh_value(d, k) = newnode2(env, val_node, 0);

        lis = nextnode1(lis);
    }

    UNARY(DICT_NEWNODE, d);
}

/**
Q0  OK  3809  dict>\0fromdict  :  D  ->  L
L is an association list created from dictionary D.
Each element of L is a [key value] pair.
*/
void fromdict_(pEnv env)
{
    khash_t(Dict)* d;
    khint_t k;
    Index head = 0, tail = 0, node, pair_head, key_node, val_node;
    int count = 0;

    ONEPARAM("dict>");
    DICT("dict>");

    d = (khash_t(Dict)*)nodevalue(env->stck).dict;

    if (d) {
        /* Count entries first for ensure_capacity */
        for (k = kh_begin(d); k != kh_end(d); ++k)
            if (kh_exist(d, k))
                count++;

        if (count > 0) {
#ifdef NOBDW
            ensure_capacity(env, count * 4);  /* Each entry needs 4 nodes: list node, pair, key, val */
#endif
            for (k = kh_begin(d); k != kh_end(d); ++k) {
                if (kh_exist(d, k)) {
                    /* Create [key value] pair */
                    key_node = STRING_NEWNODE(GC_strdup(kh_key(d, k)), 0);
                    val_node = newnode2(env, kh_value(d, k), 0);
                    nextnode1(key_node) = val_node;
                    pair_head = key_node;

                    /* Wrap in list node */
                    node = LIST_NEWNODE(pair_head, 0);

                    if (!head) {
                        head = node;
                        tail = node;
                    } else {
                        nextnode1(tail) = node;
                        tail = node;
                    }
                }
            }
        }
    }

    UNARY(LIST_NEWNODE, head);
}

/**
Q0  OK  3810  dict  :  X  ->  B
B is true if X is a dictionary, false otherwise.
*/
void dict_(pEnv env)
{
    ONEPARAM("dict");
    UNARY(BOOLEAN_NEWNODE, nodetype(env->stck) == DICT_);
}

/**
Q0  OK  3811  dmerge  :  D1 D2  ->  D3
D3 is the merge of dictionaries D1 and D2.
If a key exists in both, the value from D2 is used.
*/
void dmerge_(pEnv env)
{
    khash_t(Dict) *d1, *d2, *d3;
    khint_t k;
    int ret;

    TWOPARAMS("dmerge");
    DICT("dmerge");
    DICT2("dmerge");

    d2 = (khash_t(Dict)*)nodevalue(env->stck).dict;
    d1 = (khash_t(Dict)*)nodevalue(nextnode1(env->stck)).dict;

    /* Start with a copy of d1 */
    d3 = dict_copy(env, d1);

    /* Merge in d2 (overwrites existing keys) */
    if (d2) {
        for (k = kh_begin(d2); k != kh_end(d2); ++k) {
            if (kh_exist(d2, k)) {
                khint_t key = kh_put(Dict, d3, GC_strdup(kh_key(d2, k)), &ret);
                kh_value(d3, key) = kh_value(d2, k);
            }
        }
    }

    BINARY(DICT_NEWNODE, d3);
}

/**
Q0  OK  3812  dgetd  :  D K V  ->  V'
V' is the value associated with key K in dictionary D,
or default value V if the key is not present.
*/
void dgetd_(pEnv env)
{
    khash_t(Dict)* d;
    char* key;
    khint_t k;
    Index defval;

    THREEPARAMS("dgetd");
    STRING2("dgetd");
    DICT3("dgetd");

    defval = env->stck;
    key = GETSTRING(nextnode1(env->stck));
    d = (khash_t(Dict)*)nodevalue(nextnode2(env->stck)).dict;

    if (!d) {
        /* Empty dict, return default */
        env->stck = newnode2(env, defval, nextnode3(env->stck));
        return;
    }

    k = kh_get(Dict, d, key);
    if (k == kh_end(d)) {
        /* Key not found, return default */
        env->stck = newnode2(env, defval, nextnode3(env->stck));
        return;
    }

    GTERNARY(kh_value(d, k));
}
