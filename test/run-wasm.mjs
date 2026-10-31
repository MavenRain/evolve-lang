// Runs entries of a langc Wasm module and prints each result as `langc eval`
// does: a number, or `trap` when the module traps. The arguments are BigInt
// values for i64 parameters and Number values for i32 parameters. An i64
// result prints as an unsigned 64-bit decimal, an i32 result as an unsigned
// 32-bit decimal.
// Usage: node test/run-wasm.mjs FILE NAME [ARGS...]
//        node test/run-wasm.mjs FILE -   (one `NAME [ARGS...]` line on stdin per call)
import { readFileSync } from "node:fs";

const [file, name, ...args] = process.argv.slice(2);
if (file === undefined || name === undefined) {
  process.stderr.write("usage: node test/run-wasm.mjs FILE NAME [ARGS...] | FILE -\n");
  process.exit(2);
}
const instance = new WebAssembly.Instance(new WebAssembly.Module(readFileSync(file)), {});

// An i32 parameter does not take a BigInt: the call gives a TypeError before
// the entry runs. Then the call is done again with Number arguments.
const invoke = (fn, words) => {
  try {
    return fn(...words.map(BigInt));
  } catch (e) {
    if (e instanceof TypeError) return fn(...words.map(Number));
    throw e;
  }
};

const show = (r) => (typeof r === "bigint" ? BigInt.asUintN(64, r) : r >>> 0).toString();

const call = ([entry, ...words]) => {
  const fn = instance.exports[entry];
  if (typeof fn !== "function") throw new Error(`no exported entry '${entry}'`);
  try {
    return show(invoke(fn, words));
  } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return "trap";
    throw e;
  }
};

const calls = name === "-"
  ? readFileSync(0, "utf8").split("\n").filter((line) => line !== "").map((line) => line.split(" "))
  : [[name, ...args]];
process.stdout.write(calls.map((c) => call(c) + "\n").join(""));
