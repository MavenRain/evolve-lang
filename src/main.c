/* The langc command line. SHARED by the tcc kits.
   Exit 0: ok. Exit 1: the program is refused. Exit 2: usage or IO. */
#include <stdio.h>
#include <string.h>

#include "codes.h"
#include "front/check.h"
#include "front/fmt.h"
#include "front/front.h"
#include "front/lower.h"
#include "front/parser.h"
#include "json.h"
#include "mcp.h"
#include "oracle.h"
#include "sql.h"
#include "steps.h"
#include "target.h"

#define ARENA_LIMIT_BYTES ((size_t)1 << 30)
/* The progress object: the fixed text and four size_t numbers. */
#define PROGRESS_MAX_BYTES 256u

typedef enum {
  CMD_CHECK,
  CMD_REPORT,
  CMD_EVAL,
  CMD_IR,
  CMD_ABI,
  CMD_BUILD,
  CMD_FMT,
  CMD_SQL,
  CMD_WASM,
  CMD_MCP,
  CMD_EXPLAIN_CODE,
  CMD_LIST_STEPS
} Command;

typedef struct {
  Command command;
  const char *prog_path;
  const char *entry;
  const char *protocol;
  const char *code;
  char **args;
  int arg_count;
  const char *out_path;
  int runtime;
  int json;
} Options;

static const struct {
  const char *word;
  Command command;
} COMMANDS[] = {
  {"check", CMD_CHECK},
  {"report", CMD_REPORT},
  {"eval", CMD_EVAL},
  {"ir", CMD_IR},
  {"abi", CMD_ABI},
  {"build", CMD_BUILD},
  {"fmt", CMD_FMT},
  {"sql", CMD_SQL},
  {"wasm", CMD_WASM},
  {"mcp", CMD_MCP},
  {"explain-code", CMD_EXPLAIN_CODE},
  {"list-steps", CMD_LIST_STEPS},
};

static int usage(FILE *err) {
  fputs("usage: langc check [--json] PROG\n"
        "       langc report PROG PROTOCOL\n"
        "       langc eval PROG NAME [ARGS...]\n"
        "       langc ir PROG\n"
        "       langc abi PROG\n"
        "       langc build PROG -o OUT [--runtime]\n"
        "       langc fmt PROG\n"
        "       langc sql PROG PROTOCOL OUTDIR\n"
        "       langc wasm PROG PROTOCOL -o OUT\n"
        "       langc mcp\n"
        "       langc explain-code CODE\n"
        "       langc list-steps\n", err);
  return 2;
}

static int find_command(const char *word, Command *out) {
  for (size_t i = 0; i < sizeof COMMANDS / sizeof COMMANDS[0]; i++) {
    if (strcmp(COMMANDS[i].word, word) == 0) {
      *out = COMMANDS[i].command;
      return 1;
    }
  }
  return 0;
}

static int parse_build_options(int argc, char **argv, Options *opt, Diag *diag) {
  for (int i = 3; i < argc; i++) {
    int is_out = strcmp(argv[i], "-o") == 0 && i + 1 < argc;
    int is_runtime = strcmp(argv[i], "--runtime") == 0;
    if (!is_out && !is_runtime) return diag_fail(diag, "USAGE", NULL, "unknown build argument '%s'", argv[i]);
    opt->out_path = is_out ? argv[i + 1] : opt->out_path;
    opt->runtime |= is_runtime;
    i += is_out;
  }
  return opt->out_path != NULL ? 1 : diag_fail(diag, "USAGE", NULL, "build needs -o OUT");
}

