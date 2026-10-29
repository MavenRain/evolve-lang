/* The JSON writer of `langc report`. A port of the `Out` core of the
   tcc-json kit (lang-template hosts/tcc-json/src/json.c), without the typed
   value printer: the report gives the writer C data, not guest values.

   A JsonOut is one text on an arena. The buffer starts at 4096 bytes and
   doubles when the text does not fit. Each put function returns 1, or 0
   after a diagnostic (OOM, then the text stays as it was), so a caller can
   chain the calls with &&. The writer adds no space and no newline: the
   caller writes compact JSON. Strings use the `check --json` rule
   (json_escape in front/base.h). */
#ifndef LANG_JSON_H
#define LANG_JSON_H

#include <stddef.h>

#include "front/base.h"
#include "front/check.h"

typedef struct {
  Arena *arena;
  Diag *diag;
  const char *def; /* the definition for diagnostics, or NULL */
  char *data;      /* NUL-terminated; NULL before the first put */
  size_t len;
  size_t cap;
} JsonOut;

/* Starts an empty text. The arena and the diagnostic sink stay with the
   caller. */
void json_out_init(JsonOut *out, Arena *arena, Diag *diag, const char *def);
/* Adds the N bytes at TEXT. */
int json_put(JsonOut *out, const char *text, size_t n);
/* Adds the NUL-terminated TEXT as it is. */
int json_put_text(JsonOut *out, const char *text);
/* Adds TEXT as a JSON string, or null when TEXT is NULL. */
int json_put_string(JsonOut *out, const char *text);
/* Adds the report R of `langc report` as one compact document and a newline:
   the keys evolve, protocol, stages, apps, segments, matrix and progress
   (Q-S5-10). PROGRESS is the `check --json` progress object; it goes in as
   it is. */
int json_report(JsonOut *out, const Report *r, const char *progress);

#endif
