#define _POSIX_C_SOURCE 200809L
#include "mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* The test of `langc mcp` (Q-S8-11 (a)). Run it from the repo root with a
   temporary directory as argument 1. It sends test/mcp/requests.jsonl to
   build/langc mcp and compares the replies with test/mcp/responses.jsonl
   line by line, then it sends the 3 lines of CAP_CASES, then a `check` of
   each of EXAMPLES. It prints a FAIL line for each mismatch, then `mcp: N
   requests, M mismatches; 5 examples`, and exits 0 only when M is 0.
   It also checks that an output write error gives exit 2. */

/* The lines of a file without their newlines. FOUND is 0 when the file could
   not be read. */
typedef struct {
  char **lines;
  size_t count;
  int found;
} Lines;

/* A ping with the string id ID, padded with spaces before the last brace to
   LEN bytes without the newline (LEN 0: no spaces), and the reply WANT. */
typedef struct {
  const char *id;
  size_t len;
  const char *want;
} CapCase;

/* M9: the line of exactly MCP_LINE_MAX_BYTES bytes gets its reply. The line
   one byte longer is a valid request, so a server with no cap answers it:
   the test needs -32700 with id null. The ping after it needs its reply, so
   a server that stops after the long line fails. */
static const CapCase CAP_CASES[3] = {
  {"cap", MCP_LINE_MAX_BYTES, "{\"jsonrpc\":\"2.0\",\"id\":\"cap\",\"result\":{}}"},
  {"over", MCP_LINE_MAX_BYTES + 1, "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}"},
  {"after", 0, "{\"jsonrpc\":\"2.0\",\"id\":\"after\",\"result\":{}}"},
};

/* The graded examples of the drive (Q-S8-11 (a)), relative to the repo
   root. */
static const char *const EXAMPLES[5] = {
  "examples/agent/01-add-column.lang", "examples/agent/02-rename.lang",   "examples/agent/03-drop-early.lang",
  "examples/agent/04-not-null.lang",   "examples/agent/05-stale-read.lang",
};

/* The bytes of a file. FOUND is 0 when the file could not be read. */
typedef struct {
  char *bytes;
  size_t len;
  int found;
} Bytes;

static void read_lines(const char *path, Lines *lines) {
  FILE *file = fopen(path, "rb");
  char *line = NULL;
  size_t size = 0;
  ssize_t len;
  memset(lines, 0, sizeof *lines);
  if (file == NULL) return;
  lines->found = 1;
  while ((len = getline(&line, &size, file)) >= 0) {
    char **grown = realloc(lines->lines, (lines->count + 1) * sizeof *grown);
    if (grown == NULL) {
      lines->found = 0;
      break;
    }
    lines->lines = grown;
    if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
    lines->lines[lines->count++] = line;
    line = NULL;
    size = 0;
  }
  free(line);
  if (ferror(file)) lines->found = 0;
  fclose(file);
}

static void free_lines(Lines *lines) {
  for (size_t i = 0; i < lines->count; i++) free(lines->lines[i]);
  free(lines->lines);
  memset(lines, 0, sizeof *lines);
}

static void read_bytes(const char *path, Bytes *bytes) {
  FILE *file = fopen(path, "rb");
  size_t size = 0;
  int c;
  memset(bytes, 0, sizeof *bytes);
  if (file == NULL) return;
  bytes->found = 1;
  while ((c = getc(file)) != EOF) {
    if (bytes->len == size) {
      size = size * 2 + 256;
      char *grown = realloc(bytes->bytes, size);
      if (grown == NULL) {
        bytes->found = 0;
        break;
      }
      bytes->bytes = grown;
    }
    bytes->bytes[bytes->len++] = (char)c;
  }
  if (ferror(file)) bytes->found = 0;
  fclose(file);
}

/* Returns 1 and moves *AT past TEXT when *AT starts with TEXT, else 0. */
static int take(const char **at, const char *text) {
  size_t len = strlen(text);
  if (strncmp(*at, text, len) != 0) return 0;
  *at += len;
  return 1;
}

/* Reads the 4 hex digits at TEXT into *VALUE. Returns 0 when one is not a
   hex digit. */
static int hex4(const char *text, unsigned *value) {
  static const char DIGITS[] = "0123456789abcdef0123456789ABCDEF";
  *value = 0;
  for (size_t i = 0; i < 4; i++) {
    const char *digit = text[i] != '\0' ? strchr(DIGITS, text[i]) : NULL;
    if (digit == NULL) return 0;
    *value = *value * 16 + (unsigned)((digit - DIGITS) % 16);
  }
  return 1;
}

/* Reads the JSON string at *AT (from its opening quote) and returns 1 when
   its value is the WANT_LEN bytes of WANT, else 0. On 1, *AT moves past the
   closing quote. A \u escape is compared as its UTF-8 bytes; a surrogate or
   a raw control byte is a mismatch (the outputs are ASCII). */
