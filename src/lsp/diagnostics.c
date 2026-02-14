/*
 *  module  : diagnostics.c
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  textDocument/publishDiagnostics handler.
 *  Walks tree-sitter tree for ERROR/MISSING nodes.
 */
#include "diagnostics.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* LSP DiagnosticSeverity */
#define SEVERITY_ERROR   1
#define SEVERITY_WARNING 2

static void collect_errors(TSNode node, const char *text, size_t text_len,
                           JsonValue *diags) {
    if (ts_node_is_error(node)) {
        JsonValue *diag = json_object();

        TSPoint sp = ts_node_start_point(node);
        TSPoint ep = ts_node_end_point(node);

        JsonValue *range = json_object();
        JsonValue *start = json_object();
        json_object_set(start, "line", json_int(sp.row));
        json_object_set(start, "character", json_int(sp.column));
        JsonValue *end = json_object();
        json_object_set(end, "line", json_int(ep.row));
        json_object_set(end, "character", json_int(ep.column));
        json_object_set(range, "start", start);
        json_object_set(range, "end", end);
        json_object_set(diag, "range", range);

        json_object_set(diag, "severity", json_int(SEVERITY_ERROR));
        json_object_set(diag, "source", json_string("joy"));
        json_object_set(diag, "message", json_string("Syntax error"));

        json_array_push(diags, diag);
        return; /* Don't recurse into error nodes */
    }

    if (ts_node_is_missing(node)) {
        JsonValue *diag = json_object();

        TSPoint sp = ts_node_start_point(node);

        JsonValue *range = json_object();
        JsonValue *start = json_object();
        json_object_set(start, "line", json_int(sp.row));
        json_object_set(start, "character", json_int(sp.column));
        /* Missing nodes are zero-width */
        json_object_set(range, "start", start);
        json_object_set(range, "end", json_object_get(range, "start"));

        /* Re-create end since we can't reuse the same object */
        JsonValue *end = json_object();
        json_object_set(end, "line", json_int(sp.row));
        json_object_set(end, "character", json_int(sp.column));
        json_object_set(range, "end", end);

        json_object_set(diag, "range", range);
        json_object_set(diag, "severity", json_int(SEVERITY_WARNING));
        json_object_set(diag, "source", json_string("joy"));

        const char *type = ts_node_type(node);
        char msg[128];
        snprintf(msg, sizeof(msg), "Missing %s", type ? type : "token");
        json_object_set(diag, "message", json_string(msg));

        json_array_push(diags, diag);
        return;
    }

    /* Recurse into children */
    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; i++) {
        TSNode child = ts_node_child(node, i);
        if (ts_node_has_error(child) || ts_node_is_missing(child)) {
            collect_errors(child, text, text_len, diags);
        }
    }
}

JsonValue *build_diagnostics(Document *doc) {
    JsonValue *notification = json_object();
    json_object_set(notification, "jsonrpc", json_string("2.0"));
    json_object_set(notification, "method", json_string("textDocument/publishDiagnostics"));

    JsonValue *params = json_object();
    json_object_set(params, "uri", json_string(doc->uri));

    JsonValue *diags = json_array();

    if (doc->tree) {
        TSNode root = ts_tree_root_node(doc->tree);
        if (ts_node_has_error(root)) {
            collect_errors(root, doc->text, doc->text_len, diags);
        }
    }

    json_object_set(params, "diagnostics", diags);
    json_object_set(notification, "params", params);
    return notification;
}
