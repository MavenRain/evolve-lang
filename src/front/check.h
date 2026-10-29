/* The type checker, the refusals R1-R4 and the eval command. SHARED by the
   tcc kits. */
#ifndef LANG_FRONT_CHECK_H
#define LANG_FRONT_CHECK_H

#include "front/core.h"

#define ENTRY_PARAMS_MAX 16u

/* An entry: a program definition of type Nat, Bool, or a function of Nat and
   Bool arguments to a Nat or Bool result. */
typedef struct {
  uint32_t param_count;
  int param_bool[ENTRY_PARAMS_MAX];
  int result_bool;
} Entry;

/* The data of `langc report` (S5 U2): the stages, the live sets and the
   compatibility matrix of one protocol. All texts and arrays are on the
   arena of the check. */

/* A runtime fact: FACT is the guest form with the names (`retired v1`),
   GUARD the PLAN 4.5 sentence with the names. */
typedef struct {
  const char *fact;
  const char *guard;
} ReportFact;

/* Stage I: the step that gives it, its lockOf and scanOf constructors
   (all three NULL at stage 0) and the facts of the guest stageGuard. */
typedef struct {
  const char *step;
  const char *lock;
  const char *scan;
  ReportFact *guards;
  size_t guard_count;
} ReportStage;

typedef enum {
  REPORT_COMPATIBLE,
  REPORT_REFUSED,
  REPORT_RUNTIME
} ReportVerdict;

/* The guest compat verdict of one app at one stage: CODE and FIX for
   REPORT_REFUSED, FACT for REPORT_RUNTIME. */
typedef struct {
  int live;
  ReportVerdict verdict;
  const char *code;
  const char *fix;
  ReportFact fact;
} ReportCell;

/* The stages FIRST to LAST, with the same live apps. */
typedef struct {
  size_t first;
  size_t last;
} ReportSegment;

/* The caller sets PROTOCOL; the check sets the other fields when the
   program has that protocol (FOUND 1). There are STAGE_COUNT stages (the
   steps and stage 0). App A at stage I is CELLS[A * STAGE_COUNT + I]. */
typedef struct {
  const char *protocol;
  int found;
  const char **apps;
  size_t app_count;
  ReportStage *stages;
  size_t stage_count;
  ReportCell *cells;
  ReportSegment *segments;
  size_t segment_count;
} Report;

/* Checks the domain declarations, then the program, into M. Returns 1, or 0
   after a diagnostic. */
int check_program(Arena *arena, const DeclList *decls, Machine *m, Diag *diag);
/* As check_program; when REPORT is not NULL, also fills REPORT for the
   protocol REPORT->protocol. */
int check_program_report(Arena *arena, const DeclList *decls, Machine *m, Diag *diag, Report *report);
/* Returns 1 when TYPE is the type of an entry. */
int entry_of(Machine *m, const Value *type, Entry *out);
/* Evaluates the definition NAME on ARGS and prints the result. Returns 0, 1
   after a diagnostic, or 2 after an EVAL_ARGS diagnostic. */
int eval_command(Machine *m, const char *name, char *const *args, int arg_count, FILE *out, FILE *err);

#endif
