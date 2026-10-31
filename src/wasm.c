/* The Wasm back end. It writes a Wasm 1.0 (MVP) binary module directly.

   Module layout:
   - Function 0 is the internal allocator `alloc (bytes : i32) -> i64`. It
     bumps the mutable i64 global 0 (the heap pointer) over memory 0. When a
     block does not fit, it grows the memory. When the memory cannot grow (the
     maximum is WASM_MAX_PAGES), it traps.
   - Function 1 + K is IR entry K, exported as its name. Each parameter and the
     result is an i64. Each entry first resets the heap pointer to 0, and it
     traps on a Bool argument larger than 1.
   - A word is 8 bytes. Word K of the block at address A is at A + 8K.

   Each entry has three i64 scratch locals after the IR locals: S and S + 1
   hold the operands of SUB, ADD_CARRY and MUL_CARRY, and S + 2 holds the
   address of an ALLOC. An operator reads its operands into S and S + 1 only
   after both operand expressions are done, so a nested operator cannot
   overwrite them.

   Each sized part (a section, a function body) is written in two passes: the
   first pass counts the bytes, the second pass writes them after the size.
   So the back end needs no buffer. The byte helpers are in src/wasmbin.h. */
#include <stdint.h>

#include "target.h"
#include "wasmbin.h"

#define WASM_MAX_PAGES 16384u /* 1 GiB, the arena limit of langc eval */
#define WASM_DEPTH_MAX 2000u

enum {
  OP_UNREACHABLE = 0x00,
  OP_BLOCK = 0x02,
  OP_LOOP = 0x03,
  OP_IF = 0x04,
  OP_ELSE = 0x05,
  OP_END = 0x0B,
  OP_BR = 0x0C,
  OP_BR_IF = 0x0D,
  OP_BR_TABLE = 0x0E,
  OP_RETURN = 0x0F,
  OP_CALL = 0x10,
  OP_SELECT = 0x1B,
  OP_LOCAL_GET = 0x20,
  OP_LOCAL_SET = 0x21,
  OP_LOCAL_TEE = 0x22,
  OP_GLOBAL_GET = 0x23,
  OP_GLOBAL_SET = 0x24,
  OP_I64_LOAD = 0x29,
  OP_I64_STORE = 0x37,
  OP_MEMORY_SIZE = 0x3F,
  OP_MEMORY_GROW = 0x40,
  OP_I32_EQ = 0x46,
  OP_I64_EQZ = 0x50,
  OP_I64_EQ = 0x51,
  OP_I64_LT_U = 0x54,
  OP_I64_GT_U = 0x56,
  OP_I64_LE_U = 0x58,
  OP_I64_GE_U = 0x5A,
  OP_I64_ADD = 0x7C,
  OP_I64_SUB = 0x7D,
  OP_I64_MUL = 0x7E,
  OP_I64_DIV_U = 0x80,
  OP_I64_OR = 0x84,
  OP_I64_SHL = 0x86,
  OP_I64_SHR_U = 0x88,
  OP_I32_WRAP_I64 = 0xA7,
  OP_I64_EXTEND_I32_U = 0xAD,
  TYPE_I32 = 0x7F,
  TYPE_I64 = 0x7E,
  TYPE_FUNC = 0x60,
  TYPE_EMPTY = 0x40,
  ALIGN_WORD = 3
};

enum {
  SECTION_TYPE = 1,
  SECTION_FUNCTION = 3,
  SECTION_MEMORY = 5,
  SECTION_GLOBAL = 6,
  SECTION_EXPORT = 7,
  SECTION_CODE = 10
};

const char target_name[] = "wasm";
static int memarg(WasmSink *s, unsigned op, uint32_t word) {
  return wasm_put_byte(s, op) && wasm_put_uleb(s, ALIGN_WORD) && wasm_put_uleb(s, 8u * (uint64_t)word);
}

static int deep(WasmSink *s) {
  fprintf(s->err, "langc: WASM_DEPTH: -: the IR nests deeper than %u levels\n", WASM_DEPTH_MAX);
  return 0;
}

static int emit_expr(WasmSink *s, const IrExpr *e, uint32_t t, unsigned depth);

/* The operands are on the stack. Moves them to the scratch locals T, T + 1. */
static int operands(WasmSink *s, uint32_t t) {
  return wasm_op2(s, OP_LOCAL_SET, t + 1u) && wasm_op2(s, OP_LOCAL_SET, t);
}

