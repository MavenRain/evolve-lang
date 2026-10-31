#define _POSIX_C_SOURCE 200809L
#include "codes.h"
#include "front/base.h"
#include "steps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/* The test of `langc explain-code` and `langc list-steps` (Q-S8-7 (a),
   Q-S8-8 (a)). Run it from the repo root. Part 1 runs `build/langc
   explain-code CODE` for each code row of the docs/DIAGNOSTICS.md table and
   compares its stdout with the JSON line of the doc row; then it compares
   each row of the C table (src/codes.c) with the doc row of its code, cell
   for cell. Part 2 compares the step names of `build/langc list-steps`, in
   order, with the Step constructors of domain/domain.lang, and each line
   with the JSON line of its row in src/steps.c. It prints a FAIL line for
   each mismatch, then `explain-code: N codes match docs/DIAGNOSTICS.md` and
   `list-steps: N steps match domain/domain.lang`, and exits 0 only when
   there is no FAIL line. */

#define DOC_PATH "docs/DIAGNOSTICS.md"
#define DOMAIN_PATH "domain/domain.lang"

/* The compared cells: Code, Severity, Stage and Meaning (not Fixture). */
#define CELL_COUNT 4

/* The lines of a file or of the stdout of a command, without their
   newlines. */
typedef struct {
  char **lines;
  size_t count;
} Lines;

/* A code row of the docs/DIAGNOSTICS.md table: the Code cell without its
   backticks, then the Severity, Stage and Meaning cells, each without the
   spaces around it. The cells point into the line. */
typedef struct {
  char *cells[CELL_COUNT];
} DocRow;

static void free_lines(Lines *lines) {
  for (size_t i = 0; i < lines->count; i++) free(lines->lines[i]);
  free(lines->lines);
  lines->lines = NULL;
  lines->count = 0;
}

/* Reads the lines of FILE into LINES. Returns 0 on a read error or when
   there is no memory. */
static int read_stream(FILE *file, Lines *lines) {
  char *line = NULL;
  size_t size = 0;
  ssize_t len;
  int ok = 1;
  while (ok && (len = getline(&line, &size, file)) >= 0) {
    char **grown = realloc(lines->lines, (lines->count + 1) * sizeof *grown);
    ok = grown != NULL;
    if (ok) {
      lines->lines = grown;
      if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
      lines->lines[lines->count++] = line;
      line = NULL;
      size = 0;
    }
  }
  free(line);
  return ok && !ferror(file);
}

/* Reads the lines of the file PATH into LINES. Returns 0 when the file
   cannot be read. */
static int read_file(const char *path, Lines *lines) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) return 0;
  int ok = read_stream(file, lines);
  return fclose(file) == 0 && ok;
}

/* Runs COMMAND and reads its stdout into LINES. Returns the exit code, or -1
   when the command cannot run or its stdout cannot be read. */
