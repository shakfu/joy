/*
 *  module  : main.c
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  Entry point for the Joy LSP server.
 *  Reads JSON-RPC messages from stdin, dispatches them, writes responses to stdout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <tree_sitter/api.h>
#include "transport.h"
#include "server.h"

/* External function from tree-sitter-joy */
extern const TSLanguage *tree_sitter_joy(void);

int main(void) {
    /* Initialize tree-sitter parser */
    TSParser *parser = ts_parser_new();
    if (!parser) {
        fprintf(stderr, "joy-lsp: failed to create parser\n");
        return 1;
    }

    if (!ts_parser_set_language(parser, tree_sitter_joy())) {
        fprintf(stderr, "joy-lsp: failed to set language\n");
        ts_parser_delete(parser);
        return 1;
    }

    /* Initialize server */
    server_init(parser);

    fprintf(stderr, "joy-lsp: server started\n");

    /* Message loop */
    for (;;) {
        size_t msg_len;
        char *msg = transport_read(&msg_len);
        if (!msg) break; /* EOF */

        int should_exit = server_handle_message(msg, msg_len);
        free(msg);

        if (should_exit) break;
    }

    fprintf(stderr, "joy-lsp: server exiting\n");

    /* Cleanup */
    server_free();
    ts_parser_delete(parser);

    return 0;
}
