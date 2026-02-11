/*
 *  module  : completion.c
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  textDocument/completion handler.
 *  Prefix-matches against builtins and document definitions.
 */
#include "completion.h"
#include "document.h"
#include "lsp_builtins.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* LSP CompletionItemKind constants */
#define COMPLETION_FUNCTION  3
#define COMPLETION_VARIABLE 6
#define COMPLETION_KEYWORD  14

JsonValue *handle_completion(const JsonValue *params) {
    const JsonValue *td = json_object_get(params, "textDocument");
    const char *uri = json_object_get_string(td, "uri");
    const JsonValue *pos = json_object_get(params, "position");
    uint32_t line = (uint32_t)json_object_get_int(pos, "line", 0);
    uint32_t character = (uint32_t)json_object_get_int(pos, "character", 0);

    Document *doc = uri ? doc_get(uri) : NULL;
    char *prefix = doc ? doc_word_at(doc, line, character) : NULL;
    size_t prefix_len = prefix ? strlen(prefix) : 0;

    JsonValue *result = json_object();
    json_object_set(result, "isIncomplete", json_bool(false));
    JsonValue *items = json_array();

    /* Match builtins */
    for (size_t i = 0; i < LSP_BUILTINS_COUNT; i++) {
        const LspBuiltin *b = &lsp_builtins[i];
        if (prefix_len == 0 || strncmp(b->name, prefix, prefix_len) == 0) {
            JsonValue *item = json_object();
            json_object_set(item, "label", json_string(b->name));
            json_object_set(item, "kind", json_int(COMPLETION_FUNCTION));
            if (b->signature[0]) {
                char detail[512];
                snprintf(detail, sizeof(detail), "%s : %s", b->name, b->signature);
                json_object_set(item, "detail", json_string(detail));
            }
            if (b->description[0]) {
                JsonValue *doc_obj = json_object();
                json_object_set(doc_obj, "kind", json_string("markdown"));
                json_object_set(doc_obj, "value", json_string(b->description));
                json_object_set(item, "documentation", doc_obj);
            }
            json_array_push(items, item);
        }
    }

    /* Match document definitions */
    if (doc) {
        for (size_t i = 0; i < doc->defs_len; i++) {
            DocDefinition *d = &doc->defs[i];
            if (prefix_len == 0 || strncmp(d->name, prefix, prefix_len) == 0) {
                JsonValue *item = json_object();
                json_object_set(item, "label", json_string(d->name));
                json_object_set(item, "kind", json_int(COMPLETION_VARIABLE));
                if (d->body_text) {
                    JsonValue *doc_obj = json_object();
                    json_object_set(doc_obj, "kind", json_string("markdown"));
                    char *md = malloc(strlen(d->body_text) + 32);
                    sprintf(md, "```joy\n%s\n```", d->body_text);
                    json_object_set(doc_obj, "value", json_string(md));
                    free(md);
                    json_object_set(item, "documentation", doc_obj);
                }
                json_array_push(items, item);
            }
        }
    }

    json_object_set(result, "items", items);
    free(prefix);
    return result;
}
