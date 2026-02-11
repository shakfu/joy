/*
 *  module  : completion.h
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  textDocument/completion handler.
 */
#ifndef LSP_COMPLETION_H
#define LSP_COMPLETION_H

#include "json.h"

/* Handle textDocument/completion request.
 * Returns a CompletionList JSON value. */
JsonValue *handle_completion(const JsonValue *params);

#endif /* LSP_COMPLETION_H */
