/*
 *  module  : diagnostics.h
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  textDocument/publishDiagnostics handler.
 */
#ifndef LSP_DIAGNOSTICS_H
#define LSP_DIAGNOSTICS_H

#include "json.h"
#include "document.h"

/* Build a publishDiagnostics notification for the given document.
 * Returns the full JSON-RPC notification object. */
JsonValue *build_diagnostics(Document *doc);

#endif /* LSP_DIAGNOSTICS_H */
