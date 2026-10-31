/* The MCP stdio server of `langc mcp` (Q-S8-1, Q-S8-2, Q-S8-6). This file
   calls no function of main.c: main.c gives the verbs as a callback, so
   build/diagnostics and build/sql-writer link without main.c. */
#ifndef LANG_MCP_H
#define LANG_MCP_H

#include <stdio.h>

/* The longest request line that the server reads, without the newline. A
   longer line gives -32700 with id null; the server drops the rest of the
   line and reads on. */
#define MCP_LINE_MAX_BYTES ((size_t)1 << 20)

/* The protocol revision that `initialize` always gives (Q-S8-1 (a)). */
#define MCP_PROTOCOL_VERSION "2025-11-25"

typedef enum {
  MCP_TOOL_CHECK,
  MCP_TOOL_FMT,
  MCP_TOOL_REPORT,
  MCP_TOOL_EXPLAIN_CODE,
  MCP_TOOL_LIST_STEPS
} McpTool;

/* Runs the verb of TOOL as the CLI does (`check --json ARG`, `fmt ARG`,
   `report ARG PROTOCOL`, `explain-code ARG`, `list-steps` with ARG NULL)
   with fresh Options and a fresh Arena (Q-S8-15 (a)). Writes the CLI stdout
   to OUT and the CLI stderr to ERR. Returns the CLI exit code. */
typedef int (*McpRunTool)(McpTool tool, const char *arg, const char *protocol, FILE *out, FILE *err);

/* Reads one JSON-RPC 2.0 message on each line of IN until EOF and writes
   each reply on one line of OUT. Returns 0, or 2 when IN cannot be read or
   OUT cannot be written. */
int mcp_serve(FILE *in, FILE *out, McpRunTool run_tool);

#endif
