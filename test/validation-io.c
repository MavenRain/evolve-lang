#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Exercise the law-map CLI with complete inputs, including bytes and rows
   that a line or cell parser could otherwise silently discard. */
#define PLAN "### 6.1 Laws\n- L1 law\n### 4.2 Judgements\n- J1 judgement\n"
#define HEADER "<!-- laws begin -->\n| id | statement | tests |\n|---|---|---|\n"
#define LAW "| L1 | law | `test/validation.c` |\n"
#define JUDGEMENT "| J1 | judgement | `test/validation.c` |\n"
#define END "<!-- laws end -->\n"
#define DOC HEADER LAW JUDGEMENT END

static int write_file(const char *path, const char *text, size_t size) {
  FILE *file = fopen(path, "wb");
  if (file == NULL) return 0;
  int ok = fwrite(text, 1, size, file) == size;
  return fclose(file) == 0 && ok;
}

static int run(const char *plan, const char *doc, int output) {
  FILE *errors = tmpfile();
  if (errors == NULL) return -1;
  pid_t child = fork();
  if (child == 0) {
    if (dup2(output, STDOUT_FILENO) < 0 || dup2(fileno(errors), STDERR_FILENO) < 0) _exit(126);
    execl("build/validation-test", "build/validation-test", plan, doc, (char *)NULL);
    _exit(127);
  }
  int status = 0;
  int waited = child > 0 && waitpid(child, &status, 0) == child;
  fclose(errors);
  return waited && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

typedef struct {
  const char *name;
  const char *plan;
  size_t plan_size;
  const char *doc;
  size_t doc_size;
  int status;
  int readonly;
} Case;

#define CASE(NAME, PLAN_TEXT, DOC_TEXT, STATUS, READONLY) \
  {NAME, PLAN_TEXT, sizeof(PLAN_TEXT) - 1, DOC_TEXT, sizeof(DOC_TEXT) - 1, STATUS, READONLY}

int main(void) {
  const Case cases[] = {
    CASE("valid", PLAN, DOC, 0, 0),
    CASE("blank and indented rows", PLAN, HEADER LAW "\n  " JUDGEMENT END, 0, 0),
    CASE("unknown row after blank", PLAN, HEADER LAW JUDGEMENT "\n| L7 | extra | `test/validation.c` |\n" END, 1, 0),
    CASE("indented duplicate row", PLAN, HEADER LAW JUDGEMENT "  " JUDGEMENT END, 1, 0),
    CASE("unterminated fourth cell", PLAN, HEADER "| L1 | law | `test/validation.c` | extra\n" JUDGEMENT END, 1, 0),
    CASE("document with hidden bytes", PLAN, HEADER LAW JUDGEMENT "\0hidden\n" END, 2, 0),
    CASE("plan with hidden bytes", PLAN "\0hidden\n", DOC, 2, 0),
    CASE("unwritable stdout", PLAN, DOC, 2, 1)
  };
  char plan[] = "/tmp/evolve-validation-plan-XXXXXX";
  char doc[] = "/tmp/evolve-validation-doc-XXXXXX";
  int plan_fd = mkstemp(plan);
  int doc_fd = mkstemp(doc);
  FILE *output = tmpfile();
  int readonly = open("/dev/null", O_RDONLY);
  if (plan_fd >= 0) close(plan_fd);
  if (doc_fd >= 0) close(doc_fd);
  if (plan_fd < 0 || doc_fd < 0 || output == NULL || readonly < 0) {
    if (plan_fd >= 0) unlink(plan);
    if (doc_fd >= 0) unlink(doc);
    if (output != NULL) fclose(output);
    if (readonly >= 0) close(readonly);
    fputs("FAIL: validation IO: cannot create test files\n", stderr);
    return 1;
  }
  int fails = 0;
  size_t count = sizeof cases / sizeof cases[0];
  for (size_t i = 0; i < count; i++) {
    const Case *test = &cases[i];
    int actual = -1;
    if (write_file(plan, test->plan, test->plan_size) && write_file(doc, test->doc, test->doc_size)) {
      actual = run(plan, doc, test->readonly ? readonly : fileno(output));
    }
    if (actual != test->status) {
      printf("FAIL: validation IO: %s exits %d, expected %d\n", test->name, actual, test->status);
      fails++;
    }
  }
  unlink(plan);
  unlink(doc);
  close(readonly);
  fclose(output);
  printf("validation IO regressions: %zu cases, %d failures\n", count, fails);
  return fails == 0 ? 0 : 1;
}