static int parse_options(int argc, char **argv, Options *opt, Diag *diag) {
  memset(opt, 0, sizeof *opt);
  /* mcp and list-steps take no argument; explain-code takes a code. */
  if (argc < 2 || !find_command(argv[1], &opt->command) ||
      (argc < 3 && opt->command != CMD_MCP && opt->command != CMD_EXPLAIN_CODE && opt->command != CMD_LIST_STEPS)) {
    return diag_fail(diag, "USAGE", NULL, "expected a command and a program path");
  }
  opt->json = opt->command == CMD_CHECK && strcmp(argv[2], "--json") == 0;
  opt->prog_path = argv[2 + opt->json];
  switch (opt->command) {
    case CMD_CHECK:
    case CMD_IR:
    case CMD_ABI:
    case CMD_FMT:
      if (opt->json && argc == 3) return diag_fail(diag, "USAGE", NULL, "check --json needs a program path");
      return argc == 3 + opt->json ? 1 : diag_fail(diag, "USAGE", NULL, "too many arguments");
    case CMD_REPORT:
      if (argc != 4) return diag_fail(diag, "USAGE", NULL, "report needs a program path and a protocol name");
      opt->protocol = argv[3];
      return 1;
    case CMD_SQL:
      if (argc != 5) return diag_fail(diag, "USAGE", NULL, "sql needs a program path, a protocol name and a directory");
      opt->protocol = argv[3];
      opt->out_path = argv[4];
      return 1;
    case CMD_WASM:
      if (argc != 6 || strcmp(argv[4], "-o") != 0) {
        return diag_fail(diag, "USAGE", NULL, "wasm needs a program path, a protocol name and -o OUT");
      }
      opt->protocol = argv[3];
      opt->out_path = argv[5];
      return 1;
    case CMD_EVAL:
      if (argc < 4) return diag_fail(diag, "USAGE", NULL, "eval needs a definition name");
      opt->entry = argv[3];
      opt->args = argv + 4;
      opt->arg_count = argc - 4;
      return 1;
    case CMD_BUILD:
      return parse_build_options(argc, argv, opt, diag);
    case CMD_MCP:
      return argc == 2 ? 1 : diag_fail(diag, "USAGE", NULL, "mcp takes no arguments");
    case CMD_EXPLAIN_CODE:
      if (argc != 3) return diag_fail(diag, "USAGE", NULL, "explain-code needs one code");
      opt->code = argv[2];
      return 1;
    case CMD_LIST_STEPS:
      return argc == 2 ? 1 : diag_fail(diag, "USAGE", NULL, "list-steps takes no arguments");
  }
  return 0;
}

static int read_source(Arena *arena, const char *path, char **out, size_t *len, Diag *diag) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) return diag_fail(diag, "IO", NULL, "cannot open %s", path);
  char *buf = arena_alloc(arena, SOURCE_MAX_BYTES + 1);
  size_t count = buf == NULL ? 0 : fread(buf, 1, SOURCE_MAX_BYTES + 1, file);
  int read_error = ferror(file);
  fclose(file);
  if (buf == NULL) return diag_fail(diag, "OOM", NULL, "no memory for %s", path);
  if (read_error) return diag_fail(diag, "IO", NULL, "cannot read %s", path);
  if (count > SOURCE_MAX_BYTES) return diag_fail(diag, "IO_SIZE", NULL, "%s is larger than %zu bytes", path, SOURCE_MAX_BYTES);
  *out = buf;
  *len = count;
  return 1;
}

/* Lowers the entries, then runs the ir, abi or build command. */
static int run_target(const Options *opt, Machine *machine, Diag *diag) {
  IrProgram prog;
  FILE *out;
  size_t i;
  int ok = 1;
  if (!lower_program(machine, &prog)) return 1;
  switch (opt->command) {
    case CMD_IR:
      ir_print(&prog, stdout);
      return 0;
    case CMD_ABI:
      for (i = 0; i < prog.func_count && ok; i++) ok = target_abi_line(&prog.funcs[i], stdout, stderr);
      return ok ? 0 : 1;
    case CMD_CHECK:
    case CMD_REPORT:
    case CMD_EVAL:
    case CMD_BUILD:
    case CMD_FMT:
    case CMD_SQL:
    case CMD_WASM:
    case CMD_MCP:
    case CMD_EXPLAIN_CODE:
    case CMD_LIST_STEPS:
      break;
  }
  out = fopen(opt->out_path, "wb");
  if (out == NULL) return diag_fail(diag, "IO", NULL, "cannot write '%s'", opt->out_path) + 1;
  ok = target_write(&prog, opt->runtime ? TARGET_PART_RUNTIME : TARGET_PART_MAIN, out, stderr);
  return fclose(out) == 0 && ok ? 0 : 1;
}

