/* The Wasm oracle writer of `langc wasm` (S7, PLAN 5.3): a Wasm 1.0 (MVP)
   module that holds the compatibility table of one checked protocol. The
   writer reads the Report of the check. It does not decide a verdict, and
   the module is not the type checker (Q3).

   The module has one data segment at offset 0 (Q-S7-5 (a)): one byte for
   each cell in the Report order (cell A * STAGE_COUNT + I), then one byte
   for each stage, its fact count. A cell byte is 0 for compatible, 1 to 8
   for refused (the Code in the domain order: dropRequired 1, missingColumn
   2, staleRead 3, unfilledRead 4, nullInsert 5, stepIndex 6, lockScan 7,
   renameInplace 8), and 9 for runtime (Q-S7-3 (a)). A cell that is not live
   keeps the code of its matrix row (Q-S5-9 (a)).

   The exports (Q-S7-2 (a)), each argument and result i32: `stages()`,
   `apps()`, `verdict(app, stage)` (the cell byte) and `guard(stage,
   facts)`. Bit K of FACTS (bit 0 first) is 1 when fact K of the stage
   guards holds. `guard` gives K + 1 for the lowest K below the fact count
   with bit K 0, else 0; the bits at K >= the fact count have no effect
   (Q-S7-4 (a), the mirror of domain.lang guardFirst). An app or a stage out
   of range traps (`unreachable`). */
#ifndef LANG_ORACLE_H
#define LANG_ORACLE_H

#include "front/base.h"
#include "front/check.h"

/* The largest fact count of a stage: the bits of the i32 mask. */
#define ORACLE_FACTS_MAX 32u

/* Writes the module of the protocol R->protocol to the file OUT. The table
   is made on ARENA first, so a WASM_LIMIT program writes no file. Returns 0,
   or 2 after WASM_LIMIT (a stage with more than ORACLE_FACTS_MAX facts) or
   an IO, OOM or INTERNAL diagnostic. */
int oracle_write(Arena *arena, Diag *diag, const Report *r, const char *out);

#endif
