/*
 *  module  : symbols.c
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  textDocument/documentSymbol handler.
 *  Returns document outline from cached definitions.
 */
#include "symbols.h"
#include "document.h"
#include <string.h>

/* LSP SymbolKind */
#define SYMBOL_FUNCTION 12

JsonValue *handle_document_symbols(const JsonValue *params) {
    const JsonValue *td = json_object_get(params, "textDocument");
    const char *uri = json_object_get_string(td, "uri");

    Document *doc = uri ? doc_get(uri) : NULL;
    JsonValue *result = json_array();
    if (!doc) return result;

    for (size_t i = 0; i < doc->defs_len; i++) {
        DocDefinition *d = &doc->defs[i];

        JsonValue *sym = json_object();
        json_object_set(sym, "name", json_string(d->name));
        json_object_set(sym, "kind", json_int(SYMBOL_FUNCTION));

        JsonValue *location = json_object();
        json_object_set(location, "uri", json_string(uri));

        JsonValue *range = json_object();
        JsonValue *start = json_object();
        json_object_set(start, "line", json_int(d->start_row));
        json_object_set(start, "character", json_int(d->start_col));
        JsonValue *end = json_object();
        json_object_set(end, "line", json_int(d->end_row));
        json_object_set(end, "character", json_int(d->end_col));
        json_object_set(range, "start", start);
        json_object_set(range, "end", end);

        json_object_set(location, "range", range);
        json_object_set(sym, "location", location);

        json_array_push(result, sym);
    }

    return result;
}