static int run(const Options *opt, Arena *arena, Diag *diag) {
  char *text = NULL;
  size_t len = 0;
  if (!read_source(arena, opt->prog_path, &text, &len, diag)) return 2;
  Front front;
  if (!front_load(arena, opt->prog_path, text, len, &front, diag)) return 1;
  Machine machine;
  if (!check_program(arena, &front.decls, &machine, diag)) return 1;
  switch (opt->command) {
    case CMD_EVAL:
      return eval_command(&machine, opt->entry, opt->args, opt->arg_count, stdout, stderr);
    case CMD_CHECK: /* run_check does check */
    case CMD_REPORT: /* run_report does report */
    case CMD_IR:
    case CMD_ABI:
    case CMD_BUILD:
    case CMD_FMT: /* run_fmt does fmt */
    case CMD_SQL: /* run_sql does sql */
    case CMD_WASM: /* run_wasm does wasm */
    case CMD_MCP:  /* mcp_serve does mcp */
    case CMD_EXPLAIN_CODE: /* run_explain_code does explain-code */
    case CMD_LIST_STEPS: /* run_list_steps does list-steps */
      break;
  }
  return run_target(opt, &machine, diag);
}

/* The span of declaration D. A domain declaration is in domain/domain.lang. */
static Span decl_span(const char *path, const Decl *d) {
  Span span = {d->origin == ORIGIN_PRELUDE ? "src/front/prelude.lang" : d->origin == ORIGIN_DOMAIN ? "domain/domain.lang" : path, d->line, d->col, d->end_line, d->end_col};
  return span;
}

/* The offset after COUNT more newlines from AT, or LEN. */
static size_t skip_lines(const char *text, size_t len, size_t at, int count) {
  size_t pos = at;
  for (int n = 0; n < count && pos < len; pos++) n += text[pos] == '\n';
  return pos;
}

static int count_newlines(const char *text, size_t len) {
  int n = 0;
  for (size_t i = 0; i < len; i++) n += text[i] == '\n';
  return n;
}

/* Records W_NONCANON in WARN for the region of lines FIRST to LAST of
   declaration D (NULL for the tail). FOUND is the input text of the region,
   FIX its canonical text. */
static void warn_region(Arena *arena, const char *path, const Decl *d, int first, const char *found,
                        size_t found_len, const char *fix, size_t fix_len, Diag *warn) {
  Span span = {path, first, 1, first, 1};
  for (size_t i = 0; i < found_len; i++) {
    unsigned char byte = (unsigned char)found[i];
    if (byte == '\n') {
      span.end_line++;
      span.end_col = 1;
    } else if ((byte & 0xc0u) != 0x80u) {
      span.end_col++;
    }
  }
  int last = span.end_line - (span.end_col == 1 && span.end_line > first);
  diag_fail_at(warn, "W_NONCANON", d != NULL ? d->name : NULL, span, NULL, NULL, NULL,
               "%s:%d:%d: lines %d to %d are not canonical; replace them with the fix text (the langc fmt output)",
               path, span.line, span.col, first, last);
  const char *found_copy = arena_strndup(arena, found, found_len);
  const char *fix_copy = fix_len == 0 ? "" : arena_strndup(arena, fix, fix_len);
  diag_attach(warn, fix_copy, found_copy, fix_copy);
}

