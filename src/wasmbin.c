/* The shared Wasm 1.0 byte writer. See src/wasmbin.h. */
#include <string.h>

#include "wasmbin.h"

enum {
  OP_I32_CONST = 0x41,
  OP_I64_CONST = 0x42
};

int wasm_put_byte(WasmSink *s, unsigned v) {
  s->count++;
  return s->out == NULL || fputc((int)(v & 0xFFu), s->out) != EOF;
}

int wasm_put_uleb(WasmSink *s, uint64_t v) {
  int ok = 1;
  for (; ok && v >= 0x80u; v >>= 7) ok = wasm_put_byte(s, (unsigned)(v & 0x7Fu) | 0x80u);
  return ok && wasm_put_byte(s, (unsigned)v);
}

int wasm_put_sleb(WasmSink *s, uint64_t v) {
  uint64_t fill = (v >> 63) != 0 ? ~(~(uint64_t)0 >> 7) : 0;
  unsigned low = (unsigned)(v & 0x7Fu);
  uint64_t rest = (v >> 7) | fill;
  int last = (rest == 0 && (low & 0x40u) == 0) || (rest == ~(uint64_t)0 && (low & 0x40u) != 0);
  int ok = wasm_put_byte(s, last ? low : low | 0x80u);
  return ok && (last || wasm_put_sleb(s, rest));
}

int wasm_put_name(WasmSink *s, const char *name) {
  size_t i;
  size_t len = strlen(name);
  int ok = wasm_put_uleb(s, len);
  for (i = 0; i < len && ok; i++) ok = wasm_put_byte(s, (unsigned char)name[i]);
  return ok;
}

int wasm_sized(WasmSink *s, WasmPart part, const void *arg) {
  WasmSink count = {NULL, 0, s->err};
  return part(&count, arg) && wasm_put_uleb(s, count.count) && part(s, arg);
}

int wasm_section(WasmSink *s, unsigned id, WasmPart part, const void *arg) {
  return wasm_put_byte(s, id) && wasm_sized(s, part, arg);
}

int wasm_op2(WasmSink *s, unsigned op, uint64_t imm) {
  return wasm_put_byte(s, op) && wasm_put_uleb(s, imm);
}

int wasm_i64_const(WasmSink *s, uint64_t v) {
  return wasm_put_byte(s, OP_I64_CONST) && wasm_put_sleb(s, v);
}

int wasm_i32_const(WasmSink *s, uint64_t v) {
  return wasm_put_byte(s, OP_I32_CONST) && wasm_put_sleb(s, v);
}
