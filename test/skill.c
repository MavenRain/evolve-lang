#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/* The test of skills/evolve-lang/SKILL.md (Q-S8-9 (a), Q-S8-10 (a)). Run it
   from the repo root. For each example, it compares the program block (the
   fence `evolve-lang PATH`) with the file PATH byte for byte, compares the
   stdout of `build/langc check --json PATH` with the json block line for
   line, and compares the exit code with the sentence `The exit code is N.`
   Then it compares examples 1, 2 and 4 with their sources. It prints a FAIL
   line for each mismatch, then `skill: N examples, M outputs match`, and
   exits 0 only when there is no FAIL line. */

#define SKILL_PATH "skills/evolve-lang/SKILL.md"
#define EXAMPLE_COUNT 5
#define PROGRAM_FENCE "```evolve-lang "
#define JSON_FENCE "```json"
#define END_FENCE "```"
#define EXIT_PREFIX "The exit code is "

/* The characters of a path that the test puts on a command line. */
#define PATH_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/._-"

/* Examples 1, 2 and 4 are copies (Q-S8-10 (a)): each pair is the example and
   its source. */
#define COPY_COUNT 3
static const char *const COPIES[COPY_COUNT][2] = {
  {"examples/agent/01-add-column.lang", "test/report/single.lang"},
  {"examples/agent/02-rename.lang", "examples/rename.lang"},
  {"examples/agent/04-not-null.lang", "test/report/strict.lang"},
};

/* The lines of a file or of the stdout of a command, without their
   newlines. */
typedef struct {
  char **lines;
  size_t count;
} Lines;

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

/* Returns the index of the first line at FROM or after it that starts with
   PREFIX, or the line count when there is none. */
static size_t find_line(const Lines *lines, size_t from, const char *prefix) {
  size_t i = from;
  while (i < lines->count && strncmp(lines->lines[i], prefix, strlen(prefix)) != 0) i++;
  return i;
}

/* Returns 1 when the bytes of the file PATH are the lines FROM to TO - 1 of
   LINES, each with its newline. */
static int same_as_block(const char *path, const Lines *lines, size_t from, size_t to) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) return 0;
  int same = 1;
  for (size_t i = from; same && i < to; i++) {
    for (const char *c = lines->lines[i]; same && *c != '\0'; c++) same = getc(file) == (unsigned char)*c;
    same = same && getc(file) == '\n';
  }
  same = same && getc(file) == EOF;
  return fclose(file) == 0 && same;
}

/* Returns 1 when the files A and B have the same bytes. */
static int same_files(const char *a, const char *b) {
  FILE *fa = fopen(a, "rb");
  FILE *fb = fopen(b, "rb");
  int same = fa != NULL && fb != NULL;
  int c = 0;
  while (same && c != EOF) {
    c = getc(fa);
    same = c == getc(fb);
  }
  if (fa != NULL) fclose(fa);
  if (fb != NULL) fclose(fb);
  return same;
}

/* Checks example N, whose program fence is at line AT of SKILL. Sets *NEXT
   to the line after the exit sentence and *MATCH to 1 when the output and
   the exit code match. Returns the number of FAIL lines. */
static int check_example(const Lines *skill, size_t at, size_t n, size_t *next, int *match) {
  const char *path = skill->lines[at] + strlen(PROGRAM_FENCE);
  size_t to = find_line(skill, at + 1, END_FENCE);
  size_t json = find_line(skill, to + 1, JSON_FENCE);
  size_t json_to = find_line(skill, json + 1, END_FENCE);
  size_t exit_at = find_line(skill, json_to + 1, EXIT_PREFIX);
  *next = exit_at + 1;
  *match = 0;
  if (exit_at >= skill->count || strspn(path, PATH_CHARS) != strlen(path)) {
    printf("FAIL: skill: example %zu (%s) has no program, json or exit block\n", n, path);
    return 1;
  }
  int fails = 0;
  if (!same_as_block(path, skill, at + 1, to)) {
    printf("FAIL: skill: example %zu: the program block is not %s\n", n, path);
    fails++;
  }
  const char *text = skill->lines[exit_at] + strlen(EXIT_PREFIX);
  char *end = NULL;
  long want = strtol(text, &end, 10);
  char command[512];
  Lines out = {NULL, 0};
  int code = snprintf(command, sizeof command, "build/langc check --json %s", path) < (int)sizeof command ? run_command(command, &out) : -1;
  int same = out.count == json_to - json - 1;
  for (size_t i = 0; same && i < out.count; i++) same = strcmp(out.lines[i], skill->lines[json + 1 + i]) == 0;
  if (!same) {
    printf("FAIL: skill: example %zu: the stdout of check --json %s is not the json block\n", n, path);
    fails++;
  }
  if (end == text || strcmp(end, ".") != 0 || code != want) {
    printf("FAIL: skill: example %zu: exit %d, the sentence gives '%s'\n", n, code, text);
    fails++;
  }
  *match = fails == 0;
  free_lines(&out);
  return fails;
}

int main(void) {
  Lines skill = {NULL, 0};
  int fails = read_file(SKILL_PATH, &skill) ? 0 : 1;
  if (fails > 0) printf("FAIL: skill: cannot read %s\n", SKILL_PATH);
  size_t examples = 0;
  size_t matches = 0;
  size_t at = find_line(&skill, 0, PROGRAM_FENCE);
  while (at < skill.count) {
    size_t next = 0;
    int match = 0;
    fails += check_example(&skill, at, ++examples, &next, &match);
    matches += (size_t)match;
    at = find_line(&skill, next, PROGRAM_FENCE);
  }
  if (examples != EXAMPLE_COUNT) {
    printf("FAIL: skill: %s has %zu examples, not %d\n", SKILL_PATH, examples, EXAMPLE_COUNT);
    fails++;
  }
  for (size_t i = 0; i < COPY_COUNT; i++) {
    if (same_files(COPIES[i][0], COPIES[i][1])) continue;
    printf("FAIL: skill: %s is not a copy of %s\n", COPIES[i][0], COPIES[i][1]);
    fails++;
  }
  if (fails == 0) {
    printf("skill: %zu examples, %zu outputs match\n", examples, matches);
  } else {
    printf("skill: %d mismatches\n", fails);
  }
  free_lines(&skill);
  return fails == 0 ? 0 : 1;
}