/* Records one W_NONCANON in WARN for the first program region that differs
   from its canonical text (HANDOFF decision 3): region I runs from the line
   after declaration I-1 to the end of the last line of declaration I. The
   text after the last declaration is one more region (the tail). */
static void noncanon(Arena *arena, const char *path, const char *text, size_t len, const Front *front, Diag *warn) {
  const Decl *decls = front->decls.items + front->domain_count;
  size_t count = front->decls.count - front->domain_count;
  FmtText canon;
  size_t next_comment = 0;
  size_t pos = 0;
  int prev_line = 0;
  fmt_text_init(&canon, arena);
  for (size_t i = 0; i <= count && !warn->set && !canon.oom; i++) {
    int tail = i == count;
    size_t before = canon.len;
    size_t end = tail ? len : skip_lines(text, len, pos, decls[i].end_line - prev_line);
    if (tail) {
      fmt_tail(&canon, count, &front->toks, next_comment);
    } else {
      fmt_region(&canon, decls, i, &front->toks, &next_comment);
    }
    size_t fix_len = canon.len - before;
    int same = fix_len == end - pos && (fix_len == 0 || memcmp(canon.data + before, text + pos, fix_len) == 0);
    int lines = count_newlines(text + pos, end - pos);
    int last = tail ? prev_line + (lines > 0 ? lines : 1) : decls[i].end_line;
    if (!same && !canon.oom) {
      warn_region(arena, path, tail ? NULL : &decls[i], prev_line + 1, text + pos, end - pos, canon.data + before,
                  fix_len, warn);
    }
    prev_line = last;
    pos = end;
  }
}

/* The progress object of `check --json` and `report`, without the newline.
   The count is the number of program declarations: after a parse failure,
   the complete ones and the failed one; after a lex failure, 0. */
static void progress_text(const Front *front, const Machine *machine, int parsed, int typed, char *buf, size_t size) {
  size_t done = front->lexed ? front->decls.count - front->domain_count : 0;
  size_t count = done + (size_t)(front->lexed && !parsed);
  size_t checked = machine->checked > front->domain_count ? machine->checked - front->domain_count : 0;
  size_t ok = typed ? count : parsed ? checked : 0;
  snprintf(buf, size, "{\"parsed\":\"%zu of %zu\",\"typed\":\"%zu of %zu\",\"affine\":\"0 of 0\",\"laws\":\"0 of 0\"}",
           done, count, ok, count);
}

/* The last line of `check --json`. */
static void print_progress(const Front *front, const Machine *machine, int parsed, int typed, FILE *out) {
  char buf[PROGRESS_MAX_BYTES];
  progress_text(front, machine, parsed, typed, buf, sizeof buf);
  fprintf(out, "%s\n", buf);
}

/* A program after check_source. */
typedef struct {
  char *text;
  size_t len;
  Front front;
  Machine machine;
  Diag warn;
  int loaded;
  int parsed;
  int typed;
} Checked;

/* The steps of `check` and `report`: read, load and check the program (when
   REPORT is not NULL, also fill REPORT), then find the W_NONCANON warning
   (at most one) into OUT->warn. The error goes to DIAG. */
static void check_source(const Options *opt, Arena *arena, Diag *diag, Report *report, Checked *out) {
  memset(out, 0, sizeof *out);
  diag_init(&out->warn);
  out->loaded = read_source(arena, opt->prog_path, &out->text, &out->len, diag);
  out->parsed = out->loaded && front_load(arena, opt->prog_path, out->text, out->len, &out->front, diag);
  out->typed = out->parsed && check_program_report(arena, &out->front.decls, &out->machine, diag, report);
  if (out->parsed && !out->typed && out->machine.decl_failed) {
    diag_default_span(diag, decl_span(opt->prog_path, &out->front.decls.items[out->machine.checked]));
  }
  if (out->parsed) noncanon(arena, opt->prog_path, out->text, out->len, &out->front, &out->warn);
}

