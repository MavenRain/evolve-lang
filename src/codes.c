/* The table of `langc explain-code` (Q-S8-7 (a)). Each row is the row of
   docs/DIAGNOSTICS.md with the same code, in the same order; the meaning is
   the Meaning cell verbatim. The gate (test/verbs.c) compares the two tables
   cell for cell, both ways. */
#include <string.h>

#include "codes.h"

static const CodeRow CODE_ROWS[] = {
  {"LEX_CHAR", "error", "lex",
   "A character that starts no token: a non-ASCII character other than `→`, `λ`, `×` and `ℕ`, "
   "or an ASCII character that the lexer does not use."},
  {"LEX_NAT_RANGE", "error", "lex",
   "A numeral larger than 2^64-1."},
  {"PARSE_EXPECT", "error", "parse",
   "A token that the grammar does not permit at that position."},
  {"PARSE_DEPTH", "error", "parse",
   "The terms nest deeper than the parser limit."},
  {"PARSE_CLAUSE_NAME", "error", "parse",
   "A clause name is not the name of the signature above it, or a clause has no signature."},
  {"PARSE_CTOR_RESULT", "error", "parse",
   "A constructor type does not end in the data type applied to its parameters in order, or a field "
   "has no name."},
  {"REFUSE_DATA", "error", "check",
   "A program declares a data type with no type argument in its parameters or indices (rule R2). "
   "Only the domain can declare a base type."},
  {"REFUSE_CELL", "error", "check",
   "A program data type with a type argument is not a cell signature (rule R1). A cell signature "
   "has only universe parameters and no type index. Each field is a type parameter (then it is the "
   "one field of its constructor), closed data, the family at its own parameters or a path in the "
   "family. Each type parameter has exactly one generator: a point constructor with one field of "
   "that type. A path constructor has the type `Path T s t`, where T is the family at its own "
   "parameters and s, t are point terms; S2 has dimension 1 only, so a square is refused. A program "
   "`codata` type is a cocell signature (the CC criterion): one type parameter, no index and one "
   "clause that is not a path constructor. Each field is the observation A, a field D A or a "
   "labelled field E → D A, where E is closed and E is in the universe of A. There is exactly one "
   "observation."},
  {"REFUSE_AXIOM", "error", "check",
   "A program declares a postulate."},
  {"REFUSE_REC", "error", "check",
   "A `rec` declaration, or a definition that refers to itself or to a later definition (rule R3). "
   "Use `fold`."},
  {"DOMAIN_FAMILY", "error", "check",
   "A family or a fold is outside the S2 data rules. A recursive field must have the type of its "
   "family, at the parameters of the family. A fold over an indexed family or a higher inductive "
   "family needs a dependent eliminator (S3). A path constructor is only in a cell signature."},
  {"REFUSE_NAME", "error", "check",
   "A declaration uses a core name, a domain name, a reserved word or a name that is already "
   "declared. A `codata` field must not be the type name, the clause name or a name that is already "
   "a field of the clause."},
  {"REFUSE_RENAME_INPLACE", "error", "check",
   "A protocol has a `renameColumn` step. A rename in place breaks each live old version at once. "
   "Use the five-stage protocol of examples/rename. `found` is `renameColumn`. The span is the step "
   "word."},
  {"REFUSE_STEP_INDEX", "error", "check",
   "A protocol step is not legal at its source stage, because a conjunct of the `pre` function of "
   "the step is false. The message gives the step number, the step, the source stage, the first "
   "false conjunct and the arguments of the `pre` function. `expected` is the false conjunct and "
   "`found` is `false`. The span is the step word."},
  {"REFUSE_DROP_REQUIRED", "error", "check",
   "A `dropColumn t c` step drops a column that a live version reads, writes or inserts (J4, PLAN "
   "4.4). The message is `stage N (STEP): the live version V WHEN; HINT`, where N is the stage "
   "after the step. The hint is \"retire V before stage N\". `expected` is null and `found` is the "
   "version. The span is the step word."},
  {"REFUSE_MISSING_COLUMN", "error", "check",
   "A live version reads, writes or inserts a column that is not present at a stage (J2.1). The "
   "message has the form of `REFUSE_DROP_REQUIRED`, and at stage 0 it starts `stage 0: the live "
   "version V`. The hint is \"move the deploy of V after stage N\". `expected` is null and `found` is "
   "the version. The span is the step word, or the app name at stage 0."},
  {"REFUSE_STALE_READ", "error", "check",
   "A live version reads a column that another live version updates through a different column, and "
   "no sync trigger joins the two columns (J2.2). The message has the form of "
   "`REFUSE_DROP_REQUIRED`. The hint is \"install a sync trigger before stage N\". `expected` is null "
   "and `found` is the version. The span is the step word, or the app name at stage 0."},
  {"REFUSE_UNFILLED_READ", "error", "check",
   "A live version reads a column that the protocol adds, and no earlier backfill fills it (J2.3). "
   "The message has the form of `REFUSE_DROP_REQUIRED`. The hint is \"add a backfill step before the "
   "deploy of V\". `expected` is null and `found` is the version. The span is the step word, or the "
   "app name at stage 0."},
  {"REFUSE_NULL_INSERT", "error", "check",
   "A live version inserts into a table, and a not-null or CHECK column of that table gets no value "
   "from the insert or from a sync trigger (J2.4). The message has the form of "
   "`REFUSE_DROP_REQUIRED`. The hint is \"install a sync trigger, or retire V before stage N\". "
   "`expected` is null and `found` is the version. The span is the step word, or the app name at "
   "stage 0."},
  {"REFUSE_LOCK_SCAN", "error", "check",
   "A step scans its table under an ACCESS EXCLUSIVE lock (J5, PLAN 5.1), for example `setNotNull` "
   "with no valid CHECK before it. The message is `stage N (STEP): the step scans its table under "
   "an ACCESS EXCLUSIVE lock; HINT`. The hint is \"add addNotNullCheck and validate before "
   "setNotNull\". `expected` is null and `found` is the step word. The span is the step word."},
  {"TYPE_REFL", "error", "check",
   "`refl` where the two sides do not have the same normal form. The message gives the two normal "
   "forms."},
  {"TYPE_MISMATCH", "error", "check",
   "A term does not have the type that its position needs."},
  {"NAME_UNKNOWN", "error", "check",
   "A name that is not in scope. The span is the name. For a kit name that S1 or S2 removed, the "
   "message says what to use. `fix` is the new name for `Flag` (`Bool`), `flagYes` (`true`), "
   "`flagNo` (`false`), `pair` (`pack`), `first` (`witness`), `second` (`payload`) and `Eq` "
   "(`Path`). `fix` is `null` for `Prod`, `List`, `cons`, `filter` and `unfold`, because a name "
   "cannot replace them."},
  {"TYPE_UNIVERSE", "error", "check",
   "A universe level that is out of range."},
  {"TYPE_INFER", "error", "check",
   "The checker cannot find the type of a term at that position. Add a type: an annotated binder or "
   "a signature."},
  {"TYPE_ARITY", "error", "check",
   "A term has the wrong number of arguments."},
  {"TYPE_NOT_FUNCTION", "error", "check",
   "An application of a term that is not a function."},
  {"TYPE_PRETYPE", "error", "check",
   "A pretype where the checker needs a type in a universe: the type of a definition, a field or a "
   "parameter, a part of a Σ type, or the type argument of `Path` or `Option`. The pretypes are "
   "`I` and the function types over `I`. Their sort is SSet, which is in no universe."},
  {"TYPE_ENDPOINT", "error", "check",
   "A path `λ i → t` where `t` at `i0` or at `i1` does not have the same normal form as the end "
   "that the `Path` or `PathP` type gives. The message gives the two normal forms."},
  {"TYPE_SYSTEM", "error", "check",
   "A system `[ φ ↦ u , ... ]` that does not agree with its `Partial ψ A` type: the join of the "
   "cell faces is not `ψ`, two branches do not have the same normal form where their faces meet, a "
   "face overlap is not in the face bounds (16 meets of 8 literals), or the system has more than 16 "
   "cells. An `hcomp` face that depends on its side binder is also refused. The message identifies "
   "the failing condition."},
  {"TYPE_LINE", "error", "check",
   "A `transp A r u` where the line `A` is not constant where `r` holds. The checker compares `A i` "
   "with `A i0` on each meet of `r` and of the faces of the enclosing system cells. An atom in a "
   "meet stays free, so the test is conservative there. A face that is not in the face bounds needs "
   "a constant line. The message gives the line."},
  {"GLUE_EQUIV", "error", "check",
   "A `Glue A [ φ ↦ pack T e , ... ]` where a branch is not `pack T e` with `T : Type 0` and `e "
   ": Equiv T A`. The message keeps the code and the message of the failure in the branch."},
  {"GLUE_COVER", "error", "check",
   "A `glue [ φ ↦ t , ... ] a` against a type `Glue A [ ψ ↦ ... ]` where the join of the "
   "faces of `glue` is not `ψ`."},
  {"GLUE_BOUNDARY", "error", "check",
   "A `glue [ φ ↦ t , ... ] a` where, on the face `φ` of a branch `pack T e` of its type, the "
   "witness `e t` is not equal to the base `a`."},
  {"UNGLUE_TYPE", "error", "check",
   "An `unglue u` where the type of `u` is not a type `Glue A sys`."},
  {"W_NONCANON", "warning", "check",
   "The first declaration (or the tail of the file) whose text is not the `langc fmt` text. `found` "
   "is the input text; `expected` and `fix` are the canonical text."},
  {"REFUSE_RUNTIME_CLOSURE", "error", "lowering",
   "A function value must exist at run time. The Wasm target cannot hold one (HOST-LIMIT)."},
  {"REFUSE_RUNTIME_TREE", "error", "lowering",
   "A fold over a tree must run at run time. The Wasm target cannot run it (HOST-LIMIT)."},
  {"REFUSE_RUNTIME_PATH", "error", "lowering",
   "An interval point or a path value must exist at run time, for example in the state of a fold. A "
   "path is a proof. It has no run-time form."},
  {"REFUSE_RUNTIME_GLUE", "error", "lowering",
   "A Glue type, a `glue` term or an `unglue` term must exist at run time, for example in the state "
   "of a fold. A Glue value is a proof. It has no run-time form."},
  {"EVAL_ARGS", "error", "eval",
   "The arguments of `langc eval` do not agree with the entry (count or range)."},
  {"EVAL_ENTRY", "error", "eval",
   "`langc eval` names no definition, or the entry does not compute a number."},
  {"EVAL_PRINT", "error", "eval",
   "The value is too large to print."},
  {"EVAL_OVERFLOW", "error", "eval",
   "A ℕ operation overflows past 2^64-1."},
  {"EVAL_DEPTH", "error", "eval, lowering",
   "The evaluation or the lowering nests deeper than 2000 levels."},
  {"EVAL_FUEL", "error", "eval",
   "The evaluation takes more than 20000000 steps."},
  {"SQL_NAME_LONG", "error", "sql",
   "A generated name of `langc sql` (`evolve_P_NN_sync`, `_fill`, `_nn` or `_idx`), or a table or "
   "column name used by the protocol, is longer than 63 bytes, the PostgreSQL name limit. Shorten "
   "the relevant protocol, table or column name."},
  {"WASM_LIMIT", "error", "wasm",
   "A stage of the protocol has more than 32 guard facts. The i32 guard mask of the `langc wasm` "
   "module holds a maximum of 32 facts. `langc wasm` writes no module."},
  {"INTERNAL", "error", "check, eval, lowering",
   "An internal rule of `langc` fails. This is a defect in `langc`."},
  {"IO", "error", "all",
   "`langc` cannot read the program file or write its output."},
  {"IO_SIZE", "error", "all",
   "The program file is larger than the size limit."},
  {"OOM", "error", "all",
   "`langc` has no free memory."},
  {"USAGE", "error", "all",
   "The command line is not valid."},
};

size_t code_count(void) { return sizeof CODE_ROWS / sizeof CODE_ROWS[0]; }

const CodeRow *code_at(size_t index) { return index < code_count() ? &CODE_ROWS[index] : NULL; }

const CodeRow *code_find(const char *code) {
  for (size_t i = 0; i < code_count(); i++) {
    if (strcmp(CODE_ROWS[i].code, code) == 0) return &CODE_ROWS[i];
  }
  return NULL;
}
