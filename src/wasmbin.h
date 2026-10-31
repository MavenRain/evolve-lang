/* The shared Wasm 1.0 (MVP) byte writer. Each file that writes a Wasm module
   gets the byte encodings from here. This file has no IR facts.

   Each sized part (a section, a function body) is written in two passes: the
   first pass counts the bytes, the second pass writes them after the size.
   So a writer needs no buffer. Each function returns 1, or 0 when a write
   fails. */
#ifndef LANG_WASMBIN_H
#define LANG_WASMBIN_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* A byte sink. With out == NULL it only counts. ERR gets the messages of the
   writer that uses the sink. */
typedef struct {
  FILE *out;
  size_t count;
  FILE *err;
} WasmSink;

typedef int (*WasmPart)(WasmSink *s, const void *arg);

int wasm_put_byte(WasmSink *s, unsigned v);
int wasm_put_uleb(WasmSink *s, uint64_t v);

/* Signed LEB128 of the two's complement value of V. */
int wasm_put_sleb(WasmSink *s, uint64_t v);

/* The byte length of NAME, then its bytes. */
int wasm_put_name(WasmSink *s, const char *name);

/* Writes the byte size of PART, then PART. */
int wasm_sized(WasmSink *s, WasmPart part, const void *arg);

/* The section id ID, then the sized PART. */
int wasm_section(WasmSink *s, unsigned id, WasmPart part, const void *arg);

/* The opcode OP, then the unsigned LEB128 immediate IMM. */
int wasm_op2(WasmSink *s, unsigned op, uint64_t imm);

int wasm_i64_const(WasmSink *s, uint64_t v);
int wasm_i32_const(WasmSink *s, uint64_t v);

#endif
