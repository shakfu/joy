/*
 *  module  : server.h
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  LSP server core: method dispatch and lifecycle.
 */
#ifndef LSP_SERVER_H
#define LSP_SERVER_H

#include "json.h"
#include <tree_sitter/api.h>

/* Initialize the server (call once with a configured parser) */
void server_init(TSParser *parser);

/* Process one JSON-RPC message. Returns 1 if the server should exit. */
int server_handle_message(const char *msg, size_t msg_len);

/* Clean up server resources */
void server_free(void);

#endif /* LSP_SERVER_H */
