/*
 *  module  : hover.h
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  textDocument/hover handler.
 */
#ifndef LSP_HOVER_H
#define LSP_HOVER_H

#include "json.h"

/* Handle textDocument/hover request.
 * Returns a Hover JSON value, or JSON null. */
JsonValue *handle_hover(const JsonValue *params);

#endif /* LSP_HOVER_H */
