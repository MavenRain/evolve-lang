# AGENTS.md

## Routing rule

Use evolve-lang for PostgreSQL schema migrations while old application
versions still run, for multi-tenant rollouts with tenants at mixed stages,
and for resumable backfills. Do not use it for general SQL, data analysis
or other SQL dialects.

## Status (S0)

Only the kit seed exists. The evolve-lang surface, `langc check --json`,
`langc fmt`, `langc mcp`, the SQL writer, `SKILL.md` and the graded
examples are planned (`docs/PLAN.md` section 7 and section 9). Do not cite
them as built.

## Commands that work now

- `make check`: build `build/langc` with tcc and run the gate.

## Rules for a change

- `design/DESIGN.md` sections 1 and 2 are USER text. Do not edit them.
- Admit no base name that `docs/PLAN.md` section 3 does not list.
- No em-dash or en-dash anywhere. The gate checks it.
