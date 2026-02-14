/*
 *  module  : hover.c
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  textDocument/hover handler.
 *  Shows signature + description for builtins, or definition body for user symbols.
 */
#include "hover.h"
#include "document.h"
#include "lsp_builtins.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

JsonValue *handle_hover(const JsonValue *params) {
    const JsonValue *td = json_object_get(params, "textDocument");
    const char *uri = json_object_get_string(td, "uri");
    const JsonValue *pos = json_object_get(params, "position");
    uint32_t line = (uint32_t)json_object_get_int(pos, "line", 0);
    uint32_t col = (uint32_t)json_object_get_int(pos, "character", 0);

    Document *doc = uri ? doc_get(uri) : NULL;
    if (!doc) return json_null();

    char *sym = doc_symbol_at(doc, line, col);
    if (!sym) return json_null();

    /* Search builtins first */
    for (size_t i = 0; i < LSP_BUILTINS_COUNT; i++) {
        const LspBuiltin *b = &lsp_builtins[i];
        if (strcmp(b->name, sym) == 0) {
            /* Format: **name** : `signature`\n\ndescription */
            size_t md_len = strlen(b->name) + strlen(b->signature) + strlen(b->description) + 64;
            char *md = malloc(md_len);
            if (b->signature[0])
                snprintf(md, md_len, "**%s** : `%s`\n\n%s", b->name, b->signature, b->description);
            else
                snprintf(md, md_len, "**%s**\n\n%s", b->name, b->description);

            JsonValue *result = json_object();
            JsonValue *contents = json_object();
            json_object_set(contents, "kind", json_string("markdown"));
            json_object_set(contents, "value", json_string(md));
            json_object_set(result, "contents", contents);
            free(md);
            free(sym);
            return result;
        }
    }

    /* Search document definitions */
    for (size_t i = 0; i < doc->defs_len; i++) {
        DocDefinition *d = &doc->defs[i];
        if (strcmp(d->name, sym) == 0 && d->body_text) {
            size_t md_len = strlen(d->name) + strlen(d->body_text) + 64;
            char *md = malloc(md_len);
            snprintf(md, md_len, "**%s** (user-defined)\n\n```joy\n%s\n```", d->name, d->body_text);

            JsonValue *result = json_object();
            JsonValue *contents = json_object();
            json_object_set(contents, "kind", json_string("markdown"));
            json_object_set(contents, "value", json_string(md));
            json_object_set(result, "contents", contents);
            free(md);
            free(sym);
            return result;
        }
    }

    free(sym);
    return json_null();
}