/* The lines of `check --json`: the error, the warning, the progress line. */
static void print_check_json(const Diag *diag, const Checked *c, FILE *out) {
  if (diag->set) diag_print_json(diag, out);
  if (c->warn.set) diag_print_json(&c->warn, out);
  print_progress(&c->front, &c->machine, c->parsed, c->typed, out);
}

/* `check` in both modes: the error (at most one), then the W_NONCANON
   warning (at most one), then `ok` (text) or the progress line (JSON). A
   warning does not change the exit code. */
static int run_check(const Options *opt, Arena *arena, Diag *diag, FILE *out, FILE *err) {
  Checked c;
  check_source(opt, arena, diag, NULL, &c);
  if (opt->json) {
    print_check_json(diag, &c, out);
  } else {
    if (diag->set) diag_print(diag, err);
    if (c.warn.set) diag_print(&c.warn, err);
    if (c.typed) fputs("ok\n", out);
  }
  return c.typed ? 0 : c.loaded ? 1 : 2;
}

/* `report`: one JSON document for the protocol OPT->protocol on OUT. A
   program that is not checked gives the `check --json` lines (exit 1, or 2
   when it cannot be read). A program with no such protocol is a usage error
   (exit 2). The W_NONCANON warning goes to ERR as text, so OUT stays one
   document. */
static int run_report(const Options *opt, Arena *arena, Diag *diag, FILE *out, FILE *err) {
  Report report;
  memset(&report, 0, sizeof report);
  report.protocol = opt->protocol;
  Checked c;
  check_source(opt, arena, diag, &report, &c);
  if (!c.typed) {
    print_check_json(diag, &c, out);
    return c.loaded ? 1 : 2;
  }
  int found = report.found || diag_fail(diag, "USAGE", NULL, "%s has no protocol %s", opt->prog_path, opt->protocol);
  if (diag->set) diag_print(diag, err);
  if (c.warn.set) diag_print(&c.warn, err);
  if (!found) return 2;
  char progress[PROGRESS_MAX_BYTES];
  progress_text(&c.front, &c.machine, c.parsed, c.typed, progress, sizeof progress);
  JsonOut doc;
  json_out_init(&doc, arena, diag, NULL);
  if (!json_report(&doc, &report, progress)) {
    diag_print(diag, err);
    return 2;
  }
  size_t written = fwrite(doc.data, 1, doc.len, out);
  if (written != doc.len || fflush(out) != 0) {
    diag_fail(diag, "IO", NULL, "cannot write the report");
    diag_print(diag, err);
    return 2;
  }
  return 0;
}

/* `sql`: the psql scripts of the protocol OPT->protocol in the directory
   OPT->out_path (sql.h). A program that is not checked gives the `check`
   text diagnostics (exit 1, or 2 when it cannot be read). A program with no
   such protocol is a usage error (exit 2). stdout stays empty. */
static int run_sql(const Options *opt, Arena *arena, Diag *diag) {
  Report report;
  memset(&report, 0, sizeof report);
  report.protocol = opt->protocol;
  Checked c;
  check_source(opt, arena, diag, &report, &c);
  int found = c.typed && (report.found || diag_fail(diag, "USAGE", NULL, "%s has no protocol %s", opt->prog_path, opt->protocol));
  int status = !c.typed ? (c.loaded ? 1 : 2) : !found ? 2 : sql_write(arena, diag, &c.front.decls, &report, c.text, c.len, opt->out_path);
  if (diag->set) diag_print(diag, stderr);
  if (c.warn.set) diag_print(&c.warn, stderr);
  return status;
}

/* `wasm`: the oracle module of the protocol OPT->protocol in the file
   OPT->out_path (oracle.h). The error paths are those of `sql`. */
