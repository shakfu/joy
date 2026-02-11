/*
 *  module  : server.c
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  LSP server core: method dispatch, lifecycle, document sync.
 */
#include "server.h"
#include "transport.h"
#include "document.h"
#include "completion.h"
#include "hover.h"
#include "definition.h"
#include "diagnostics.h"
#include "symbols.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int initialized;
static int shutdown_requested;

static void send_response(const JsonValue *id, JsonValue *result) {
    JsonValue *resp = json_object();
    json_object_set(resp, "jsonrpc", json_string("2.0"));

    /* Copy the id */
    if (id && id->type == JSON_INT)
        json_object_set(resp, "id", json_int(id->u.integer));
    else if (id && id->type == JSON_STRING)
        json_object_set(resp, "id", json_string(id->u.string));
    else
        json_object_set(resp, "id", json_null());

    json_object_set(resp, "result", result);

    JsonBuf buf;
    json_buf_init(&buf);
    json_emit(&buf, resp);
    transport_write(buf.buf, buf.len);
    json_buf_free(&buf);
    json_free(resp);
}

static void send_error(const JsonValue *id, int code, const char *message) {
    JsonValue *resp = json_object();
    json_object_set(resp, "jsonrpc", json_string("2.0"));

    if (id && id->type == JSON_INT)
        json_object_set(resp, "id", json_int(id->u.integer));
    else if (id && id->type == JSON_STRING)
        json_object_set(resp, "id", json_string(id->u.string));
    else
        json_object_set(resp, "id", json_null());

    JsonValue *error = json_object();
    json_object_set(error, "code", json_int(code));
    json_object_set(error, "message", json_string(message));
    json_object_set(resp, "error", error);

    JsonBuf buf;
    json_buf_init(&buf);
    json_emit(&buf, resp);
    transport_write(buf.buf, buf.len);
    json_buf_free(&buf);
    json_free(resp);
}

static void send_notification(JsonValue *notification) {
    JsonBuf buf;
    json_buf_init(&buf);
    json_emit(&buf, notification);
    transport_write(buf.buf, buf.len);
    json_buf_free(&buf);
    json_free(notification);
}

static void publish_diagnostics(Document *doc) {
    JsonValue *notif = build_diagnostics(doc);
    send_notification(notif);
}

/* ---------- handlers ---------- */

static JsonValue *handle_initialize(const JsonValue *params) {
    (void)params;

    JsonValue *result = json_object();

    JsonValue *caps = json_object();

    /* Text document sync: full */
    JsonValue *text_sync = json_object();
    json_object_set(text_sync, "openClose", json_bool(true));
    json_object_set(text_sync, "change", json_int(1)); /* Full sync */
    json_object_set(caps, "textDocumentSync", text_sync);

    /* Completion */
    JsonValue *completion = json_object();
    JsonValue *trigger = json_array();
    /* No specific trigger characters; complete on request */
    json_object_set(completion, "triggerCharacters", trigger);
    json_object_set(caps, "completionProvider", completion);

    /* Hover */
    json_object_set(caps, "hoverProvider", json_bool(true));

    /* Go to definition */
    json_object_set(caps, "definitionProvider", json_bool(true));

    /* Document symbols */
    json_object_set(caps, "documentSymbolProvider", json_bool(true));

    json_object_set(result, "capabilities", caps);

    /* Server info */
    JsonValue *info = json_object();
    json_object_set(info, "name", json_string("joy-lsp"));
    json_object_set(info, "version", json_string("0.1.0"));
    json_object_set(result, "serverInfo", info);

    return result;
}

static void handle_did_open(const JsonValue *params) {
    const JsonValue *td = json_object_get(params, "textDocument");
    if (!td) return;
    const char *uri = json_object_get_string(td, "uri");
    const char *text = json_object_get_string(td, "text");
    if (!uri || !text) return;

    doc_open(uri, text, strlen(text));

    Document *doc = doc_get(uri);
    if (doc) publish_diagnostics(doc);
}

