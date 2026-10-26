# evolve-lang domain design

This file is the only source of the core types and the core operations of
evolve-lang. `docs/PLAN.md` sections 2, 3 and 4 cite it. Do not add a core
type or a core operation that this file and the PLAN admission table (PLAN
section 3) do not give.

Section 1 is the USER text of 2026-10-25, verbatim. Do not edit section 1.
Section 2 holds the USER rulings of the same day as recorded at the time.
Section 3 holds the research facts that the target question used. The
sources are in `evolve-lang-work/USER-ASK.md` outside this repository.

## 1. The USER ask (2026-10-25, verbatim)


I want to write a programming language with the following characteristics:

For type constructors/higher-kinded types, admit only cell monads and their duals in a locally presentable locally cartesian closed \((\infty,1)\)-category, as represented computationally in Cubical Type Theory.

For base types and operations, admit only what is required to satisfy the following description:

```
1. evolve-lang: database migrations as checked transitions
The problem: A schema change can be valid SQL while breaking an older application version, blocking production traffic, or leaving half the tenants migrated. A recent Reddit discussion about migrations across hundreds of tenant databases describes exactly that coordination problem.
The language: Declare schemas, application read/write requirements, and a migration protocol. The compiler checks which application versions remain compatible at each intermediate stage.
For example, renaming name to display_name becomes:
1. Add the new column.
2. Install a compatible write path.
3. Backfill with resumable progress.
4. Switch readers.
5. Remove the old column once its consumers are retired.
Why this deserves a DSL: The interesting object is the whole transition protocol, including mixed application versions and partial rollout. That gives dependent types and state transitions something useful to express.
First target: PostgreSQL SQL plus a small runner and a machine-readable compatibility report. PostgreSQL's documented constraint-validation and locking behavior provides concrete semantics to model.
First compelling demo: Reject dropping a column while an active application version still requires it.
Differentiation: Migration graphs and verification already have competition, including Prisma Next's announced approach. Your opportunity would be explicit compatibility obligations across intermediate states, with runtime checks for facts the compiler cannot know. Avoid promising that static checking alone guarantees zero downtime.
```

The host language shall be TinyCC.  Use your judgment to provide a multiple-choice option for compilation target(s).

Finally, I want agents to reach for this language when appropriate.  To that end, also adhere to the following criteria:

```
  The plan for making agents reach for the language includes:
  - one canonical form per operation and a langc fmt canonical printer;
  - langc check --json, giving fix hints and progress counts (parsed, typed, laws
    discharged);
  - a SKILL.md with 3 to 5 graded examples;
  - an MCP server written in TinyCC (langc mcp);
  - AGENTS.md and a README table saying when to use the language;
  - a token-count comparison against Python and Rust.
```

## 2. The USER rulings (2026-10-25, AskUserQuestion, verbatim answers)

- Q "Which compilation targets should evolve-lang v1 emit?" (multi-select) = "PostgreSQL SQL stages (Recommended), JSON compat report (Recommended), Wasm checker module". NOT chosen: "Native runner `langc run` (Recommended)".
- Q "If the runner is in scope, how should it connect to PostgreSQL?" = "Link libpq (Recommended)". The question was conditional; the runner is not a v1 target, so this applies when a runner slice lands.

Option texts as shown to the USER:
- SQL stages: one .sql file per protocol stage, using the safe PostgreSQL forms: lock_timeout, ADD CONSTRAINT ... NOT VALID then VALIDATE, CREATE INDEX CONCURRENTLY, dual-write triggers, backfill in key-range batches.
- JSON compat report: a machine-readable matrix of app version x stage: compatible, refused (with the reason), or depends on a runtime check. CI and agents read it. Built on the tcc-json kit's writer.
- Wasm checker module: `langc check` and the report compiled to a Wasm module, for CI or sandboxed agents that have no C toolchain. Reuses the tcc-wasm kit's differential gate. Optional for v1.
- libpq: TLS, every auth method, COPY; build needs libpq headers (`brew install libpq`, NOT installed on this machine 2026-10-25); integration tests against a Docker Postgres.

## 3. Research facts (web, 2026-10-25)

- Prisma Next was renamed Prisma 8. A March 2026 Prisma post introduced it; Prisma 8 migrations are TypeScript files, "explicit, verifiable and safe to retry", each step verified before and after it runs. RC as of 2026-10-17, GA expected October 2026. Internal package `@prisma-next/migration-tools`: on-disk migration graph (reconstructGraph, findPath, detectCycles, detectOrphans), migration hashes. Sources: https://www.prisma.io/blog/prisma-next-roadmap , https://www.prisma.io/blog/data-migrations-in-prisma-next , https://socket.dev/npm/package/@prisma-next/migration-tools/overview/0.15.0 . Reading: Prisma checks the graph and each step; it does not model application versions that run against intermediate states. That is the gap the brief names.
- PostgreSQL 18 allows NOT NULL constraints added as NOT VALID; later VALIDATE CONSTRAINT runs under SHARE UPDATE EXCLUSIVE (reads and writes continue). The main ALTER TABLE page still says NOT VALID is FK-only (docs bug reported on 18.4). Sources: https://www.depesz.com/2025/05/01/waiting-for-postgresql-18-allow-not-null-constraints-to-be-added-as-not-valid , https://bytebase.com/blog/what-is-new-in-postgres-18 , https://www.postgresql.org/message-id/CAEdngj81QMQ%3D_a0DKhX%2BiUomLE%3DiPr3X598Pr1O%3DJyJs0s00Jw%40mail.gmail.com
- CHECK / FK NOT VALID then VALIDATE: new writes checked at once, old rows checked at VALIDATE (https://www.vincentschmalbach.com/adding-and-validating-postgresql-constraints-safely/). Other lock levels (ADD COLUMN, SET NOT NULL scan skip via a valid CHECK since PG 12, CREATE INDEX CONCURRENTLY outside a transaction, lock queue behind an ACCESS EXCLUSIVE waiter) = from memory, NOT confirmed this session: the builder confirms each against the PostgreSQL docs before PLAN cites it.
- The Reddit thread was not found by search. Multi-tenant rollout prior art: Atlas staged rollouts (canary tenants, waves; https://atlasgo.io/guides/database-per-tenant/rollout), Turso (https://turso.tech/blog/how-to-deploy-schema-changes-to-a-million-databases), per-schema migration ledgers for resume.
- Local tools 2026-10-25: tcc at /Users/oobi/.local/bin/tcc; docker at /usr/local/bin/docker; NO psql, pg_ctl, initdb, postgres, pg_config, libpq.
