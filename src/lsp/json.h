/*
 *  module  : json.h
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  Lightweight JSON DOM for the LSP server.
 */
#ifndef LSP_JSON_H
#define LSP_JSON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_INT,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;

typedef struct JsonValue JsonValue;

/* Key-value pair for objects */
typedef struct {
    char *key;
    JsonValue *value;
} JsonMember;

struct JsonValue {
    JsonType type;
    union {
        bool boolean;
        int64_t integer;
        char *string;
        struct { JsonValue **items; size_t len; size_t cap; } array;
        struct { JsonMember *members; size_t len; size_t cap; } object;
    } u;
};

/* Constructors */
JsonValue *json_null(void);
JsonValue *json_bool(bool v);
JsonValue *json_int(int64_t v);
JsonValue *json_string(const char *s);
JsonValue *json_string_len(const char *s, size_t len);
JsonValue *json_array(void);
JsonValue *json_object(void);

/* Array operations */
void json_array_push(JsonValue *arr, JsonValue *val);

/* Object operations */
void json_object_set(JsonValue *obj, const char *key, JsonValue *val);
JsonValue *json_object_get(const JsonValue *obj, const char *key);
const char *json_object_get_string(const JsonValue *obj, const char *key);
int64_t json_object_get_int(const JsonValue *obj, const char *key, int64_t def);

/* Parsing */
JsonValue *json_parse(const char *input, size_t len);

/* Emitting */
typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} JsonBuf;

void json_buf_init(JsonBuf *b);
void json_buf_free(JsonBuf *b);
void json_emit(JsonBuf *b, const JsonValue *v);

/* Free */
void json_free(JsonValue *v);

#endif /* LSP_JSON_H */
