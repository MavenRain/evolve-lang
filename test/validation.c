#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The law-id check of the map in docs/VALIDATION.md (Q-S10-2 (c), Q-S10-9
   (a)). Run it from the repo root: `build/validation-test [PLAN VALIDATION]`
   (docs/PLAN.md and docs/VALIDATION.md when not given). It reads the ids of
   the `- L<n> ` items in PLAN 6.1 and of the `- J<n> ` items in PLAN 4.2,
   and the rows of the table between the laws markers of VALIDATION. Each
   PLAN id has exactly one row, each row id is a PLAN id, and each
   backticked span of a tests cell that contains `/` names a path that
   exists. It prints a FAIL line for each mismatch, then `validation: L laws,
   J judgements, R rows, K mismatches`, and exits 0 only when K is 0. A usage
   error, a read error (including a NUL byte), a PLAN section with no items,
   a PLAN item that does not fit (MAX_IDS, ID_SIZE) or a file with no laws
   region prints one line on stderr and exits 2. */

#define USAGE "usage: validation-test [PLAN VALIDATION]\n"
#define DEFAULT_PLAN "docs/PLAN.md"
#define DEFAULT_VALIDATION "docs/VALIDATION.md"
#define LAW_HEADING "### 6.1 Laws"
#define JUDGEMENT_HEADING "### 4.2 Judgements"
#define BEGIN_MARK "<!-- laws begin -->"
#define END_MARK "<!-- laws end -->"
#define TABLE_HEADER "| id | statement | tests |"
#define CELL_COUNT 3
#define MAX_CELLS 4
#define MAX_IDS 32
#define ID_SIZE 8

/* The lines of a file, without their newlines. */
typedef struct {
  char **lines;
  size_t count;
} Lines;

/* The ids of the PLAN items in PLAN order, with the row count of each. */
typedef struct {
  char ids[MAX_IDS][ID_SIZE];
  size_t rows[MAX_IDS];
  size_t count;
} Ids;

static void free_lines(Lines *lines) {
  for (size_t i = 0; i < lines->count; i++) free(lines->lines[i]);
  free(lines->lines);
  lines->lines = NULL;
  lines->count = 0;
}

/* Reads the lines of FILE into LINES. Returns 0 on a read error, a NUL
   byte or when there is no memory. */
