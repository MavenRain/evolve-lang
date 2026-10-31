#define _POSIX_C_SOURCE 200809L
#include "front/check.h"
#include "oracle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* The module that oracle_write gives for a hand-made Report: FOUND 1 when
   there is a file, its first LEN bytes, the status, and CLEAN 0 when the
   temporary directory could not be made or removed. */
typedef struct {
  unsigned char bytes[512];
  size_t len;
  int found;
  int status;
  int clean;
} Probe;

/* Writes REPORT with oracle_write to probe.wasm in an empty temporary
   directory of its own, reads the file back, then removes the file and the
   directory. */
static void probe_write(Arena *arena, Diag *diag, const Report *report, Probe *p) {
  static unsigned int serial = 0;
  char directory[512];
  char path[600];
  const char *base = getenv("TMPDIR");
  memset(p, 0, sizeof *p);
  p->status = -1;
  snprintf(directory, sizeof directory, "%s/evolve-oracle-%ld-%u", base == NULL || base[0] == '\0' ? "/tmp" : base,
           (long)getpid(), serial++);
  if (mkdir(directory, 0700) != 0) return;
  snprintf(path, sizeof path, "%s/probe.wasm", directory);
  p->status = oracle_write(arena, diag, report, path);
  p->clean = 1;
  FILE *file = fopen(path, "rb");
  if (file != NULL) {
    p->found = 1;
    p->len = fread(p->bytes, 1, sizeof p->bytes, file);
    if (fclose(file) != 0 || remove(path) != 0) p->clean = 0;
  }
  if (rmdir(directory) != 0) p->clean = 0;
}

/* Exercise the WASM_LIMIT bound of oracle_write (Q-S7-4 (a)). A checked
   program does not get to it: the check reaches the arena limit before a
   stage has 33 guard facts. So each case makes a Report by hand: one live
   compatible app, stage 0 with no facts and stage 1 with FACTS facts. A case
   with CODE wants that diagnostic and no file; a case with no CODE wants a
   module whose last byte (the fact count of stage 1) is FACTS. */
static int limit_case(const char *label, size_t facts, const char *code) {
  static ReportFact guards[ORACLE_FACTS_MAX + 1];
  static const char *apps[1] = {"v1"};
  Arena arena;
  Diag diag;
  ReportStage stages[2];
  ReportCell cells[2];
  Report report;
  Probe probe;
  size_t i;
  for (i = 0; i < facts; i++) {
    guards[i].fact = "fleetAt 0";
    guards[i].guard = "each tenant ledger shows stage 0 or later";
  }
  memset(stages, 0, sizeof stages);
  memset(cells, 0, sizeof cells);
  memset(&report, 0, sizeof report);
  stages[1].step = "deploy v1";
  stages[1].guards = guards;
  stages[1].guard_count = facts;
  for (i = 0; i < 2; i++) {
    cells[i].live = 1;
    cells[i].verdict = REPORT_COMPATIBLE;
  }
  report.protocol = "probe";
  report.found = 1;
  report.apps = apps;
  report.app_count = 1;
  report.stages = stages;
  report.stage_count = 2;
  report.cells = cells;
  arena_init(&arena, (size_t)1 << 20);
  diag_init(&diag);
  probe_write(&arena, &diag, &report, &probe);
  int ok = probe.clean;
  if (code == NULL) {
    ok = ok && probe.status == 0 && !diag.set && probe.found && probe.len > 8 && probe.len < sizeof probe.bytes &&
         memcmp(probe.bytes, "\0asm", 4) == 0 && (size_t)probe.bytes[probe.len - 1] == facts;
  } else {
    ok = ok && probe.status == 2 && diag.set && strcmp(diag.code, code) == 0 && !probe.found;
  }
  if (!ok) {
    fprintf(stderr, "FAIL oracle limit %s: exit %d\n", label, probe.status);
    if (diag.set) diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return ok;
}

/* The Code constructors in the order of `data Code` (domain/domain.lang). */
static const char *const DOMAIN_CODES[8] = {
  "REFUSE_DROP_REQUIRED", "REFUSE_MISSING_COLUMN", "REFUSE_STALE_READ", "REFUSE_UNFILLED_READ",
  "REFUSE_NULL_INSERT",   "REFUSE_STEP_INDEX",     "REFUSE_LOCK_SCAN",  "REFUSE_RENAME_INPLACE",
};

/* The cell byte of each verdict (Q-S7-3 (a)). The codes 1, 6, 7 and 8
   refuse the full protocol, so a checked program has no cell with them and
   no golden module shows them (S7 U3 part b, USER ruling 10-30). So the
   Report is made by hand: one live app at 10 stages with no facts, stage 0
   compatible, stage K (1 to 8) refused with Code K of DOMAIN_CODES, stage 9
   runtime. The data segment is the last section: its size 20, the 10 cell
   bytes 0 to 9, then the 10 fact counts 0. A case with UNKNOWN gives stage
   1 that code, which is not a Code, and wants INTERNAL and no file. */
static int code_case(const char *label, const char *unknown) {
  static const char *apps[1] = {"v1"};
  Arena arena;
  Diag diag;
  ReportStage stages[10];
  ReportCell cells[10];
  Report report;
  Probe probe;
  size_t i;
  memset(stages, 0, sizeof stages);
  memset(cells, 0, sizeof cells);
  memset(&report, 0, sizeof report);
  for (i = 0; i < 10; i++) {
    stages[i].step = i == 0 ? NULL : "deploy v1";
    cells[i].live = 1;
    cells[i].verdict = i == 0 ? REPORT_COMPATIBLE : i == 9 ? REPORT_RUNTIME : REPORT_REFUSED;
    cells[i].code = i == 0 || i == 9 ? NULL : DOMAIN_CODES[i - 1];
  }
  if (unknown != NULL) cells[1].code = unknown;
  report.protocol = "probe";
  report.found = 1;
  report.apps = apps;
  report.app_count = 1;
  report.stages = stages;
  report.stage_count = 10;
  report.cells = cells;
  arena_init(&arena, (size_t)1 << 20);
  diag_init(&diag);
  probe_write(&arena, &diag, &report, &probe);
  int ok = probe.clean;
  if (unknown == NULL) {
    ok = ok && probe.status == 0 && !diag.set && probe.found && probe.len > 29 && probe.len < sizeof probe.bytes &&
         memcmp(probe.bytes, "\0asm", 4) == 0 && probe.bytes[probe.len - 21] == 20;
    for (i = 0; i < 10 && ok; i++) ok = (size_t)probe.bytes[probe.len - 20 + i] == i && probe.bytes[probe.len - 10 + i] == 0;
  } else {
    ok = ok && probe.status == 2 && diag.set && strcmp(diag.code, "INTERNAL") == 0 && !probe.found;
  }
  if (!ok) {
    fprintf(stderr, "FAIL oracle code map %s: exit %d\n", label, probe.status);
    if (diag.set) diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return ok;
}

int main(void) {
  int ok = limit_case("33 facts", ORACLE_FACTS_MAX + 1, "WASM_LIMIT");
  ok &= limit_case("32 facts", ORACLE_FACTS_MAX, NULL);
  ok &= code_case("codes 0 to 9", NULL);
  ok &= code_case("unknown code", "REFUSE_WASM_LIMIT");
  return ok ? 0 : 1;
}