static void handle_did_change(const JsonValue *params) {
    const JsonValue *td = json_object_get(params, "textDocument");
    if (!td) return;
    const char *uri = json_object_get_string(td, "uri");
    if (!uri) return;

    /* Full sync: take the last content change */
    const JsonValue *changes = json_object_get(params, "contentChanges");
    if (!changes || changes->type != JSON_ARRAY || changes->u.array.len == 0) return;

    /* Use the last change (full document text) */
    const JsonValue *last_change = changes->u.array.items[changes->u.array.len - 1];
    const char *text = json_object_get_string(last_change, "text");
    if (!text) return;

    doc_update(uri, text, strlen(text));

    Document *doc = doc_get(uri);
    if (doc) publish_diagnostics(doc);
}

static void handle_did_close(const JsonValue *params) {
    const JsonValue *td = json_object_get(params, "textDocument");
    if (!td) return;
    const char *uri = json_object_get_string(td, "uri");
    if (!uri) return;

    /* Publish empty diagnostics to clear them */
    JsonValue *notif = json_object();
    json_object_set(notif, "jsonrpc", json_string("2.0"));
    json_object_set(notif, "method", json_string("textDocument/publishDiagnostics"));
    JsonValue *p = json_object();
    json_object_set(p, "uri", json_string(uri));
    json_object_set(p, "diagnostics", json_array());
    json_object_set(notif, "params", p);
    send_notification(notif);

    doc_close(uri);
}

/* ---------- dispatch ---------- */

void server_init(TSParser *parser) {
    doc_store_init(parser);
    initialized = 0;
    shutdown_requested = 0;
}

void server_free(void) {
    doc_store_free();
}

int server_handle_message(const char *msg, size_t msg_len) {
    JsonValue *root = json_parse(msg, msg_len);
    if (!root) {
        fprintf(stderr, "joy-lsp: failed to parse JSON message\n");
        return 0;
    }

    const char *method = json_object_get_string(root, "method");
    const JsonValue *id = json_object_get(root, "id");
    const JsonValue *params = json_object_get(root, "params");

    if (!method) {
        /* Probably a response to something — ignore */
        json_free(root);
        return 0;
    }

    fprintf(stderr, "joy-lsp: <- %s\n", method);

    /* Lifecycle */
    if (strcmp(method, "initialize") == 0) {
        JsonValue *result = handle_initialize(params);
        send_response(id, result);
        initialized = 1;
    } else if (strcmp(method, "initialized") == 0) {
        /* Notification, no response needed */
    } else if (strcmp(method, "shutdown") == 0) {
        shutdown_requested = 1;
        send_response(id, json_null());
    } else if (strcmp(method, "exit") == 0) {
        json_free(root);
        return 1; /* Signal exit */
    }
    /* Document sync */
    else if (strcmp(method, "textDocument/didOpen") == 0) {
        handle_did_open(params);
    } else if (strcmp(method, "textDocument/didChange") == 0) {
        handle_did_change(params);
    } else if (strcmp(method, "textDocument/didClose") == 0) {
        handle_did_close(params);
    }
    /* Features */
    else if (strcmp(method, "textDocument/completion") == 0) {
        JsonValue *result = handle_completion(params);
        send_response(id, result);
    } else if (strcmp(method, "textDocument/hover") == 0) {
        JsonValue *result = handle_hover(params);
        send_response(id, result);
    } else if (strcmp(method, "textDocument/definition") == 0) {
        JsonValue *result = handle_definition(params);
        send_response(id, result);
    } else if (strcmp(method, "textDocument/documentSymbol") == 0) {
        JsonValue *result = handle_document_symbols(params);
        send_response(id, result);
    }
    /* Unknown method */
    else if (id) {
        /* Request requires response */
        send_error(id, -32601, "Method not found");
    }
    /* else: unknown notification, ignore */

    json_free(root);
    return 0;
}