static int read_stream(FILE *file, Lines *lines) {
  char *line = NULL;
  size_t size = 0;
  ssize_t len;
  int ok = 1;
  while (ok && (len = getline(&line, &size, file)) >= 0) {
    if (memchr(line, '\0', (size_t)len) != NULL) {
      ok = 0;
      break;
    }
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

/* Returns the index of the first line from FROM that is TEXT, or the line
   count when there is no such line. */
static size_t find_exact(const Lines *lines, size_t from, const char *text) {
  size_t i = from;
  while (i < lines->count && strcmp(lines->lines[i], text) != 0) i++;
  return i;
}

/* Adds to IDS the ids of the `- LETTER<n> ` items of the PLAN section under
   the line HEADING. The section stops at the next heading. Returns the
   number of ids that it adds, -1 when PLAN has no line HEADING, or -2 when
   an item does not fit in IDS (more than MAX_IDS ids, or an id of more than
   ID_SIZE - 1 bytes). */
static int read_ids(const Lines *plan, const char *heading, char letter, Ids *ids) {
  size_t at = find_exact(plan, 0, heading);
  if (at >= plan->count) return -1;
  int added = 0;
  for (at++; at < plan->count && plan->lines[at][0] != '#'; at++) {
    const char *line = plan->lines[at];
    size_t digits = line[0] == '-' && line[1] == ' ' && line[2] == letter ? strspn(line + 3, "0123456789") : 0;
    if (digits == 0 || line[3 + digits] != ' ') continue;
    if (digits + 2 > ID_SIZE || ids->count >= MAX_IDS) return -2;
    memcpy(ids->ids[ids->count], line + 2, digits + 1);
    ids->ids[ids->count][digits + 1] = '\0';
    ids->rows[ids->count++] = 0;
    added++;
  }
  return added;
}

/* Returns the index of the id TEXT in IDS, or the id count when it is not
   there. */
static size_t find_id(const Ids *ids, const char *text) {
  size_t i = 0;
  while (i < ids->count && strcmp(ids->ids[i], text) != 0) i++;
  return i;
}

/* Cuts spaces and tabs at the two ends of CELL. */
static char *trim(char *cell) {
  char *start = cell + strspn(cell, " \t");
  size_t len = strlen(start);
  while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t')) start[--len] = '\0';
  return start;
}

/* Splits the table row LINE in place into its cells (the text between two
   `|`, trimmed). Returns the cell count, at most MAX_CELLS. */
static size_t split_cells(char *line, char **cells) {
  size_t count = 0;
  char *at = strchr(line, '|');
  char *end = at == NULL ? NULL : strchr(at + 1, '|');
  while (end != NULL && count < MAX_CELLS) {
    *end = '\0';
    cells[count++] = trim(at + 1);
    at = end;
    end = strchr(at + 1, '|');
  }
  /* A fourth cell need not end in a pipe. Do not discard its bytes. */
  if (at != NULL && *trim(at + 1) != '\0') return MAX_CELLS;
  return count;
}

/* Checks each backticked span of the tests cell CELL of row N in place: a
   span that contains `/` must name a path that exists. Returns the FAIL
   count. */
static int check_paths(char *cell, size_t n) {
  int fails = 0;
  char *at = strchr(cell, '`');
  while (at != NULL) {
    char *end = strchr(at + 1, '`');
    if (end == NULL) {
      printf("FAIL: validation: row %zu: the tests cell has an odd number of backticks\n", n);
      return fails + 1;
    }
    *end = '\0';
    if (strchr(at + 1, '/') != NULL && access(at + 1, F_OK) != 0) {
      printf("FAIL: validation: row %zu: %s does not exist\n", n, at + 1);
      fails++;
    }
    at = strchr(end + 1, '`');
  }
  return fails;
}

/* Checks row N of the table (the line LINE) and counts the row on its id in
   IDS. Returns the FAIL count. */
static int check_row(char *line, size_t n, Ids *ids) {
  char *cells[MAX_CELLS];
  if (line[0] != '|' || split_cells(line, cells) != CELL_COUNT) {
    printf("FAIL: validation: row %zu does not have %d cells\n", n, CELL_COUNT);
    return 1;
  }
  size_t i = find_id(ids, cells[0]);
  int extra = i == ids->count;
  if (extra) {
    printf("FAIL: validation: row %zu: %s is not an id of PLAN 6.1 or 4.2\n", n, cells[0]);
  } else {
    ids->rows[i]++;
  }
  return extra + check_paths(cells[2], n);
}

/* Checks the table between the laws markers of DOC (the lines BEGIN and
   END). Returns the FAIL count, and the row count in *ROWS. */
static int check_table(const Lines *doc, size_t begin, size_t end, Ids *ids, size_t *rows) {
  size_t at = begin + 1;
  while (at < end && trim(doc->lines[at])[0] != '|') at++;
  *rows = 0;
  if (at + 1 >= end || strcmp(trim(doc->lines[at]), TABLE_HEADER) != 0 || strncmp(trim(doc->lines[at + 1]), "|---", 4) != 0) {
    printf("FAIL: validation: the laws region has no table %s\n", TABLE_HEADER);
    return 1;
  }
  int fails = 0;
  for (at += 2; at < end; at++) {
    char *line = trim(doc->lines[at]);
    if (*line != '\0') fails += check_row(line, ++*rows, ids);
  }
  return fails;
}

/* Checks that each id of IDS has exactly one row. Returns the FAIL count. */
static int check_ids(const Ids *ids) {
  int fails = 0;
  for (size_t i = 0; i < ids->count; i++) {
    if (ids->rows[i] == 0) {
      printf("FAIL: validation: %s has no row\n", ids->ids[i]);
      fails++;
    }
    if (ids->rows[i] > 1) {
      printf("FAIL: validation: %s has %zu rows\n", ids->ids[i], ids->rows[i]);
      fails++;
    }
  }
  return fails;
}

/* Prints `validation: PATH: TEXT` on stderr. Returns 2. */
static int setup_error(const char *path, const char *text) {
  fprintf(stderr, "validation: %s: %s\n", path, text);
  return 2;
}

/* Flush before reporting success: stdio can defer a write failure until
   after the last printf. */
static int output_status(int status) {
  if (fflush(stdout) == 0 && !ferror(stdout)) return status;
  fputs("validation: cannot write stdout\n", stderr);
  return 2;
}

/* Runs the check on the files PLAN_PATH and DOC_PATH, read into PLAN and
   DOC. Returns the exit code. */
static int run(const char *plan_path, const char *doc_path, Lines *plan, Lines *doc) {
  if (!read_file(plan_path, plan)) return setup_error(plan_path, "cannot read");
  if (!read_file(doc_path, doc)) return setup_error(doc_path, "cannot read");
  Ids ids;
  ids.count = 0;
  int laws = read_ids(plan, LAW_HEADING, 'L', &ids);
  int judgements = read_ids(plan, JUDGEMENT_HEADING, 'J', &ids);
  if (laws == -2 || judgements == -2) return setup_error(plan_path, "an L or J item does not fit (too many ids, or an id too long)");
  if (laws <= 0) return setup_error(plan_path, "no L items under " LAW_HEADING);
  if (judgements <= 0) return setup_error(plan_path, "no J items under " JUDGEMENT_HEADING);
  size_t begin = find_exact(doc, 0, BEGIN_MARK);
  size_t end = find_exact(doc, begin + 1, END_MARK);
  if (end >= doc->count) return setup_error(doc_path, "no laws region");
  size_t rows = 0;
  int fails = check_table(doc, begin, end, &ids, &rows);
  fails += check_ids(&ids);
  printf("validation: %d laws, %d judgements, %zu rows, %d mismatches\n", laws, judgements, rows, fails);
  return output_status(fails == 0 ? 0 : 1);
}

int main(int argc, char **argv) {
  if (argc != 1 && argc != 3) {
    fputs(USAGE, stderr);
    return 2;
  }
  Lines plan = {NULL, 0};
  Lines doc = {NULL, 0};
  int code = run(argc == 3 ? argv[1] : DEFAULT_PLAN, argc == 3 ? argv[2] : DEFAULT_VALIDATION, &plan, &doc);
  free_lines(&plan);
  free_lines(&doc);
  return code;
}
