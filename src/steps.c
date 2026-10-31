/* The table of `langc list-steps` (Q-S8-8 (a)): the Step constructors in
   the order of domain/domain.lang, each with its surface form (SPEC 5.2).
   The gate (test/verbs.c) compares the names and the order with the
   domain. */
#include "steps.h"

static const StepRow STEP_ROWS[] = {
  {"addColumn", "addColumn t c ty"},
  {"installSync", "installSync t c c'"},
  {"dropSync", "dropSync t c c'"},
  {"backfill", "backfill t c ← c' batch N"},
  {"deploy", "deploy a"},
  {"retire", "retire a"},
  {"dropColumn", "dropColumn t c"},
  {"addNotNullCheck", "addNotNullCheck t c"},
  {"validate", "validate t c"},
  {"setNotNull", "setNotNull t c"},
  {"dropCheck", "dropCheck t c"},
  {"createIndexConcurrently", "createIndexConcurrently t c"},
};

size_t step_count(void) { return sizeof STEP_ROWS / sizeof STEP_ROWS[0]; }

const StepRow *step_at(size_t index) { return index < step_count() ? &STEP_ROWS[index] : NULL; }