static int emit_op(WasmSink *s, IrOp op, uint32_t t) {
  switch (op) {
    case IR_OP_ADD:
      return wasm_put_byte(s, OP_I64_ADD);
    case IR_OP_SUB: /* select(a - b, 0, a >= b) */
      return operands(s, t) && wasm_op2(s, OP_LOCAL_GET, t) && wasm_op2(s, OP_LOCAL_GET, t + 1u) &&
             wasm_put_byte(s, OP_I64_SUB) && wasm_i64_const(s, 0) && wasm_op2(s, OP_LOCAL_GET, t) &&
             wasm_op2(s, OP_LOCAL_GET, t + 1u) && wasm_put_byte(s, OP_I64_GE_U) && wasm_put_byte(s, OP_SELECT);
    case IR_OP_MUL:
      return wasm_put_byte(s, OP_I64_MUL);
    case IR_OP_EQ:
      return wasm_put_byte(s, OP_I64_EQ) && wasm_put_byte(s, OP_I64_EXTEND_I32_U);
    case IR_OP_LE:
      return wasm_put_byte(s, OP_I64_LE_U) && wasm_put_byte(s, OP_I64_EXTEND_I32_U);
    case IR_OP_ADD_CARRY: /* a + b < a, modulo 2^64 */
      return operands(s, t) && wasm_op2(s, OP_LOCAL_GET, t) && wasm_op2(s, OP_LOCAL_GET, t + 1u) &&
             wasm_put_byte(s, OP_I64_ADD) && wasm_op2(s, OP_LOCAL_GET, t) && wasm_put_byte(s, OP_I64_LT_U) &&
             wasm_put_byte(s, OP_I64_EXTEND_I32_U);
    case IR_OP_MUL_CARRY:
      /* Exact: a * b > 2^64 - 1 if and only if b > floor((2^64 - 1) / a), for
         a > 0. For a = 0 the divisor is 1, and no b is larger than 2^64 - 1. */
      return operands(s, t) && wasm_op2(s, OP_LOCAL_GET, t + 1u) && wasm_i64_const(s, ~(uint64_t)0) &&
             wasm_i64_const(s, 1) && wasm_op2(s, OP_LOCAL_GET, t) && wasm_op2(s, OP_LOCAL_GET, t) &&
             wasm_put_byte(s, OP_I64_EQZ) && wasm_put_byte(s, OP_SELECT) && wasm_put_byte(s, OP_I64_DIV_U) &&
             wasm_put_byte(s, OP_I64_GT_U) && wasm_put_byte(s, OP_I64_EXTEND_I32_U);
    case IR_OP_OR:
      return wasm_put_byte(s, OP_I64_OR);
  }
  return 0;
}

static int emit_expr(WasmSink *s, const IrExpr *e, uint32_t t, unsigned depth) {
  if (depth > WASM_DEPTH_MAX) return deep(s);
  switch (e->kind) {
    case IR_EXPR_CONST:
      return wasm_i64_const(s, e->value);
    case IR_EXPR_LOCAL:
      return wasm_op2(s, OP_LOCAL_GET, e->local);
    case IR_EXPR_BINARY:
      return emit_expr(s, e->left, t, depth + 1u) && emit_expr(s, e->right, t, depth + 1u) && emit_op(s, e->op, t);
    case IR_EXPR_LOAD:
      return emit_expr(s, e->left, t, depth + 1u) && wasm_put_byte(s, OP_I32_WRAP_I64) &&
             memarg(s, OP_I64_LOAD, e->field);
  }
  return 0;
}

static int emit_block(WasmSink *s, const IrBlock *b, uint32_t t, unsigned depth);

/* local := alloc(8 * n); word k := fields[k]. The address waits in T + 2. */
static int emit_alloc(WasmSink *s, const IrStmt *st, uint32_t t, unsigned depth) {
  size_t k;
  int ok = wasm_i32_const(s, 8u * (uint64_t)st->field_count) && wasm_op2(s, OP_CALL, 0) &&
           wasm_op2(s, OP_LOCAL_SET, t + 2u);
  for (k = 0; k < st->field_count && ok; k++) {
    ok = wasm_op2(s, OP_LOCAL_GET, t + 2u) && wasm_put_byte(s, OP_I32_WRAP_I64) &&
         emit_expr(s, st->fields[k], t, depth + 1u) && memarg(s, OP_I64_STORE, (uint32_t)k);
  }
  return ok && wasm_op2(s, OP_LOCAL_GET, t + 2u) && wasm_op2(s, OP_LOCAL_SET, st->local);
}

