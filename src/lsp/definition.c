/*
 *  module  : definition.c
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  textDocument/definition handler.
 *  Finds the definition of a symbol in the current document.
 */
#include "definition.h"
#include "document.h"
#include <string.h>
#include <stdlib.h>

JsonValue *handle_definition(const JsonValue *params) {
    const JsonValue *td = json_object_get(params, "textDocument");
    const char *uri = json_object_get_string(td, "uri");
    const JsonValue *pos = json_object_get(params, "position");
    uint32_t line = (uint32_t)json_object_get_int(pos, "line", 0);
    uint32_t col = (uint32_t)json_object_get_int(pos, "character", 0);

    Document *doc = uri ? doc_get(uri) : NULL;
    if (!doc) return json_null();

    char *sym = doc_symbol_at(doc, line, col);
    if (!sym) return json_null();

    /* Search document definitions */
    for (size_t i = 0; i < doc->defs_len; i++) {
        DocDefinition *d = &doc->defs[i];
        if (strcmp(d->name, sym) == 0) {
            JsonValue *result = json_object();
            json_object_set(result, "uri", json_string(uri));

            JsonValue *range = json_object();
            JsonValue *start = json_object();
            json_object_set(start, "line", json_int(d->start_row));
            json_object_set(start, "character", json_int(d->start_col));
            JsonValue *end = json_object();
            json_object_set(end, "line", json_int(d->end_row));
            json_object_set(end, "character", json_int(d->end_col));
            json_object_set(range, "start", start);
            json_object_set(range, "end", end);
            json_object_set(result, "range", range);

            free(sym);
            return result;
        }
    }

    free(sym);
    return json_null();
}
