# evolve-lang

evolve-lang is a language for PostgreSQL schema migrations as checked
transitions. A program declares schema states, the read and write
requirements of each application version, and a migration protocol: a chain
of steps. The checker computes which application versions stay compatible at
each intermediate stage. It refuses a protocol that leaves a live
application version without a column that the version needs. The host is
TinyCC. The planned targets are PostgreSQL SQL stage files, a JSON
compatibility report and a Wasm compatibility oracle module.

## Status (S0)

This repository is the tcc-wasm kit seed of lang-template, the design and
the plan. No evolve-lang feature is built yet. There is no SQL writer yet.
`make check` passes on the kit seed. `docs/PLAN.md` section 9 gives the
slices.

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
