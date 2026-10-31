#define _POSIX_C_SOURCE 200809L
/* The MCP stdio server of `langc mcp` (mcp.h): the framing (Q-S8-2 (a)),
   the lifecycle (Q-S8-1 (a)) and the tools check, fmt and report (Q-S8-4
   (a), Q-S8-5 (a)). */
#include "mcp.h"

#include <stdlib.h>
#include <string.h>

#include "front/base.h"
#include "front/jsonin.h"

/* The arena of one request line. arena_init is lazy, so the limit costs
   nothing up front. */
#define MCP_ARENA_LIMIT_BYTES ((size_t)1 << 30)

/* JSON-RPC 2.0 error codes. */
enum {
  RPC_PARSE = -32700,
  RPC_BAD_REQUEST = -32600,
  RPC_UNKNOWN_METHOD = -32601,
  RPC_BAD_PARAMS = -32602,
  RPC_INTERNAL = -32603
};

/* The tools/list result. */
static const char MCP_TOOLS[] =
  "{\"tools\":["
  "{\"name\":\"check\",\"description\":\"Checks the evolve-lang program at path. The text is the langc check --json lines.\","
  "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
  "{\"name\":\"fmt\",\"description\":\"Gives the canonical text of the evolve-lang program at path, as langc fmt.\","
  "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
  "{\"name\":\"report\",\"description\":\"Gives the langc report JSON document of the protocol of the evolve-lang program at path.\","
  "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"protocol\":{\"type\":\"string\"}},"
  "\"required\":[\"path\",\"protocol\"]}},"
  "{\"name\":\"explain-code\",\"description\":\"Gives the row of the diagnostic code in docs/DIAGNOSTICS.md, as langc explain-code.\","
  "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"code\":{\"type\":\"string\"}},\"required\":[\"code\"]}},"
  "{\"name\":\"list-steps\",\"description\":\"Gives the 12 protocol steps and their surface forms, as langc list-steps.\","
  "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
  "]}";

/* Writes the LEN bytes of TEXT as a JSON string by the rule of json_escape.
   TEXT[LEN] is a NUL byte. */
static void put_string(const char *text, size_t len, FILE *out) {
  fputc('"', out);
  for (size_t at = 0; at < len;) {
    char esc[JSON_ESCAPE_MAX];
    size_t esc_len;
    at += json_escape(text + at, esc, &esc_len);
    fwrite(esc, 1, esc_len, out);
  }
  fputc('"', out);
}

/* Writes the id of a request: a string, a number (its source text) or
   null. */
static void put_id(const JsonValue *id, FILE *out) {
  if (id != NULL && id->kind == JSON_STRING) {
    put_string(id->text, id->len, out);
    return;
  }
  fputs(id != NULL && id->kind == JSON_NUMBER ? id->text : "null", out);
}

static void rpc_head(const JsonValue *id, FILE *out) {
  fputs("{\"jsonrpc\":\"2.0\",\"id\":", out);
  put_id(id, out);
}

static void rpc_error(const JsonValue *id, int code, const char *message, FILE *out) {
  rpc_head(id, out);
  fprintf(out, ",\"error\":{\"code\":%d,\"message\":", code);
  put_string(message, strlen(message), out);
  fputs("}}\n", out);
}

/* `initialize` always gives MCP_PROTOCOL_VERSION, whatever revision the
   client asks for (Q-S8-1 (a)). */
static void mcp_initialize(const JsonValue *id, const JsonValue *params, McpRunTool run_tool, FILE *out) {
  (void)run_tool;
  if (json_get_string(json_member(params, "protocolVersion")) == NULL) {
    rpc_error(id, RPC_BAD_PARAMS, "Invalid params", out);
    return;
  }
  rpc_head(id, out);
  fputs(",\"result\":{\"protocolVersion\":\"" MCP_PROTOCOL_VERSION "\",\"capabilities\":{\"tools\":{}},"
        "\"serverInfo\":{\"name\":\"evolve-lang\",\"version\":\"S8\"}}}\n",
        out);
}

static void mcp_ping(const JsonValue *id, const JsonValue *params, McpRunTool run_tool, FILE *out) {
  (void)params;
  (void)run_tool;
  rpc_head(id, out);
  fputs(",\"result\":{}}\n", out);
}

static void mcp_tools_list(const JsonValue *id, const JsonValue *params, McpRunTool run_tool, FILE *out) {
  (void)params;
  (void)run_tool;
  rpc_head(id, out);
  fputs(",\"result\":", out);
  fputs(MCP_TOOLS, out);
  fputs("}\n", out);
}

/* Runs the tool NAME with ARGS. Returns the exit code of the verb, or -1
   for an unknown tool or a missing argument. */
static int call_tool(const char *name, const JsonValue *args, McpRunTool run_tool, FILE *out, FILE *err) {
  const char *path = json_get_string(json_member(args, "path"));
  const char *protocol = json_get_string(json_member(args, "protocol"));
  const char *code = json_get_string(json_member(args, "code"));
  if (strcmp(name, "list-steps") == 0) return run_tool(MCP_TOOL_LIST_STEPS, NULL, NULL, out, err);
  if (strcmp(name, "explain-code") == 0) return code != NULL ? run_tool(MCP_TOOL_EXPLAIN_CODE, code, NULL, out, err) : -1;
  if (path == NULL) return -1;
  if (strcmp(name, "check") == 0) return run_tool(MCP_TOOL_CHECK, path, NULL, out, err);
  if (strcmp(name, "fmt") == 0) return run_tool(MCP_TOOL_FMT, path, NULL, out, err);
  if (strcmp(name, "report") == 0) return protocol != NULL ? run_tool(MCP_TOOL_REPORT, path, protocol, out, err) : -1;
  return -1;
}