static int same_string(const char **at, const char *want, size_t want_len) {
  static const char ESCAPES[] = "\"\\/bfnrt";
  static const char VALUES[] = "\"\\/\b\f\n\r\t";
  const char *text = *at;
  size_t done = 0;
  int same = *text == '"';
  if (same) text++;
  while (same && *text != '"' && *text != '\0') {
    unsigned char unit[3];
    size_t units = 1;
    unsigned value = (unsigned char)*text;
    int raw = text[0] != '\\';
    const char *escape = text[0] == '\\' && text[1] != '\0' ? strchr(ESCAPES, text[1]) : NULL;
    if (text[0] == '\\' && text[1] == 'u') {
      same = hex4(text + 2, &value) && (value < 0xD800 || value > 0xDFFF);
      text += same ? 6 : 0;
    } else if (escape != NULL) {
      value = (unsigned char)VALUES[escape - ESCAPES];
      text += 2;
    } else {
      same = value >= 0x20 && value != '\\';
      text++;
    }
    if (raw || value < 0x80) {
      unit[0] = (unsigned char)value;
    } else if (value < 0x800) {
      unit[0] = (unsigned char)(0xC0 | value >> 6);
      unit[1] = (unsigned char)(0x80 | (value & 0x3F));
      units = 2;
    } else {
      unit[0] = (unsigned char)(0xE0 | value >> 12);
      unit[1] = (unsigned char)(0x80 | (value >> 6 & 0x3F));
      unit[2] = (unsigned char)(0x80 | (value & 0x3F));
      units = 3;
    }
    same = same && done + units <= want_len && memcmp(want + done, unit, units) == 0;
    done += units;
  }
  if (!same || *text != '"' || done != want_len) return 0;
  *at = text + 1;
  return 1;
}

/* Prints a FAIL line and returns 1 when the reply GOT (NULL: missing) is not
   WANT (NULL: an extra reply), else returns 0. */
static size_t mismatch(const char *tag, size_t number, const char *want, const char *got) {
  if (want != NULL && got != NULL && strcmp(want, got) == 0) return 0;
  printf("FAIL mcp %s reply %zu: want %.100s, got %.100s\n", tag, number, want == NULL ? "no line" : want,
         got == NULL ? "no line" : got);
  return 1;
}

/* Runs `build/langc mcp` with the stdin INPUT, the stdout DIR/TAG.out and the
   stderr DIR/TAG.err, and reads the replies into GOT. Returns 1 (and prints a
   FAIL line) for a non-zero exit or a non-empty stderr, else 0. */
static size_t run_server(const char *dir, const char *input, const char *tag, Lines *got) {
  char out[600];
  char err[600];
  char command[2048];
  snprintf(out, sizeof out, "%s/%s.out", dir, tag);
  snprintf(err, sizeof err, "%s/%s.err", dir, tag);
  snprintf(command, sizeof command, "build/langc mcp <'%s' >'%s' 2>'%s'", input, out, err);
  int code = system(command);
  int exit_code = code != -1 && WIFEXITED(code) ? WEXITSTATUS(code) : -1;
  read_lines(out, got);
  FILE *file = fopen(err, "rb");
  int quiet = file != NULL && getc(file) == EOF;
  if (file != NULL) fclose(file);
  if (exit_code == 0 && quiet && got->found) return 0;
  printf("FAIL mcp %s: exit %d, stderr %s\n", tag, exit_code, quiet ? "empty" : "not empty");
  return 1;
}

/* Writes the ping of each row of CAP_CASES to PATH, one line each. Returns 0
   when the file cannot be written. */
static int write_caps(const char *path) {
  FILE *file = fopen(path, "wb");
  int ok = file != NULL;
  for (size_t i = 0; i < 3 && ok; i++) {
    int head = fprintf(file, "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"method\":\"ping\"", CAP_CASES[i].id);
    for (size_t n = head < 0 ? 0 : (size_t)head + 1; n < CAP_CASES[i].len; n++) putc(' ', file);
    ok = head >= 0 && fputs("}\n", file) >= 0;
  }
  return file != NULL && fclose(file) == 0 && ok;
}

/* The drive (Q-S8-11 (a)): one server run with a `check` tools/call of each
   of EXAMPLES, with the id "exI". Reply I has item 1 the stdout of
   `build/langc check --json PATH`, item 2 its stderr when it is not empty,
   and isError exit != 0. Returns the number of mismatches. */