static int run_wasm(const Options *opt, Arena *arena, Diag *diag) {
  Report report;
  memset(&report, 0, sizeof report);
  report.protocol = opt->protocol;
  Checked c;
  check_source(opt, arena, diag, &report, &c);
  int found = c.typed && (report.found || diag_fail(diag, "USAGE", NULL, "%s has no protocol %s", opt->prog_path, opt->protocol));
  int status = !c.typed ? (c.loaded ? 1 : 2) : !found ? 2 : oracle_write(arena, diag, &report, opt->out_path);
  if (diag->set) diag_print(diag, stderr);
  if (c.warn.set) diag_print(&c.warn, stderr);
  return status;
}

/* Lexes and parses the program only (no domain, no check), then writes the
   canonical text to OUT. */
static int fmt_source(const Options *opt, Arena *arena, Diag *diag, FILE *out) {
  char *text = NULL;
  size_t len = 0;
  if (!read_source(arena, opt->prog_path, &text, &len, diag)) return 2;
  TokenList toks;
  DeclList decls = {NULL, 0, 0};
  if (!lex_source(arena, opt->prog_path, text, len, &toks, diag)) return 1;
  if (!parse_source(arena, opt->prog_path, ORIGIN_PROGRAM, &toks, &decls, diag)) return 1;
  FmtText canon;
  fmt_text_init(&canon, arena);
  if (!fmt_program(&canon, decls.items, decls.count, &toks)) {
    return diag_fail(diag, "OOM", NULL, "no memory for the canonical text of %s", opt->prog_path) + 2;
  }
  size_t written = canon.len == 0 ? 0 : fwrite(canon.data, 1, canon.len, out);
  if (written != canon.len || fflush(out) != 0) return diag_fail(diag, "IO", NULL, "cannot write the canonical text") + 2;
  return 0;
}

/* `fmt`: the canonical text on OUT, or the error on ERR. */
static int run_fmt(const Options *opt, Arena *arena, Diag *diag, FILE *out, FILE *err) {
  int status = fmt_source(opt, arena, diag, out);
  if (status != 0 && diag->set) diag_print(diag, err);
  return status;
}

/* Writes TEXT as a JSON string by the rule of json_escape. */
static void put_json_string(const char *text, FILE *out) {
  fputc('"', out);
  for (const char *s = text; *s != '\0';) {
    char esc[JSON_ESCAPE_MAX];
    size_t len;
    s += json_escape(s, esc, &len);
    fwrite(esc, 1, len, out);
  }
  fputc('"', out);
}

/* Exit 0, or exit 2 with the IO error on ERR when OUT cannot be written. */
static int flush_out(Diag *diag, FILE *out, FILE *err) {
  if (fflush(out) == 0 && !ferror(out)) return 0;
  diag_fail(diag, "IO", NULL, "cannot write the output");
  diag_print(diag, err);
  return 2;
}

/* `explain-code`: the row of the code as one JSON line on OUT (Q-S8-7 (a)),
   or the USAGE error on ERR when no row has that code. */
static int run_explain_code(const Options *opt, Diag *diag, FILE *out, FILE *err) {
  const CodeRow *row = code_find(opt->code);
  if (row == NULL) {
    diag_fail(diag, "USAGE", NULL, "unknown code '%s'", opt->code);
    diag_print(diag, err);
    return 2;
  }
  fputs("{\"code\":", out);
  put_json_string(row->code, out);
  fputs(",\"severity\":", out);
  put_json_string(row->severity, out);
  fputs(",\"stage\":", out);
  put_json_string(row->stage, out);
  fputs(",\"meaning\":", out);
  put_json_string(row->meaning, out);
  fputs("}\n", out);
  return flush_out(diag, out, err);
}

/* `list-steps`: one JSON line {"step","form"} on OUT for each Step
   constructor, in the domain order (Q-S8-8 (a)). */
