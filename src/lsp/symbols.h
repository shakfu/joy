/*
 *  module  : symbols.h
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  textDocument/documentSymbol handler.
 */
#ifndef LSP_SYMBOLS_H
#define LSP_SYMBOLS_H

#include "json.h"

/* Handle textDocument/documentSymbol request.
 * Returns a JSON array of SymbolInformation. */
JsonValue *handle_document_symbols(const JsonValue *params);

#endif /* LSP_SYMBOLS_H */