/* Blocks: exit, trap, then arm n - 1 down to arm 0 (the innermost). br_table
   label k ends the block of arm k, so the code of arm k follows. A value of
   n or more takes label n, the trap block. After arm k, label n - k is the
   exit block. */
static int emit_switch(WasmSink *s, const IrStmt *st, uint32_t t, unsigned depth) {
  size_t k;
  size_t n = st->arm_count;
  int ok = wasm_put_byte(s, OP_BLOCK) && wasm_put_byte(s, TYPE_EMPTY) && wasm_put_byte(s, OP_BLOCK) &&
           wasm_put_byte(s, TYPE_EMPTY);
  for (k = 0; k < n && ok; k++) ok = wasm_put_byte(s, OP_BLOCK) && wasm_put_byte(s, TYPE_EMPTY);
  ok = ok && emit_expr(s, st->expr, t, depth + 1u) && wasm_op2(s, OP_LOCAL_TEE, t) &&
       wasm_put_byte(s, OP_I32_WRAP_I64) && wasm_i32_const(s, n) && wasm_op2(s, OP_LOCAL_GET, t) &&
       wasm_i64_const(s, n) && wasm_put_byte(s, OP_I64_LT_U) && wasm_put_byte(s, OP_SELECT) &&
       wasm_op2(s, OP_BR_TABLE, n);
  for (k = 0; k <= n && ok; k++) ok = wasm_put_uleb(s, k);
  for (k = 0; k < n && ok; k++) {
    ok = wasm_put_byte(s, OP_END) && emit_block(s, &st->arms[k], t, depth + 1u) && wasm_op2(s, OP_BR, n - k);
  }
  return ok && wasm_put_byte(s, OP_END) && wasm_put_byte(s, OP_UNREACHABLE) && wasm_put_byte(s, OP_END);
}

/* counter := expr; then, at the top of each run, leave when counter = 0 or
   (for WHILE) local = 0. counter decreases by 1 after each run. */
static int emit_loop(WasmSink *s, const IrStmt *st, int is_while, uint32_t t, unsigned depth) {
  return emit_expr(s, st->expr, t, depth + 1u) && wasm_op2(s, OP_LOCAL_SET, st->counter) &&
         wasm_put_byte(s, OP_BLOCK) && wasm_put_byte(s, TYPE_EMPTY) && wasm_put_byte(s, OP_LOOP) &&
         wasm_put_byte(s, TYPE_EMPTY) && wasm_op2(s, OP_LOCAL_GET, st->counter) && wasm_put_byte(s, OP_I64_EQZ) &&
         wasm_op2(s, OP_BR_IF, 1) &&
         (!is_while ||
          (wasm_op2(s, OP_LOCAL_GET, st->local) && wasm_put_byte(s, OP_I64_EQZ) && wasm_op2(s, OP_BR_IF, 1))) &&
         emit_block(s, &st->body, t, depth + 1u) && wasm_op2(s, OP_LOCAL_GET, st->counter) && wasm_i64_const(s, 1) &&
         wasm_put_byte(s, OP_I64_SUB) && wasm_op2(s, OP_LOCAL_SET, st->counter) && wasm_op2(s, OP_BR, 0) &&
         wasm_put_byte(s, OP_END) && wasm_put_byte(s, OP_END);
}