static int run_list_steps(Diag *diag, FILE *out, FILE *err) {
  for (size_t i = 0; i < step_count(); i++) {
    const StepRow *row = step_at(i);
    fputs("{\"step\":", out);
    put_json_string(row->step, out);
    fputs(",\"form\":", out);
    put_json_string(row->form, out);
    fputs("}\n", out);
  }
  return flush_out(diag, out, err);
}

/* The verbs of `langc mcp` (mcp.h): TOOL with fresh Options and a fresh
   Arena, as the CLI runs it, on OUT and ERR. ARG is the program path, the
   code of explain-code, or NULL for list-steps (Q-S8-15 (a)). */
static int run_tool(McpTool tool, const char *arg, const char *protocol, FILE *out, FILE *err) {
  Options opt;
  memset(&opt, 0, sizeof opt);
  opt.command = tool == MCP_TOOL_FMT            ? CMD_FMT
                : tool == MCP_TOOL_REPORT       ? CMD_REPORT
                : tool == MCP_TOOL_EXPLAIN_CODE ? CMD_EXPLAIN_CODE
                : tool == MCP_TOOL_LIST_STEPS   ? CMD_LIST_STEPS
                                                : CMD_CHECK;
  opt.prog_path = opt.command == CMD_EXPLAIN_CODE ? NULL : arg;
  opt.code = opt.command == CMD_EXPLAIN_CODE ? arg : NULL;
  opt.protocol = protocol;
  opt.json = tool == MCP_TOOL_CHECK;
  Diag diag;
  diag_init(&diag);
  Arena arena;
  arena_init(&arena, ARENA_LIMIT_BYTES);
  int status = tool == MCP_TOOL_FMT            ? run_fmt(&opt, &arena, &diag, out, err)
               : tool == MCP_TOOL_REPORT       ? run_report(&opt, &arena, &diag, out, err)
               : tool == MCP_TOOL_EXPLAIN_CODE ? run_explain_code(&opt, &diag, out, err)
               : tool == MCP_TOOL_LIST_STEPS   ? run_list_steps(&diag, out, err)
                                               : run_check(&opt, &arena, &diag, out, err);
  arena_release(&arena);
  return status;
}

int main(int argc, char **argv) {
  Diag diag;
  diag_init(&diag);
  Options opt;
  if (!parse_options(argc, argv, &opt, &diag)) {
    diag_print(&diag, stderr);
    return usage(stderr);
  }
  if (opt.command == CMD_MCP) return mcp_serve(stdin, stdout, run_tool);
  Arena arena;
  arena_init(&arena, ARENA_LIMIT_BYTES);
  int status = opt.command == CMD_FMT            ? run_fmt(&opt, &arena, &diag, stdout, stderr)
               : opt.command == CMD_CHECK        ? run_check(&opt, &arena, &diag, stdout, stderr)
               : opt.command == CMD_REPORT       ? run_report(&opt, &arena, &diag, stdout, stderr)
               : opt.command == CMD_SQL          ? run_sql(&opt, &arena, &diag)
               : opt.command == CMD_WASM         ? run_wasm(&opt, &arena, &diag)
               : opt.command == CMD_EXPLAIN_CODE ? run_explain_code(&opt, &diag, stdout, stderr)
               : opt.command == CMD_LIST_STEPS   ? run_list_steps(&diag, stdout, stderr)
                                                 : run(&opt, &arena, &diag);
  arena_release(&arena);
  /* run_fmt, run_check, run_report, run_sql, run_wasm, run_explain_code and
     run_list_steps print their own diagnostics. */
  int printed = opt.command == CMD_FMT || opt.command == CMD_CHECK || opt.command == CMD_REPORT || opt.command == CMD_SQL ||
                opt.command == CMD_WASM || opt.command == CMD_EXPLAIN_CODE || opt.command == CMD_LIST_STEPS;
  if (status != 0 && diag.set && !printed) diag_print(&diag, stderr);
  return status;
}
