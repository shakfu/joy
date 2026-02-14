/*
 *  module  : transport.h
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  JSON-RPC transport layer for LSP (Content-Length framing).
 */
#ifndef LSP_TRANSPORT_H
#define LSP_TRANSPORT_H

#include <stddef.h>

/* Read one JSON-RPC message from stdin.
 * Returns malloc'd buffer with the JSON body, or NULL on EOF.
 * *out_len receives the body length. */
char *transport_read(size_t *out_len);

/* Write a JSON-RPC message to stdout.
 * body/body_len is the JSON body to send. */
void transport_write(const char *body, size_t body_len);

#endif /* LSP_TRANSPORT_H */
