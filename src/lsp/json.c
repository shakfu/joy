/*
 *  module  : json.c
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  Lightweight JSON DOM for the LSP server.
 *  Recursive descent parser and string builder emitter.
 */
#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ---------- constructors ---------- */

static JsonValue *json_alloc(JsonType type) {
    JsonValue *v = calloc(1, sizeof(JsonValue));
    v->type = type;
    return v;
}

JsonValue *json_null(void) { return json_alloc(JSON_NULL); }

JsonValue *json_bool(bool b) {
    JsonValue *v = json_alloc(JSON_BOOL);
    v->u.boolean = b;
    return v;
}

JsonValue *json_int(int64_t n) {
    JsonValue *v = json_alloc(JSON_INT);
    v->u.integer = n;
    return v;
}

JsonValue *json_string(const char *s) {
    JsonValue *v = json_alloc(JSON_STRING);
    v->u.string = strdup(s);
    return v;
}

JsonValue *json_string_len(const char *s, size_t len) {
    JsonValue *v = json_alloc(JSON_STRING);
    v->u.string = malloc(len + 1);
    memcpy(v->u.string, s, len);
    v->u.string[len] = '\0';
    return v;
}

JsonValue *json_array(void) {
    JsonValue *v = json_alloc(JSON_ARRAY);
    v->u.array.cap = 8;
    v->u.array.items = malloc(sizeof(JsonValue *) * v->u.array.cap);
    v->u.array.len = 0;
    return v;
}

JsonValue *json_object(void) {
    JsonValue *v = json_alloc(JSON_OBJECT);
    v->u.object.cap = 8;
    v->u.object.members = malloc(sizeof(JsonMember) * v->u.object.cap);
    v->u.object.len = 0;
    return v;
}

/* ---------- array / object ops ---------- */

void json_array_push(JsonValue *arr, JsonValue *val) {
    if (arr->type != JSON_ARRAY) return;
    if (arr->u.array.len >= arr->u.array.cap) {
        arr->u.array.cap *= 2;
        arr->u.array.items = realloc(arr->u.array.items,
            sizeof(JsonValue *) * arr->u.array.cap);
    }
    arr->u.array.items[arr->u.array.len++] = val;
}

void json_object_set(JsonValue *obj, const char *key, JsonValue *val) {
    if (obj->type != JSON_OBJECT) return;
    /* Check for existing key */
    for (size_t i = 0; i < obj->u.object.len; i++) {
        if (strcmp(obj->u.object.members[i].key, key) == 0) {
            json_free(obj->u.object.members[i].value);
            obj->u.object.members[i].value = val;
            return;
        }
    }
    if (obj->u.object.len >= obj->u.object.cap) {
        obj->u.object.cap *= 2;
        obj->u.object.members = realloc(obj->u.object.members,
            sizeof(JsonMember) * obj->u.object.cap);
    }
    obj->u.object.members[obj->u.object.len].key = strdup(key);
    obj->u.object.members[obj->u.object.len].value = val;
    obj->u.object.len++;
}

JsonValue *json_object_get(const JsonValue *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    for (size_t i = 0; i < obj->u.object.len; i++) {
        if (strcmp(obj->u.object.members[i].key, key) == 0)
            return obj->u.object.members[i].value;
    }
    return NULL;
}

const char *json_object_get_string(const JsonValue *obj, const char *key) {
    JsonValue *v = json_object_get(obj, key);
    if (v && v->type == JSON_STRING) return v->u.string;
    return NULL;
}

int64_t json_object_get_int(const JsonValue *obj, const char *key, int64_t def) {
    JsonValue *v = json_object_get(obj, key);
    if (v && v->type == JSON_INT) return v->u.integer;
    return def;
}

/* ---------- free ---------- */

void json_free(JsonValue *v) {
    if (!v) return;
    switch (v->type) {
    case JSON_STRING:
        free(v->u.string);
        break;
    case JSON_ARRAY:
        for (size_t i = 0; i < v->u.array.len; i++)
            json_free(v->u.array.items[i]);
        free(v->u.array.items);
        break;
    case JSON_OBJECT:
        for (size_t i = 0; i < v->u.object.len; i++) {
            free(v->u.object.members[i].key);
            json_free(v->u.object.members[i].value);
        }
        free(v->u.object.members);
        break;
    default:
        break;
    }
    free(v);
}

/* ---------- parser ---------- */

typedef struct {
    const char *p;
    const char *end;
} Parser;

