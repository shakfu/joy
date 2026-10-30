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

/*
 * Strict RFC 8259 parser. Input is untrusted (stdin), so nesting is capped
 * to bound recursion, and any syntax error rejects the whole message.
 */
#define JSON_MAX_DEPTH 64

typedef struct {
    const char *p;
    const char *end;
    int depth;
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

/* Read 4 hex digits after "\u"; -1 if missing or invalid */
static long parse_hex4(Parser *ps) {
    long cp = 0;

    if (ps->end - ps->p < 4)
        return -1;
    for (int i = 0; i < 4; i++) {
        char c = ps->p[i];
        cp <<= 4;
        if (c >= '0' && c <= '9') cp |= c - '0';
        else if (c >= 'a' && c <= 'f') cp |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') cp |= c - 'A' + 10;
        else return -1;
    }
    ps->p += 4;
    return cp;
}

/* Append code point cp as UTF-8; buf has room for 4 bytes */
static size_t put_utf8(char *buf, unsigned long cp) {
    if (cp < 0x80) {
        buf[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    buf[0] = (char)(0xF0 | (cp >> 18));
    buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* Decode the code point after "\u"; a lone surrogate becomes U+FFFD */
static long parse_unicode_escape(Parser *ps) {
    long cp = parse_hex4(ps), lo;

    if (cp < 0)
        return -1;
    if (cp >= 0xDC00 && cp <= 0xDFFF)
        return 0xFFFD;
    if (cp < 0xD800 || cp > 0xDBFF)
        return cp;
    /* High surrogate: combine with a following \uDC00-\uDFFF */
    if (ps->end - ps->p >= 2 && ps->p[0] == '\\' && ps->p[1] == 'u') {
        const char *save = ps->p;
        ps->p += 2;
        lo = parse_hex4(ps);
        if (lo >= 0xDC00 && lo <= 0xDFFF)
            return 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        ps->p = save;
    }
    return 0xFFFD;
}

static char *parse_string_value(Parser *ps) {
    if (ps->p >= ps->end || *ps->p != '"') return NULL;
    ps->p++; /* skip opening quote */

    size_t cap = 64;
    char *buf = malloc(cap);
    size_t len = 0;

    for (;;) {
        if (ps->p >= ps->end)
            goto fail; /* unterminated */
        unsigned char c = (unsigned char)*ps->p++;
        if (c == '"')
            break;
        if (c < 0x20)
            goto fail; /* raw control characters must be escaped */
        if (len + 4 >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        if (c != '\\') {
            buf[len++] = (char)c;
            continue;
        }
        if (ps->p >= ps->end)
            goto fail;
        switch (*ps->p++) {
        case '"':  buf[len++] = '"';  break;
        case '\\': buf[len++] = '\\'; break;
        case '/':  buf[len++] = '/';  break;
        case 'b':  buf[len++] = '\b'; break;
        case 'f':  buf[len++] = '\f'; break;
        case 'n':  buf[len++] = '\n'; break;
        case 'r':  buf[len++] = '\r'; break;
        case 't':  buf[len++] = '\t'; break;
        case 'u': {
            long cp = parse_unicode_escape(ps);
            if (cp < 0)
                goto fail;
            len += put_utf8(buf + len, (unsigned long)cp);
            break;
        }
        default:
            goto fail; /* unknown escape */
        }
    }
    buf[len] = '\0';
    return buf;

fail:
    free(buf);
    return NULL;
}

static JsonValue *parse_string(Parser *ps) {
    char *s = parse_string_value(ps);
    if (!s) return NULL;
    JsonValue *v = json_alloc(JSON_STRING);
    v->u.string = s;
    return v;
}

static bool is_digit(Parser *ps) {
    return ps->p < ps->end && *ps->p >= '0' && *ps->p <= '9';
}

/*
 * Numbers follow the JSON grammar. LSP only sends integers that matter here,
 * so a fraction or exponent is accepted but truncated to an integer.
 */
static JsonValue *parse_number(Parser *ps) {
    char tmp[64];
    const char *start = ps->p;
    bool integral = true;

    if (*ps->p == '-') ps->p++;
    if (!is_digit(ps)) return NULL;
    if (*ps->p == '0') ps->p++;
    else while (is_digit(ps)) ps->p++;
    if (ps->p < ps->end && *ps->p == '.') {
        ps->p++;
        if (!is_digit(ps)) return NULL;
        while (is_digit(ps)) ps->p++;
        integral = false;
    }
    if (ps->p < ps->end && (*ps->p == 'e' || *ps->p == 'E')) {
        ps->p++;
        if (ps->p < ps->end && (*ps->p == '+' || *ps->p == '-')) ps->p++;
        if (!is_digit(ps)) return NULL;
        while (is_digit(ps)) ps->p++;
        integral = false;
    }
    /* The input is not NUL-terminated: convert from a bounded copy */
    size_t n = (size_t)(ps->p - start);
    if (n >= sizeof(tmp)) return NULL;
    memcpy(tmp, start, n);
    tmp[n] = '\0';
    if (integral)
        return json_int(strtoll(tmp, NULL, 10));
    double d = strtod(tmp, NULL);
    if (!(d > -9.2e18 && d < 9.2e18)) return NULL; /* out of int64 range */
    return json_int((int64_t)d);
}

static JsonValue *parse_array(Parser *ps) {
    JsonValue *arr = json_array();
    ps->p++; /* skip [ */
    if (match_char(ps, ']'))
        return arr;
    for (;;) {
        JsonValue *item = parse_value(ps);
        if (!item) goto fail;
        json_array_push(arr, item);
        if (match_char(ps, ','))
            continue;
        if (match_char(ps, ']'))
            return arr;
        goto fail;
    }
fail:
    json_free(arr);
    return NULL;
}

static JsonValue *parse_object(Parser *ps) {
    JsonValue *obj = json_object();
    ps->p++; /* skip { */
    if (match_char(ps, '}'))
        return obj;
    for (;;) {
        skip_ws(ps);
        char *key = parse_string_value(ps);
        if (!key) goto fail;
        if (!match_char(ps, ':')) { free(key); goto fail; }
        JsonValue *val = parse_value(ps);
        if (!val) { free(key); goto fail; }
        /* Transfer ownership of key */
        if (obj->u.object.len >= obj->u.object.cap) {
            obj->u.object.cap *= 2;
            obj->u.object.members = realloc(obj->u.object.members,
                sizeof(JsonMember) * obj->u.object.cap);
        }
        obj->u.object.members[obj->u.object.len].key = key;
        obj->u.object.members[obj->u.object.len].value = val;
        obj->u.object.len++;

        if (match_char(ps, ','))
            continue;
        if (match_char(ps, '}'))
            return obj;
        goto fail;
    }
fail:
    json_free(obj);
    return NULL;
}

static JsonValue *parse_value(Parser *ps) {
    JsonValue *v;

    skip_ws(ps);
    if (ps->p >= ps->end) return NULL;

    switch (*ps->p) {
    case '"': return parse_string(ps);
    case '{':
    case '[':
        if (ps->depth >= JSON_MAX_DEPTH)
            return NULL;
        ps->depth++;
        v = *ps->p == '{' ? parse_object(ps) : parse_array(ps);
        ps->depth--;
        return v;
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

/* Parse one JSON value spanning the whole input; NULL on any error */
JsonValue *json_parse(const char *input, size_t len) {
    Parser ps = { input, input + len, 0 };
    JsonValue *v = parse_value(&ps);

    skip_ws(&ps);
    if (v && ps.p != ps.end) {
        json_free(v); /* trailing garbage */
        return NULL;
    }
    return v;
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
