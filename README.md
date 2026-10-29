# evolve-lang

evolve-lang is a language for PostgreSQL schema migrations as checked
transitions. A program declares schema states, the read and write
requirements of each application version, and a migration protocol: a chain
of steps. The checker computes which application versions stay compatible at
each intermediate stage. It refuses a protocol that leaves a live
application version without a column that the version needs. The host is
TinyCC. The JSON compatibility report is built (S5). The planned targets
are PostgreSQL SQL stage files and a Wasm compatibility oracle module.

## Status (S5)

S1 gives the evolve-lang surface on the tcc-wasm kit of lang-template: the
Agda-like syntax with ASCII aliases (`SPEC.md` section 2), `langc fmt`,
`langc check --json` and the renamed core names. S2a adds the first part
of the cubical kernel: the interval `I`, `PathP` and `Path`, `transp`,
`hcomp` and systems. S2b adds indexed families, path constructors of
dimension 1 in cell signatures and the refusal codes `REFUSE_CELL` and
`DOMAIN_FAMILY` (a hand port from quorum-lang 8dee55b). S2c adds codata
(`codata`), the CC branch of `REFUSE_CELL`, the corecursor and the `head`
and `tail` destructors (a hand port from intent-lang 67445ae). S2d adds
`Fin n` as a defined Sigma type, the kit types `Sum` and `Unit` and the
ASCII value printer. S2e adds univalence: `Glue`, `glue`, `unglue` and
`ua` (a hand port from zkpolicy-lang 1d263fb). S3 adds the evolve domain
(`domain/domain.lang`: the states, the apps and the 11 steps), the forms
`table`, `app` and `protocol`, and the step check J1. S4 adds the
compatibility checker: J2 (`compat`), J4 and J5, the codes of
`docs/PLAN.md` 4.4, the demo `examples/rename.lang` and the laws L1, L4,
L5 and L6. S5 adds the runtime obligations (`Guard`, `stageGuard`) and
the JSON compatibility report `langc report FILE PROTOCOL` (`SPEC.md` 6.2).
There is no SQL writer yet. `make check` passes.
`docs/PLAN.md` section 9 gives the slices, and `docs/STATUS.md` gives the state.

evolve-lang does not promise zero downtime. The checker proves
compatibility facts about the declared model only. Lock wait time, traffic,
long transactions, replication lag, trigger cost, disk space and whether
the application code matches its declared requirements are outside the
model (`docs/PLAN.md` 6.2). The planned SQL writer adds runtime checks for
some of these facts.

## When to use evolve-lang

| Task | evolve-lang? |
|---|---|
| a schema change while old application versions still run | yes (S4) |
| a multi-tenant rollout with tenants at mixed stages | yes (S4) |
| a resumable backfill | yes (S6) |
| NOT NULL by CHECK NOT VALID, then VALIDATE | yes (S6) |
| ad hoc SQL, data analysis, one-off local schemas | no |
| MySQL, SQLite, SQL Server or other SQL dialects | no: refused (`docs/PLAN.md` 3.3) |

## Files

- `design/DESIGN.md`: the USER ask and the rulings. Sections 1 and 2 are
  verbatim.
- `docs/PLAN.md`: rulings, the R1 reading, the base signature, the
  transition model, targets, laws, agent adoption, slices and questions.
- `AGENTS.md`: how an agent uses this repository.
- `SPEC.md`, `formers/`, `probe/`, `domain/`, `docs/STATUS.md`,
  `docs/VALIDATION.md`, `docs/host/README.md`: the kit files. The slices
  fill them.

## Build

`make check` builds `build/langc` with tcc and runs the gate. The gate
needs tcc, node, wasm-validate and wasm-objdump.

## License

MIT OR Apache-2.0.
