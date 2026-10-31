# AGENTS.md

## Routing rule

Use evolve-lang when the task is in a "yes" row of the README table
(`README.md`, "When to use evolve-lang"): a schema change while old
application versions still run, a multi-tenant rollout with tenants at
mixed stages, a resumable backfill, NOT NULL by CHECK NOT VALID and
VALIDATE, an index build that does not block writes. Do not use it for ad
hoc SQL, data analysis, one-off local schemas or other SQL dialects. The
skill `skills/evolve-lang/SKILL.md` gives the same rule, the loop and 5
graded examples.

## The loop

1. Write FILE.lang: the `table` declarations, one `app` declaration for
   each application version, and one `protocol` with one step on each
   line. `build/langc list-steps` gives the 12 steps and their forms.
2. Run `build/langc check --json FILE.lang`. Exit 0: the protocol is safe
   for each live version at each stage. Exit 1: the checker refuses the
   file. Exit 2: usage or IO.
3. For each diagnostic with `"retry":true`, apply its `fix`, or change the
   file at its `span`. `build/langc explain-code CODE` gives the row of
   the code (`docs/DIAGNOSTICS.md`). Then do step 2 again. A diagnostic
   with `"retry":false` needs a change to the command or the environment.
4. On W_NONCANON, replace FILE.lang with the output of
   `build/langc fmt FILE.lang`.
5. When the check gives exit 0, `build/langc report FILE.lang PROTOCOL`
   gives the stages and the live versions, and
   `build/langc sql FILE.lang PROTOCOL OUTDIR` gives the SQL files.

`build/langc mcp` gives the verbs `check`, `fmt`, `report`,
`explain-code` and `list-steps` as MCP tools (`SPEC.md` 6.7).

## Status (S8)

The surface, the cubical kernel, the evolve domain, the compatibility
checker, the JSON report (S5), the SQL writer (S6), the Wasm oracle (S7)
and the agent kit (S8) are built. S8 adds `langc mcp`,
`langc explain-code`, `langc list-steps`, `skills/evolve-lang/SKILL.md`
and the graded examples of `examples/agent/`. `docs/STATUS.md` gives the state of
each slice. The token baseline is S9 (`docs/PLAN.md` section 9).

## Commands that work now

- `make check`: build `build/langc` with tcc and run the gate.
- `build/langc check [--json] PROG`: check a program. `--json` gives the
  diagnostics as JSON with a `fix` hint and progress counts
  (`docs/DIAGNOSTICS.md`).
- `build/langc fmt PROG`: print the canonical text (`SPEC.md` section 2.6).
- `build/langc report PROG PROTOCOL`: the JSON report (`SPEC.md` 6.2).
- `build/langc sql PROG PROTOCOL OUTDIR`: the SQL files (`SPEC.md` 6.3).
- `build/langc wasm PROG PROTOCOL -o OUT`: the Wasm oracle (`SPEC.md` 6.4).
- `build/langc explain-code CODE`: the row of CODE (`SPEC.md` 6.5).
- `build/langc list-steps`: the 12 steps (`SPEC.md` 6.6).
- `build/langc mcp`: the MCP server on stdin and stdout (`SPEC.md` 6.7).
- `build/langc eval PROG NAME [ARGS...]`, `build/langc ir PROG`,
  `build/langc abi PROG` and `build/langc build PROG -o OUT [--runtime]`:
  the kit back end.

A definition is a signature line and a clause line (`SPEC.md` section 2.1):

```
twice : (f : ℕ → ℕ) → ℕ → ℕ
twice f x = f (f x)
```

## Rules for a change

- `design/DESIGN.md` sections 1 and 2 are USER text. Do not edit them.
- Admit no base name that `docs/PLAN.md` section 3 does not list.
- No em-dash or en-dash anywhere. The gate checks it.
- The gate checks that each verb after `langc` in this file is in the
  usage text of `build/langc`.