static int run_command(const char *command, Lines *lines) {
  FILE *pipe = popen(command, "r");
  if (pipe == NULL) return -1;
  int ok = read_stream(pipe, lines);
  int status = pclose(pipe);
  return ok && status != -1 && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Writes TEXT as a JSON string by the rule of json_escape, as langc does. */
static void put_string(const char *text, FILE *out) {
  fputc('"', out);
  for (const char *s = text; *s != '\0';) {
    char esc[JSON_ESCAPE_MAX];
    size_t len;
    s += json_escape(s, esc, &len);
    fwrite(esc, 1, len, out);
  }
  fputc('"', out);
}

/* The JSON object of the COUNT members NAMES and VALUES on one line, in a
   malloc buffer, or NULL when there is no memory. */
static char *json_line(const char *const *names, const char *const *values, int count) {
  char *text = NULL;
  size_t len = 0;
  FILE *out = open_memstream(&text, &len);
  if (out == NULL) return NULL;
  for (int i = 0; i < count; i++) {
    fprintf(out, "%s\"%s\":", i == 0 ? "{" : ",", names[i]);
    put_string(values[i], out);
  }
  fputc('}', out);
  if (fclose(out) == 0) return text;
  free(text);
  return NULL;
}

/* The text of CELL without the spaces around it. */
static char *trim(char *cell) {
  size_t len = strlen(cell);
  while (len > 0 && cell[len - 1] == ' ') cell[--len] = '\0';
  while (*cell == ' ') cell++;
  return cell;
}

/* Splits LINE, a code row `| `CODE` | ... |`, in place into ROW. Returns 0
   when LINE has fewer cells or its Code cell has no backticks. */
static int split_row(char *line, DocRow *row) {
  char *at = line + 1;
  for (int i = 0; i < CELL_COUNT; i++) {
    char *bar = strchr(at, '|');
    if (bar == NULL) return 0;
    *bar = '\0';
    row->cells[i] = trim(at);
    at = bar + 1;
  }
  size_t len = strlen(row->cells[0]);
  if (len < 2 || row->cells[0][0] != '`' || row->cells[0][len - 1] != '`') return 0;
  row->cells[0][len - 1] = '\0';
  row->cells[0]++;
  return 1;
}

/* Runs `build/langc explain-code CODE` for the doc row ROW and compares its
   stdout with the JSON line of ROW. Returns the number of FAIL lines. */
static int check_doc_row(const DocRow *row) {
  static const char *const NAMES[CELL_COUNT] = {"code", "severity", "stage", "meaning"};
  const char *values[CELL_COUNT] = {row->cells[0], row->cells[1], row->cells[2], row->cells[3]};
  char command[256];
  Lines out = {NULL, 0};
  char *want = json_line(NAMES, values, CELL_COUNT);
  snprintf(command, sizeof command, "build/langc explain-code '%s'", row->cells[0]);
  int status = run_command(command, &out);
  int ok = want != NULL && status == 0 && out.count == 1 && strcmp(out.lines[0], want) == 0;
  if (!ok) printf("FAIL: explain-code %s: exit %d, %zu lines, not the doc row\n", row->cells[0], status, out.count);
  free(want);
  free_lines(&out);
  return !ok;
}

/* Compares the table row ROW cell for cell with the doc row of its code in
   the COUNT ROWS. Returns the number of FAIL lines. */
static int check_table_row(const CodeRow *row, const DocRow *rows, size_t count) {
  const char *cells[CELL_COUNT] = {row->code, row->severity, row->stage, row->meaning};
  const DocRow *doc = NULL;
  for (size_t i = 0; i < count && doc == NULL; i++) doc = strcmp(rows[i].cells[0], row->code) == 0 ? &rows[i] : NULL;
  if (doc == NULL) {
    printf("FAIL: explain-code: the table code %s has no row in %s\n", row->code, DOC_PATH);
    return 1;
  }
  int fails = 0;
  for (int c = 1; c < CELL_COUNT; c++) {
    if (strcmp(cells[c], doc->cells[c]) != 0) {
      printf("FAIL: explain-code %s: cell %d of the table is not the doc cell\n", row->code, c + 1);
      fails++;
    }
  }
  return fails;
}

/* Part 1. Returns the number of FAIL lines. */
static int check_codes(void) {
  Lines doc = {NULL, 0};
  int fails = read_file(DOC_PATH, &doc) ? 0 : 1;
  DocRow *rows = calloc(doc.count + 1, sizeof *rows);
  size_t count = 0;
  if (fails > 0 || rows == NULL) printf("FAIL: explain-code: cannot read %s\n", DOC_PATH);
  for (size_t i = 0; rows != NULL && i < doc.count; i++) {
    if (strncmp(doc.lines[i], "| `", 3) != 0) continue;
    if (split_row(doc.lines[i], &rows[count])) {
      count++;
    } else {
      printf("FAIL: explain-code: line %zu of %s is not a row of 5 cells\n", i + 1, DOC_PATH);
      fails++;
    }
  }
  /* Doc to table: the stdout of `langc explain-code` for each doc row. */
  for (size_t i = 0; i < count; i++) fails += check_doc_row(&rows[i]);
  /* Table to doc: each row of the C table has a doc row with equal cells. */
  for (size_t i = 0; rows != NULL && i < code_count(); i++) fails += check_table_row(code_at(i), rows, count);
  if (code_count() != count) {
    printf("FAIL: explain-code: the table has %zu rows, %s has %zu\n", code_count(), DOC_PATH, count);
    fails++;
  }
  if (fails == 0) {
    printf("explain-code: %zu codes match %s\n", count, DOC_PATH);
  } else {
    printf("explain-code: %d mismatches\n", fails);
  }
  free(rows);
  free_lines(&doc);
  return fails;
}

/* Part 2: the constructors are the lines after `data Step` that start with
   two spaces; the name is the first word. Returns the number of FAIL
   lines. */
static int check_steps(void) {
  static const char *const NAMES[2] = {"step", "form"};
  Lines domain = {NULL, 0};
  Lines out = {NULL, 0};
  int read = read_file(DOMAIN_PATH, &domain);
  int status = run_command("build/langc list-steps", &out);
  int fails = 0;
  if (!read || status != 0) {
    printf("FAIL: list-steps: cannot read %s, or build/langc list-steps gave exit %d\n", DOMAIN_PATH, status);
    fails++;
  }
  size_t start = 0;
  while (start < domain.count && strncmp(domain.lines[start], "data Step ", 10) != 0) start++;
  size_t count = 0;
  for (size_t i = start + 1; i < domain.count && strncmp(domain.lines[i], "  ", 2) == 0; i++, count++) {
    char *name = domain.lines[i] + 2;
    name[strcspn(name, " ")] = '\0';
    const StepRow *row = step_at(count);
    const char *values[2] = {name, row != NULL ? row->form : ""};
    char *want = json_line(NAMES, values, 2);
    if (want == NULL || count >= out.count || strcmp(out.lines[count], want) != 0) {
      printf("FAIL: list-steps: line %zu is not the step %s of %s\n", count + 1, name, DOMAIN_PATH);
      fails++;
    }
    free(want);
  }
  if (count == 0 || count != out.count) {
    printf("FAIL: list-steps: %s has %zu steps, build/langc list-steps gave %zu lines\n", DOMAIN_PATH, count, out.count);
    fails++;
  }
  if (fails == 0) {
    printf("list-steps: %zu steps match %s\n", count, DOMAIN_PATH);
  } else {
    printf("list-steps: %d mismatches\n", fails);
  }
  free_lines(&domain);
  free_lines(&out);
  return fails;
}

int main(void) {
  int fails = check_codes();
  fails += check_steps();
  return fails == 0 ? 0 : 1;
}