/* `tools/call`: item 1 is the CLI stdout of the verb, item 2 is the CLI
   stderr when it is not empty, and isError is exit != 0 (Q-S8-5 (a)). */
static void mcp_call(const JsonValue *id, const JsonValue *params, McpRunTool run_tool, FILE *out) {
  const char *name = json_get_string(json_member(params, "name"));
  const JsonValue *args = json_member(params, "arguments");
  if (name == NULL || (args != NULL && args->kind != JSON_OBJECT)) {
    rpc_error(id, RPC_BAD_PARAMS, "Invalid params", out);
    return;
  }
  char *text = NULL;
  size_t len = 0;
  char *err_text = NULL;
  size_t err_len = 0;
  FILE *capture = open_memstream(&text, &len);
  FILE *err_capture = open_memstream(&err_text, &err_len);
  int opened = capture != NULL && err_capture != NULL;
  int status = opened ? call_tool(name, args, run_tool, capture, err_capture) : 0;
  /* Close both captures, also when the first close fails. */
  int closed = capture == NULL || fclose(capture) == 0;
  closed = (err_capture == NULL || fclose(err_capture) == 0) && closed;
  if (!opened || !closed) {
    rpc_error(id, RPC_INTERNAL, "Internal error", out);
  } else if (status < 0) {
    rpc_error(id, RPC_BAD_PARAMS, "Invalid params", out);
  } else {
    rpc_head(id, out);
    fputs(",\"result\":{\"content\":[{\"type\":\"text\",\"text\":", out);
    put_string(text != NULL ? text : "", text != NULL ? len : 0, out);
    if (err_len > 0) {
      fputs("},{\"type\":\"text\",\"text\":", out);
      put_string(err_text, err_len, out);
    }
    fprintf(out, "}],\"isError\":%s}}\n", status != 0 ? "true" : "false");
  }
  free(text);
  free(err_text);
}

typedef void (*McpMethod)(const JsonValue *id, const JsonValue *params, McpRunTool run_tool, FILE *out);

static const struct {
  const char *name;
  McpMethod run;
} MCP_METHODS[] = {
  {"initialize", mcp_initialize},
  {"ping", mcp_ping},
  {"tools/list", mcp_tools_list},
  {"tools/call", mcp_call},
};

/* An id is absent, a string, a number or null. */
static int id_ok(const JsonValue *id) {
  return id == NULL || id->kind == JSON_NUMBER || id->kind == JSON_NULL || id->kind == JSON_STRING;
}

/* Answers the message in the LEN bytes of LINE on OUT. A notification (a
   valid request without an id) gets no reply. Any other method gives
   -32601 (Q-S8-1 (a)). */
static void mcp_message(Arena *arena, const char *line, size_t len, McpRunTool run_tool, FILE *out) {
  JsonValue msg;
  Diag diag;
  diag_init(&diag);
  if (!json_parse(arena, line, len, &msg, &diag)) {
    rpc_error(NULL, RPC_PARSE, "Parse error", out);
    return;
  }
  const JsonValue *id = json_member(&msg, "id");
  const JsonValue *params = json_member(&msg, "params");
  const char *version = json_get_string(json_member(&msg, "jsonrpc"));
  const JsonValue *method = json_member(&msg, "method");
  int valid = version != NULL && strcmp(version, "2.0") == 0 && method != NULL && method->kind == JSON_STRING &&
              id_ok(id) && (params == NULL || params->kind == JSON_OBJECT || params->kind == JSON_ARRAY);
  if (!valid) {
    rpc_error(id_ok(id) ? id : NULL, RPC_BAD_REQUEST, "Invalid Request", out);
    return;
  }
  if (id == NULL) return;
  for (size_t i = 0; i < sizeof MCP_METHODS / sizeof MCP_METHODS[0]; i++) {
    if (strlen(MCP_METHODS[i].name) == method->len && memcmp(MCP_METHODS[i].name, method->text, method->len) == 0) {
      MCP_METHODS[i].run(id, params, run_tool, out);
      return;
    }
  }
  rpc_error(id, RPC_UNKNOWN_METHOD, "Method not found", out);
}

/* Reads one line of IN without its newline into LINE (MCP_LINE_MAX_BYTES
   bytes) and sets *LEN. Returns 0 at EOF before the first byte, 1 for a
   line, or -1 for a longer line: the rest of it up to the newline is read
   and dropped. */
static int read_line(FILE *in, char *line, size_t *len) {
  int c = getc(in);
  if (c == EOF) return 0;
  size_t n = 0;
  int over = 0;
  for (; c != EOF && c != '\n'; c = getc(in)) {
    over |= n == MCP_LINE_MAX_BYTES;
    if (!over) line[n++] = (char)c;
  }
  *len = n;
  return over ? -1 : 1;
}

int mcp_serve(FILE *in, FILE *out, McpRunTool run_tool) {
  char *line = malloc(MCP_LINE_MAX_BYTES);
  if (line == NULL) return 2;
  size_t len = 0;
  int got;
  int ok = 1;
  while (ok && (got = read_line(in, line, &len)) != 0) {
    len -= got > 0 && len > 0 && line[len - 1] == '\r';
    Arena arena;
    arena_init(&arena, MCP_ARENA_LIMIT_BYTES);
    if (got < 0) {
      rpc_error(NULL, RPC_PARSE, "Parse error", out);
    } else if (len > 0) {
      mcp_message(&arena, line, len, run_tool, out);
    }
    arena_release(&arena);
    ok = fflush(out) == 0 && !ferror(out);
  }
  free(line);
  return ok && !ferror(in) ? 0 : 2;
}
