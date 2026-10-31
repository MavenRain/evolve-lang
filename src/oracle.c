/* The Wasm oracle writer of `langc wasm`. See src/oracle.h. */
#include <stdio.h>
#include <string.h>

#include "oracle.h"
#include "wasmbin.h"

#define ORACLE_PAGE_BYTES 65536u
/* The cell byte of a runtime verdict: one after the 8 refused codes. */
#define ORACLE_RUNTIME 9u

enum {
  SECTION_TYPE = 1,
  SECTION_FUNCTION = 3,
  SECTION_MEMORY = 5,
  SECTION_EXPORT = 7,
  SECTION_CODE = 10,
  SECTION_DATA = 11
};

enum {
  OP_UNREACHABLE = 0x00,
  OP_BLOCK = 0x02,
  OP_LOOP = 0x03,
  OP_IF = 0x04,
  OP_END = 0x0B,
  OP_BR = 0x0C,
  OP_BR_IF = 0x0D,
  OP_RETURN = 0x0F,
  OP_LOCAL_GET = 0x20,
  OP_LOCAL_SET = 0x21,
  OP_I32_LOAD8_U = 0x2D,
  OP_I32_EQZ = 0x45,
  OP_I32_GE_U = 0x4F,
  OP_I32_ADD = 0x6A,
  OP_I32_MUL = 0x6C,
  OP_I32_AND = 0x71,
  OP_I32_OR = 0x72,
  OP_I32_SHR_U = 0x76
};

#define BLOCK_EMPTY 0x40u
#define TYPE_I32 0x7Fu

/* The refused codes in the domain order of `data Code`: the cell byte of
   CODES[K] is K + 1 (Q-S7-3 (a)). */
static const char *const CODES[] = {
  "REFUSE_DROP_REQUIRED", "REFUSE_MISSING_COLUMN", "REFUSE_STALE_READ", "REFUSE_UNFILLED_READ",
  "REFUSE_NULL_INSERT",   "REFUSE_STEP_INDEX",     "REFUSE_LOCK_SCAN",  "REFUSE_RENAME_INPLACE",
};

/* The four exports in the PLAN 5.3 order, with their type index. */
static const struct {
  const char *name;
  unsigned type;
} EXPORTS[] = {
  {"stages", 0},
  {"apps", 0},
  {"verdict", 1},
  {"guard", 1},
};

/* The Report R and its table: LEN bytes, the cells, then the fact counts. */
typedef struct {
  const Report *r;
  unsigned char *table;
  size_t len;
} Oracle;

static int cell_byte(Diag *diag, const Report *r, const ReportCell *cell, unsigned char *out) {
  size_t k;
  switch (cell->verdict) {
    case REPORT_COMPATIBLE:
      *out = 0;
      return 1;
    case REPORT_RUNTIME:
      *out = ORACLE_RUNTIME;
      return 1;
    case REPORT_REFUSED:
      break;
  }
  for (k = 0; cell->code != NULL && k < sizeof CODES / sizeof *CODES; k++) {
    if (strcmp(CODES[k], cell->code) == 0) {
      *out = (unsigned char)(k + 1);
      return 1;
    }
  }
  return diag_fail(diag, "INTERNAL", r->protocol, "a refused cell of protocol %s has no oracle code", r->protocol);
}

static int part_types(WasmSink *s, const void *arg) {
  static const unsigned char types[] = {2, 0x60, 0, 1, TYPE_I32, 0x60, 2, TYPE_I32, TYPE_I32, 1, TYPE_I32};
  size_t i;
  int ok = 1;
  (void)arg;
  for (i = 0; i < sizeof types && ok; i++) ok = wasm_put_byte(s, types[i]);
  return ok;
}

static int part_functions(WasmSink *s, const void *arg) {
  size_t i;
  int ok = wasm_put_uleb(s, sizeof EXPORTS / sizeof *EXPORTS);
  (void)arg;
  for (i = 0; i < sizeof EXPORTS / sizeof *EXPORTS && ok; i++) ok = wasm_put_uleb(s, EXPORTS[i].type);
  return ok;
}