static void skip_ws(Parser *ps) {
    while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t' ||
           *ps->p == '\n' || *ps->p == '\r'))
        ps->p++;
}

static bool match_char(Parser *ps, char c) {
    skip_ws(ps);
    if (ps->p < ps->end && *ps->p == c) {
        ps->p++;
        return true;
    }
    return false;
}

static JsonValue *parse_value(Parser *ps);

static char *parse_string_value(Parser *ps) {
    if (ps->p >= ps->end || *ps->p != '"') return NULL;
    ps->p++; /* skip opening quote */

    /* Estimate length */
    size_t cap = 64;
    char *buf = malloc(cap);
    size_t len = 0;

    while (ps->p < ps->end && *ps->p != '"') {
        if (len + 6 >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        if (*ps->p == '\\') {
            ps->p++;
            if (ps->p >= ps->end) break;
            switch (*ps->p) {
            case '"':  buf[len++] = '"';  break;
            case '\\': buf[len++] = '\\'; break;
            case '/':  buf[len++] = '/';  break;
            case 'b':  buf[len++] = '\b'; break;
            case 'f':  buf[len++] = '\f'; break;
            case 'n':  buf[len++] = '\n'; break;
            case 'r':  buf[len++] = '\r'; break;
            case 't':  buf[len++] = '\t'; break;
            case 'u':
                /* Simplified: decode basic ASCII range, else '?' */
                if (ps->p + 4 < ps->end) {
                    unsigned int cp = 0;
                    for (int i = 1; i <= 4; i++) {
                        char c = ps->p[i];
                        cp <<= 4;
                        if (c >= '0' && c <= '9') cp |= (unsigned)(c - '0');
                        else if (c >= 'a' && c <= 'f') cp |= (unsigned)(c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') cp |= (unsigned)(c - 'A' + 10);
                    }
                    if (cp < 0x80) {
                        buf[len++] = (char)cp;
                    } else if (cp < 0x800) {
                        buf[len++] = (char)(0xC0 | (cp >> 6));
                        buf[len++] = (char)(0x80 | (cp & 0x3F));
                    } else {
                        buf[len++] = (char)(0xE0 | (cp >> 12));
                        buf[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        buf[len++] = (char)(0x80 | (cp & 0x3F));
                    }
                    ps->p += 4;
                }
                break;
            default:
                buf[len++] = *ps->p;
            }
            ps->p++;
        } else {
            buf[len++] = *ps->p;
            ps->p++;
        }
    }
    if (ps->p < ps->end && *ps->p == '"')
        ps->p++; /* skip closing quote */

    buf[len] = '\0';
    return buf;
}

static JsonValue *parse_string(Parser *ps) {
    char *s = parse_string_value(ps);
    if (!s) return NULL;
    JsonValue *v = json_alloc(JSON_STRING);
    v->u.string = s;
    return v;
}

static JsonValue *parse_number(Parser *ps) {
    const char *start = ps->p;
    if (*ps->p == '-') ps->p++;
    while (ps->p < ps->end && *ps->p >= '0' && *ps->p <= '9') ps->p++;
    /* We only need integers for LSP */
    char *end;
    int64_t val = strtoll(start, &end, 10);
    return json_int(val);
}

static JsonValue *parse_array(Parser *ps) {
    JsonValue *arr = json_array();
    ps->p++; /* skip [ */
    skip_ws(ps);
    if (ps->p < ps->end && *ps->p == ']') {
        ps->p++;
        return arr;
    }
    for (;;) {
        JsonValue *item = parse_value(ps);
        if (!item) break;
        json_array_push(arr, item);
        skip_ws(ps);
        if (ps->p < ps->end && *ps->p == ',') {
            ps->p++;
        } else {
            break;
        }
    }
    match_char(ps, ']');
    return arr;
}

static JsonValue *parse_object(Parser *ps) {
    JsonValue *obj = json_object();
    ps->p++; /* skip { */
    skip_ws(ps);
    if (ps->p < ps->end && *ps->p == '}') {
        ps->p++;
        return obj;
    }
    for (;;) {
        skip_ws(ps);
        char *key = parse_string_value(ps);
        if (!key) break;
        skip_ws(ps);
        if (ps->p < ps->end && *ps->p == ':') ps->p++;
        JsonValue *val = parse_value(ps);
        if (!val) { free(key); break; }
        /* Transfer ownership of key */
        if (obj->u.object.len >= obj->u.object.cap) {
            obj->u.object.cap *= 2;
            obj->u.object.members = realloc(obj->u.object.members,
                sizeof(JsonMember) * obj->u.object.cap);
        }
        obj->u.object.members[obj->u.object.len].key = key;
        obj->u.object.members[obj->u.object.len].value = val;
        obj->u.object.len++;

        skip_ws(ps);
        if (ps->p < ps->end && *ps->p == ',') {
            ps->p++;
        } else {
            break;
        }
    }
    match_char(ps, '}');
    return obj;
}

static JsonValue *parse_value(Parser *ps) {
    skip_ws(ps);
    if (ps->p >= ps->end) return NULL;

    switch (*ps->p) {
    case '"': return parse_string(ps);
    case '{': return parse_object(ps);
    case '[': return parse_array(ps);
    case 't':
        if (ps->p + 4 <= ps->end && memcmp(ps->p, "true", 4) == 0) {
            ps->p += 4;
            return json_bool(true);
        }
        return NULL;
    case 'f':
        if (ps->p + 5 <= ps->end && memcmp(ps->p, "false", 5) == 0) {
            ps->p += 5;
            return json_bool(false);
        }
        return NULL;
    case 'n':
        if (ps->p + 4 <= ps->end && memcmp(ps->p, "null", 4) == 0) {
            ps->p += 4;
            return json_null();
        }
        return NULL;
    default:
        if (*ps->p == '-' || (*ps->p >= '0' && *ps->p <= '9'))
            return parse_number(ps);
        return NULL;
    }
}

JsonValue *json_parse(const char *input, size_t len) {
    Parser ps = { input, input + len };
    return parse_value(&ps);
}

/* ---------- emitter ---------- */

void json_buf_init(JsonBuf *b) {
    b->cap = 256;
    b->buf = malloc(b->cap);
    b->len = 0;
}

void json_buf_free(JsonBuf *b) {
    free(b->buf);
    b->buf = NULL;
    b->len = b->cap = 0;
}

static void buf_grow(JsonBuf *b, size_t need) {
    while (b->len + need >= b->cap) {
        b->cap *= 2;
        b->buf = realloc(b->buf, b->cap);
    }
}

static void buf_append(JsonBuf *b, const char *s, size_t n) {
    buf_grow(b, n);
    memcpy(b->buf + b->len, s, n);
    b->len += n;
}

static void buf_char(JsonBuf *b, char c) {
    buf_grow(b, 1);
    b->buf[b->len++] = c;
}

static void emit_string(JsonBuf *b, const char *s) {
    buf_char(b, '"');
    for (; *s; s++) {
        switch (*s) {
        case '"':  buf_append(b, "\\\"", 2); break;
        case '\\': buf_append(b, "\\\\", 2); break;
        case '\b': buf_append(b, "\\b", 2);  break;
        case '\f': buf_append(b, "\\f", 2);  break;
        case '\n': buf_append(b, "\\n", 2);  break;
        case '\r': buf_append(b, "\\r", 2);  break;
        case '\t': buf_append(b, "\\t", 2);  break;
        default:
            if ((unsigned char)*s < 0x20) {
                char esc[8];
                int n = snprintf(esc, sizeof(esc), "\\u%04x", (unsigned char)*s);
                buf_append(b, esc, (size_t)n);
            } else {
                buf_char(b, *s);
            }
        }
    }
    buf_char(b, '"');
}

void json_emit(JsonBuf *b, const JsonValue *v) {
    if (!v) { buf_append(b, "null", 4); return; }
    switch (v->type) {
    case JSON_NULL:
        buf_append(b, "null", 4);
        break;
    case JSON_BOOL:
        if (v->u.boolean)
            buf_append(b, "true", 4);
        else
            buf_append(b, "false", 5);
        break;
    case JSON_INT: {
        char num[32];
        int n = snprintf(num, sizeof(num), "%lld", (long long)v->u.integer);
        buf_append(b, num, (size_t)n);
        break;
    }
    case JSON_STRING:
        emit_string(b, v->u.string);
        break;
    case JSON_ARRAY:
        buf_char(b, '[');
        for (size_t i = 0; i < v->u.array.len; i++) {
            if (i > 0) buf_char(b, ',');
            json_emit(b, v->u.array.items[i]);
        }
        buf_char(b, ']');
        break;
    case JSON_OBJECT:
        buf_char(b, '{');
        for (size_t i = 0; i < v->u.object.len; i++) {
            if (i > 0) buf_char(b, ',');
            emit_string(b, v->u.object.members[i].key);
            buf_char(b, ':');
            json_emit(b, v->u.object.members[i].value);
        }
        buf_char(b, '}');
        break;
    }
}