static size_t drive(const char *dir) {
  char input[600];
  char command[2048];
  Lines got;
  size_t bad = 0;
  snprintf(input, sizeof input, "%s/mcp-drive.in", dir);
  FILE *file = fopen(input, "wb");
  int ok = file != NULL;
  for (size_t i = 0; i < 5 && ok; i++)
    ok = fprintf(file,
                 "{\"jsonrpc\":\"2.0\",\"id\":\"ex%zu\",\"method\":\"tools/call\","
                 "\"params\":{\"name\":\"check\",\"arguments\":{\"path\":\"%s\"}}}\n",
                 i + 1, EXAMPLES[i]) >= 0;
  ok = file != NULL && fclose(file) == 0 && ok;
  if (!ok) {
    printf("FAIL mcp: %s cannot be written\n", input);
    bad++;
  }
  bad += run_server(dir, input, "mcp-drive", &got);
  for (size_t i = 0; i < 5; i++) {
    char head[200];
    char out_path[600];
    char err_path[600];
    Bytes out;
    Bytes err;
    snprintf(out_path, sizeof out_path, "%s/ex%zu.out", dir, i + 1);
    snprintf(err_path, sizeof err_path, "%s/ex%zu.err", dir, i + 1);
    snprintf(command, sizeof command, "build/langc check --json '%s' >'%s' 2>'%s'", EXAMPLES[i], out_path, err_path);
    int code = system(command);
    int exit_code = code != -1 && WIFEXITED(code) ? WEXITSTATUS(code) : -1;
    read_bytes(out_path, &out);
    read_bytes(err_path, &err);
    snprintf(head, sizeof head, "{\"jsonrpc\":\"2.0\",\"id\":\"ex%zu\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":",
             i + 1);
    const char *at = i < got.count ? got.lines[i] : "";
    int same = (exit_code == 0 || exit_code == 1) && out.found && err.found && take(&at, head) &&
               same_string(&at, out.bytes, out.len) &&
               (err.len == 0 || (take(&at, "},{\"type\":\"text\",\"text\":") && same_string(&at, err.bytes, err.len))) &&
               take(&at, exit_code != 0 ? "}],\"isError\":true}}" : "}],\"isError\":false}}") && *at == '\0';
    if (!same) {
      printf("FAIL mcp drive %s: exit %d, reply %.100s\n", EXAMPLES[i], exit_code,
             i < got.count ? got.lines[i] : "no line");
      bad++;
    }
    free(out.bytes);
    free(err.bytes);
  }
  if (got.count > 5) {
    printf("FAIL mcp drive: %zu replies, want 5\n", got.count);
    bad++;
  }
  free_lines(&got);
  return bad;
}

static int unused_tool(McpTool tool, const char *arg, const char *protocol, FILE *out, FILE *err) {
  (void)tool;
  (void)arg;
  (void)protocol;
  (void)out;
  (void)err;
  return 2;
}

/* An unbuffered write can fail before fflush, leaving no bytes to flush.
   The server must still detect the stream error and return 2. */
static size_t output_error(void) {
  FILE *in = tmpfile();
  FILE *out = tmpfile();
  int ok = in != NULL && out != NULL;
  if (ok) {
    ok = fputs("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}\n", in) >= 0 &&
         fflush(in) == 0 && fseek(in, 0, SEEK_SET) == 0 && setvbuf(out, NULL, _IONBF, 0) == 0 &&
         close(fileno(out)) == 0;
  }
  if (ok) ok = mcp_serve(in, out, unused_tool) == 2 && ferror(out) && fflush(out) == 0;
  if (in != NULL) fclose(in);
  if (out != NULL) fclose(out);
  if (!ok) printf("FAIL mcp: output write error must give exit 2 even when fflush succeeds\n");
  return !ok;
}

int main(int argc, char **argv) {
  Lines requests;
  Lines want;
  Lines got;
  char caps[600];
  size_t bad = 0;
  if (argc != 2 || strchr(argv[1], '\'') != NULL) {
    fprintf(stderr, "usage: build/mcp-test DIRECTORY (the name has no quote)\n");
    return 2;
  }
  read_lines("test/mcp/requests.jsonl", &requests);
  read_lines("test/mcp/responses.jsonl", &want);
  if (!requests.found || !want.found) {
    printf("FAIL mcp: test/mcp/requests.jsonl or test/mcp/responses.jsonl cannot be read\n");
    bad++;
  }
  bad += run_server(argv[1], "test/mcp/requests.jsonl", "mcp", &got);
  size_t lines = want.count > got.count ? want.count : got.count;
  for (size_t i = 0; i < lines; i++)
    bad += mismatch("requests", i + 1, i < want.count ? want.lines[i] : NULL, i < got.count ? got.lines[i] : NULL);
  free_lines(&got);
  snprintf(caps, sizeof caps, "%s/mcp-cap.in", argv[1]);
  if (!write_caps(caps)) {
    printf("FAIL mcp: %s cannot be written\n", caps);
    bad++;
  }
  bad += run_server(argv[1], caps, "mcp-cap", &got);
  lines = got.count > 3 ? got.count : 3;
  for (size_t i = 0; i < lines; i++)
    bad += mismatch("cap", i + 1, i < 3 ? CAP_CASES[i].want : NULL, i < got.count ? got.lines[i] : NULL);
  free_lines(&got);
  bad += drive(argv[1]);
  bad += output_error();
  printf("mcp: %zu requests, %zu mismatches; 5 examples\n", requests.count + 3, bad);
  free_lines(&want);
  free_lines(&requests);
  return bad == 0 ? 0 : 1;
}