/* One memory, with the pages that the table needs (LEN >= 1: stage 0). */
static int part_memory(WasmSink *s, const void *arg) {
  const Oracle *o = arg;
  return wasm_put_uleb(s, 1) && wasm_put_byte(s, 0) && wasm_put_uleb(s, (o->len + ORACLE_PAGE_BYTES - 1) / ORACLE_PAGE_BYTES);
}

static int part_exports(WasmSink *s, const void *arg) {
  size_t i;
  int ok = wasm_put_uleb(s, sizeof EXPORTS / sizeof *EXPORTS);
  (void)arg;
  for (i = 0; i < sizeof EXPORTS / sizeof *EXPORTS && ok; i++)
    ok = wasm_put_name(s, EXPORTS[i].name) && wasm_put_byte(s, 0) && wasm_put_uleb(s, i);
  return ok;
}

/* Traps unless local L is below BOUND (unsigned). */
static int below_or_trap(WasmSink *s, unsigned l, size_t bound) {
  return wasm_op2(s, OP_LOCAL_GET, l) && wasm_i32_const(s, bound) && wasm_put_byte(s, OP_I32_GE_U) &&
         wasm_put_byte(s, OP_IF) && wasm_put_byte(s, BLOCK_EMPTY) && wasm_put_byte(s, OP_UNREACHABLE) &&
         wasm_put_byte(s, OP_END);
}

static int body_stages(WasmSink *s, const void *arg) {
  const Oracle *o = arg;
  return wasm_put_uleb(s, 0) && wasm_i32_const(s, o->r->stage_count) && wasm_put_byte(s, OP_END);
}

static int body_apps(WasmSink *s, const void *arg) {
  const Oracle *o = arg;
  return wasm_put_uleb(s, 0) && wasm_i32_const(s, o->r->app_count) && wasm_put_byte(s, OP_END);
}

/* verdict(app, stage): the byte at app * STAGE_COUNT + stage. */
static int body_verdict(WasmSink *s, const void *arg) {
  const Oracle *o = arg;
  return wasm_put_uleb(s, 0) && below_or_trap(s, 0, o->r->app_count) && below_or_trap(s, 1, o->r->stage_count) &&
         wasm_op2(s, OP_LOCAL_GET, 0) && wasm_i32_const(s, o->r->stage_count) && wasm_put_byte(s, OP_I32_MUL) &&
         wasm_op2(s, OP_LOCAL_GET, 1) && wasm_put_byte(s, OP_I32_ADD) && wasm_op2(s, OP_I32_LOAD8_U, 0) &&
         wasm_put_uleb(s, 0) && wasm_put_byte(s, OP_END);
}

/* guard(stage, facts): local 2 is the fact count N, local 3 is K. While
   K < N: bit K of FACTS 0 gives K + 1. After the loop: 0. */
static int body_guard(WasmSink *s, const void *arg) {
  const Oracle *o = arg;
  size_t cells = o->r->app_count * o->r->stage_count;
  return wasm_put_uleb(s, 1) && wasm_put_uleb(s, 2) && wasm_put_byte(s, TYPE_I32) &&
         below_or_trap(s, 0, o->r->stage_count) && wasm_op2(s, OP_LOCAL_GET, 0) && wasm_op2(s, OP_I32_LOAD8_U, 0) &&
         wasm_put_uleb(s, cells) && wasm_op2(s, OP_LOCAL_SET, 2) && wasm_put_byte(s, OP_BLOCK) &&
         wasm_put_byte(s, BLOCK_EMPTY) && wasm_put_byte(s, OP_LOOP) && wasm_put_byte(s, BLOCK_EMPTY) &&
         wasm_op2(s, OP_LOCAL_GET, 3) && wasm_op2(s, OP_LOCAL_GET, 2) && wasm_put_byte(s, OP_I32_GE_U) &&
         wasm_op2(s, OP_BR_IF, 1) && wasm_op2(s, OP_LOCAL_GET, 1) && wasm_op2(s, OP_LOCAL_GET, 3) &&
         wasm_put_byte(s, OP_I32_SHR_U) && wasm_i32_const(s, 1) && wasm_put_byte(s, OP_I32_AND) &&
         wasm_put_byte(s, OP_I32_EQZ) && wasm_put_byte(s, OP_IF) && wasm_put_byte(s, BLOCK_EMPTY) &&
         wasm_op2(s, OP_LOCAL_GET, 3) && wasm_i32_const(s, 1) && wasm_put_byte(s, OP_I32_ADD) &&
         wasm_put_byte(s, OP_RETURN) && wasm_put_byte(s, OP_END) && wasm_op2(s, OP_LOCAL_GET, 3) &&
         wasm_i32_const(s, 1) && wasm_put_byte(s, OP_I32_ADD) && wasm_op2(s, OP_LOCAL_SET, 3) &&
         wasm_op2(s, OP_BR, 0) && wasm_put_byte(s, OP_END) && wasm_put_byte(s, OP_END) && wasm_i32_const(s, 0) &&
         wasm_put_byte(s, OP_END);
}