static int emit_stmt(WasmSink *s, const IrStmt *st, uint32_t t, unsigned depth) {
  if (depth > WASM_DEPTH_MAX) return deep(s);
  switch (st->kind) {
    case IR_STMT_SET:
      return emit_expr(s, st->expr, t, depth + 1u) && wasm_op2(s, OP_LOCAL_SET, st->local);
    case IR_STMT_ALLOC:
      return emit_alloc(s, st, t, depth);
    case IR_STMT_IF: /* expr = 0: otherwise, else: body */
      return emit_expr(s, st->expr, t, depth + 1u) && wasm_put_byte(s, OP_I64_EQZ) && wasm_put_byte(s, OP_IF) &&
             wasm_put_byte(s, TYPE_EMPTY) && emit_block(s, &st->otherwise, t, depth + 1u) &&
             wasm_put_byte(s, OP_ELSE) && emit_block(s, &st->body, t, depth + 1u) && wasm_put_byte(s, OP_END);
    case IR_STMT_SWITCH:
      return emit_switch(s, st, t, depth);
    case IR_STMT_REPEAT:
      return emit_loop(s, st, 0, t, depth);
    case IR_STMT_WHILE:
      return emit_loop(s, st, 1, t, depth);
    case IR_STMT_TRAP:
      return wasm_put_byte(s, OP_UNREACHABLE);
    case IR_STMT_RETURN:
      return emit_expr(s, st->expr, t, depth + 1u) && wasm_put_byte(s, OP_RETURN);
  }
  return 0;
}

static int emit_block(WasmSink *s, const IrBlock *b, uint32_t t, unsigned depth) {
  size_t i;
  int ok = depth <= WASM_DEPTH_MAX || deep(s);
  for (i = 0; i < b->count && ok; i++) ok = emit_stmt(s, b->items[i], t, depth + 1u);
  return ok;
}

/* alloc (bytes : i32) -> i64, local 1 = the new end:
   old := heap; end := old + bytes; if end > memory bytes { grow by
   ceil(end / 65536) - pages; trap on -1 }; heap := end; return old. */
static int part_alloc(WasmSink *s, const void *arg) {
  (void)arg;
  return wasm_put_uleb(s, 1) && wasm_put_uleb(s, 1) && wasm_put_byte(s, TYPE_I64) && wasm_op2(s, OP_GLOBAL_GET, 0) &&
         wasm_op2(s, OP_GLOBAL_GET, 0) && wasm_op2(s, OP_LOCAL_GET, 0) && wasm_put_byte(s, OP_I64_EXTEND_I32_U) &&
         wasm_put_byte(s, OP_I64_ADD) && wasm_op2(s, OP_LOCAL_TEE, 1) && wasm_op2(s, OP_MEMORY_SIZE, 0) &&
         wasm_put_byte(s, OP_I64_EXTEND_I32_U) && wasm_i64_const(s, 16) && wasm_put_byte(s, OP_I64_SHL) &&
         wasm_put_byte(s, OP_I64_GT_U) && wasm_put_byte(s, OP_IF) && wasm_put_byte(s, TYPE_EMPTY) &&
         wasm_op2(s, OP_LOCAL_GET, 1) && wasm_i64_const(s, 65535) && wasm_put_byte(s, OP_I64_ADD) &&
         wasm_i64_const(s, 16) && wasm_put_byte(s, OP_I64_SHR_U) && wasm_op2(s, OP_MEMORY_SIZE, 0) &&
         wasm_put_byte(s, OP_I64_EXTEND_I32_U) && wasm_put_byte(s, OP_I64_SUB) && wasm_put_byte(s, OP_I32_WRAP_I64) &&
         wasm_op2(s, OP_MEMORY_GROW, 0) && wasm_i32_const(s, ~(uint64_t)0) && wasm_put_byte(s, OP_I32_EQ) &&
         wasm_put_byte(s, OP_IF) && wasm_put_byte(s, TYPE_EMPTY) && wasm_put_byte(s, OP_UNREACHABLE) &&
         wasm_put_byte(s, OP_END) && wasm_put_byte(s, OP_END) && wasm_op2(s, OP_LOCAL_GET, 1) &&
         wasm_op2(s, OP_GLOBAL_SET, 0) && wasm_put_byte(s, OP_END);
}

/* Traps when the Bool parameter K is larger than 1. */
static int bool_guard(WasmSink *s, const IrFunc *fn, size_t k) {
  return fn->params[k] == IR_SCALAR_NAT ||
         (wasm_op2(s, OP_LOCAL_GET, k) && wasm_i64_const(s, 1) && wasm_put_byte(s, OP_I64_GT_U) &&
          wasm_put_byte(s, OP_IF) && wasm_put_byte(s, TYPE_EMPTY) && wasm_put_byte(s, OP_UNREACHABLE) &&
          wasm_put_byte(s, OP_END));
}

