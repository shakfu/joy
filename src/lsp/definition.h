/*
 *  module  : definition.h
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  textDocument/definition handler.
 */
#ifndef LSP_DEFINITION_H
#define LSP_DEFINITION_H

#include "json.h"

/* Handle textDocument/definition request.
 * Returns a Location JSON value, or JSON null. */
JsonValue *handle_definition(const JsonValue *params);

#endif /* LSP_DEFINITION_H */