static int part_code(WasmSink *s, const void *arg) {
  return wasm_put_uleb(s, sizeof EXPORTS / sizeof *EXPORTS) && wasm_sized(s, body_stages, arg) &&
         wasm_sized(s, body_apps, arg) && wasm_sized(s, body_verdict, arg) && wasm_sized(s, body_guard, arg);
}

/* One active segment of memory 0 at offset 0. */
static int part_data(WasmSink *s, const void *arg) {
  const Oracle *o = arg;
  size_t i;
  int ok = wasm_put_uleb(s, 1) && wasm_put_uleb(s, 0) && wasm_i32_const(s, 0) && wasm_put_byte(s, OP_END) &&
           wasm_put_uleb(s, o->len);
  for (i = 0; i < o->len && ok; i++) ok = wasm_put_byte(s, o->table[i]);
  return ok;
}

int oracle_write(Arena *arena, Diag *diag, const Report *r, const char *out) {
  static const unsigned char header[8] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};
  /* The cells are on the arena (1 GiB), so CELLS and LEN fit an i32. */
  size_t cells = r->app_count * r->stage_count;
  Oracle o = {r, NULL, cells + r->stage_count};
  size_t i;
  int ok = 1;
  for (i = 0; i < r->stage_count; i++) {
    if (r->stages[i].guard_count > ORACLE_FACTS_MAX) {
      return diag_fail(diag, "WASM_LIMIT", r->protocol,
                       "stage %zu of protocol %s has %zu guard facts; the guard mask holds a maximum of %u facts", i,
                       r->protocol, r->stages[i].guard_count, ORACLE_FACTS_MAX) +
             2;
    }
  }
  o.table = arena_alloc(arena, o.len);
  if (o.table == NULL) return diag_fail(diag, "OOM", NULL, "out of memory") + 2;
  for (i = 0; i < cells && ok; i++) ok = cell_byte(diag, r, &r->cells[i], &o.table[i]);
  if (!ok) return 2;
  for (i = 0; i < r->stage_count; i++) o.table[cells + i] = (unsigned char)r->stages[i].guard_count;
  FILE *file = fopen(out, "wb");
  if (file == NULL) return diag_fail(diag, "IO", NULL, "cannot write '%s'", out) + 2;
  WasmSink s = {file, 0, stderr};
  for (i = 0; i < sizeof header && ok; i++) ok = wasm_put_byte(&s, header[i]);
  ok = ok && wasm_section(&s, SECTION_TYPE, part_types, &o) && wasm_section(&s, SECTION_FUNCTION, part_functions, &o) &&
       wasm_section(&s, SECTION_MEMORY, part_memory, &o) && wasm_section(&s, SECTION_EXPORT, part_exports, &o) &&
       wasm_section(&s, SECTION_CODE, part_code, &o) && wasm_section(&s, SECTION_DATA, part_data, &o);
  int closed = fclose(file) == 0;
  return ok && closed ? 0 : diag_fail(diag, "IO", NULL, "cannot write '%s'", out) + 2;
}