static int part_entry(WasmSink *s, const void *arg) {
  const IrFunc *fn = arg;
  uint32_t t = fn->local_count;
  size_t k;
  int ok = wasm_put_uleb(s, 1) && wasm_put_uleb(s, (uint64_t)(fn->local_count - fn->param_count) + 3u) &&
           wasm_put_byte(s, TYPE_I64) && wasm_i64_const(s, 0) && wasm_op2(s, OP_GLOBAL_SET, 0);
  for (k = 0; k < fn->param_count && ok; k++) ok = bool_guard(s, fn, k);
  return ok && emit_block(s, &fn->body, t, 0) && wasm_put_byte(s, OP_UNREACHABLE) && wasm_put_byte(s, OP_END);
}

static int part_types(WasmSink *s, const void *arg) {
  const IrProgram *prog = arg;
  size_t i;
  size_t k;
  int ok = wasm_put_uleb(s, 1u + prog->func_count) && wasm_put_byte(s, TYPE_FUNC) && wasm_put_uleb(s, 1) &&
           wasm_put_byte(s, TYPE_I32) && wasm_put_uleb(s, 1) && wasm_put_byte(s, TYPE_I64);
  for (i = 0; i < prog->func_count && ok; i++) {
    ok = wasm_put_byte(s, TYPE_FUNC) && wasm_put_uleb(s, prog->funcs[i].param_count);
    for (k = 0; k < prog->funcs[i].param_count && ok; k++) ok = wasm_put_byte(s, TYPE_I64);
    ok = ok && wasm_put_uleb(s, 1) && wasm_put_byte(s, TYPE_I64);
  }
  return ok;
}

static int part_functions(WasmSink *s, const void *arg) {
  const IrProgram *prog = arg;
  size_t i;
  int ok = wasm_put_uleb(s, 1u + prog->func_count);
  for (i = 0; i <= prog->func_count && ok; i++) ok = wasm_put_uleb(s, i);
  return ok;
}

static int part_memory(WasmSink *s, const void *arg) {
  (void)arg;
  return wasm_put_uleb(s, 1) && wasm_put_byte(s, 1) && wasm_put_uleb(s, 1) && wasm_put_uleb(s, WASM_MAX_PAGES);
}

static int part_global(WasmSink *s, const void *arg) {
  (void)arg;
  return wasm_put_uleb(s, 1) && wasm_put_byte(s, TYPE_I64) && wasm_put_byte(s, 1) && wasm_i64_const(s, 0) &&
         wasm_put_byte(s, OP_END);
}

static int part_exports(WasmSink *s, const void *arg) {
  const IrProgram *prog = arg;
  size_t i;
  int ok = wasm_put_uleb(s, prog->func_count);
  for (i = 0; i < prog->func_count && ok; i++) {
    ok = wasm_put_name(s, prog->funcs[i].name) && wasm_put_byte(s, 0) && wasm_put_uleb(s, 1u + i);
  }
  return ok;
}

static int part_code(WasmSink *s, const void *arg) {
  const IrProgram *prog = arg;
  size_t i;
  int ok = wasm_put_uleb(s, 1u + prog->func_count) && wasm_sized(s, part_alloc, NULL);
  for (i = 0; i < prog->func_count && ok; i++) ok = wasm_sized(s, part_entry, &prog->funcs[i]);
  return ok;
}

int target_abi_line(const IrFunc *fn, FILE *out, FILE *err) {
  (void)err;
  fprintf(out, "%s\n", fn->name);
  return 1;
}

/* Both parts are the module bytes. */
int target_write(const IrProgram *prog, TargetPart part, FILE *out, FILE *err) {
  static const unsigned char header[8] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};
  WasmSink s = {out, 0, err};
  size_t i;
  int ok = 1;
  (void)part;
  for (i = 0; i < sizeof header && ok; i++) ok = wasm_put_byte(&s, header[i]);
  ok = ok && wasm_section(&s, SECTION_TYPE, part_types, prog) &&
       wasm_section(&s, SECTION_FUNCTION, part_functions, prog) &&
       wasm_section(&s, SECTION_MEMORY, part_memory, NULL) && wasm_section(&s, SECTION_GLOBAL, part_global, NULL) &&
       wasm_section(&s, SECTION_EXPORT, part_exports, prog) && wasm_section(&s, SECTION_CODE, part_code, prog);
  if (!ok && ferror(out)) fputs("langc: IO: -: cannot write the wasm module\n", err);
  return ok;
}
