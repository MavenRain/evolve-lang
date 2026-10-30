/* The PostgreSQL writer of `langc sql` (S6): the psql scripts of one checked
   protocol. The writer reads the Report of the check and the protocol items.
   It does not decide a lock, a scan class or a verdict (Q-S5-4 (a)).

   The files are 00-setup.sql (stage 0), then NN-WORD.sql for stage 1 to K,
   with NN the stage (2 digits, more when K > 99) and WORD the step word.
   The setup file always has the two-digit name 00-setup.sql.
   The first line of each file is `-- evolve 1; source fnv1a64 HEX; stage I
   of K: STEP` (Q-S6-2 (a)). A stage file runs in psql with the variable
   `tenant`; it skips its step when the progress ledger has the stage
   (Q-S6-18 (a)). */
#ifndef LANG_SQL_H
#define LANG_SQL_H

#include <stddef.h>

#include "front/base.h"
#include "front/check.h"
#include "json.h"

/* Adds TEXT between two quotes Q, with each Q in TEXT doubled: '"' for a
   name, '\'' for a SQL string (Q-S6-11 (a)). Returns 0 after OOM. */
int sql_put_quoted(JsonOut *out, const char *text, char q);

/* Makes NAME (a text on ARENA) the generated name evolve_PROTOCOL_NN_SUFFIX
   of stage STAGE, with NN the stage in WIDTH digits. Returns 1, or 0 after
   OOM or after SQL_NAME_LONG for a name longer than 63 bytes (Q-S6-16 (a)). */
int sql_name(Arena *arena, Diag *diag, JsonOut *name, const char *protocol, size_t stage, int width, const char *suffix);

/* Writes the files of the protocol R->protocol into the directory OUTDIR,
   which must exist (Q-S6-26 (a)). DECLS has the protocol declaration. TEXT
   is the program source (LEN bytes) for the hash. All texts are made on
   ARENA first, so a SQL_NAME_LONG program writes no file. Returns 0, 1 after
   SQL_NAME_LONG (including a table or column identifier over 63 bytes), or
   2 after an IO, OOM or INTERNAL diagnostic. */
int sql_write(Arena *arena, Diag *diag, const DeclList *decls, const Report *r, const char *text, size_t len,
              const char *outdir);

#endif
