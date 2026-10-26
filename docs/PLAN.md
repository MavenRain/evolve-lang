# evolve-lang plan

evolve-lang is a language for PostgreSQL schema migrations as checked
transitions. A program declares schema states, the read and write
requirements of each application version, and a migration protocol: a chain
of steps. The checker computes which application versions stay compatible at
each intermediate stage. It refuses a protocol that leaves a live
application version without a column that the version needs. The host is
TinyCC. The v1 targets are PostgreSQL SQL stage files, a JSON compatibility
report and a Wasm compatibility oracle module.

S0 status: only the tcc-wasm kit seed of lang-template, `design/DESIGN.md`
and this plan exist. No evolve-lang feature is built. Section 9 gives the
slices.

evolve-lang does not promise zero downtime. A static check cannot know lock
wait time, traffic, long transactions or replication lag (section 6.2).
The checker proves compatibility facts about the declared model. The
emitted SQL adds runtime checks for the facts that the checker cannot know.

## 1. Rulings and readings

USER rulings (2026-10-25, the request and AskUserQuestion; the words are in
`design/DESIGN.md` sections 1 and 2):

- R1 type formers: for type constructors and higher kinds, only cell monads
  and their duals, in a locally presentable, locally cartesian closed
  (infinity,1)-category, as Cubical TT realizes it. These are the same words
  as intent-lang, resolve-lang, mandate-lang, quorum-lang and leaf-lang.
- R2 base types and operations: only what the pasted evolve-lang
  description needs. Section 3 cites the sentence for each admitted name.
- R3 host: TinyCC.
- R4 v1 targets: PostgreSQL SQL stages, a JSON compatibility report and a
  Wasm checker module. The native runner `langc run` is NOT a v1 target.
  When a runner slice lands, it links libpq (O1).
- R5 agent adoption: the six bullets of the pasted list (section 7).

Readings RULED (a) by the USER on 2026-10-25 (section 11):

- Kit: the tcc-wasm seed (it has the Wasm writer and the differential
  gate), plus a port of the JSON writer of `hosts/tcc-json` for the report
  (Q2).
- Wasm target: TinyCC has no Wasm backend, so `langc` itself cannot become
  a Wasm module under R3. The Wasm writer emits a compatibility oracle per
  protocol. It is NOT the type checker in Wasm (Q3).
- No runner in v1: resumable progress and runtime checks move into the
  emitted SQL. A progress ledger table, a PL/pgSQL backfill procedure that
  commits per key-range batch, and guard `DO` blocks that raise an error
  when a runtime fact fails (Q4).
- PostgreSQL floor: PG 15 and later (Q5).
- Proofs: guest language only; no Lean or Agda export (Q6).
- Surface: the Agda-like Unicode surface of quorum-lang and leaf-lang (Q7).
- First demo: `examples/rename` checks GREEN; a variant that drops `name`
  while app `v1` is live gets `REFUSE_DROP_REQUIRED` with a fix hint (Q8).
- Declaration forms `table`, `app` and `protocol` that elaborate to guest
  terms (Q9).
- Rollout: deploy and retire steps are fleet barriers (Q10).

## 2. The reading of R1

### 2.1 The precedent reading

evolve-lang takes the reading that intent-lang (PLAN 2.2, USER ruling S1
Q1 (a) and Q2 (a), 2026-10-22) and leaf-lang (PLAN 2.1) use (Q1):

| Part | Cubical TT | R1 limit |
|---|---|---|
| Ambient | Pi, Sigma, `Path`, `PathP`, `I`, `i0`, `i1`, `transp`, `hcomp`, `U`, `Glue`, `ua`, the builtin `Unit` | none: the internal language of the category |
| CM, cell monad | a `family` with a type parameter: the parameter is the one field of one generator `eta : A -> T A`; each other constructor takes closed base data and arguments of type `T A`, or is a path in `T A` | else `REFUSE_CELL` |
| CC, cocell comonad | a `cofamily` with one observation `eps : D A -> A`; each other field has type `D A -> D A` or `D A -> E -> D A` with `E` closed data | else `REFUSE_CELL` |

Why the two adjectives matter: local presentability gives the free algebra
of each set of cells, so each cell signature has its monad. Local cartesian
closure gives Pi and Sigma by base change, so the ambient part exists.

A family with no type parameter is a base type, not a type constructor. R2
limits it, not R1. This includes an indexed family with no type parameter,
such as `Step s s'` (quorum-lang D11 indexed families is the precedent).
Only `domain/domain.lang` declares a family or a cofamily. A program that
declares one gets `REFUSE_DATA`.

### 2.2 Every type former evolve-lang needs

| Former | Use in evolve-lang | Class | Reason |
|---|---|---|---|
| Pi, `A → B` | every function; `compat : App → Stage → Verdict`; requirement vectors `Fin k → Col` | ambient | LCC |
| Sigma, `(x : A) × B` | `Column`, `Table`, `App`, schema `State` | ambient | iterated anonymous Sigma; no `record` form (as leaf-lang S2-Q3 (b)) |
| `Fin n` | column names, stage indices, wave indices | defined | `(k : ℕ) × Path Bool (lt k n) true` |
| vectors | the columns of a table, the reads and writes of an app | ambient | `Fin n → X`, no new former |
| `Path`, `PathP`, `I`, `transp`, `hcomp` | laws and proofs (section 6) | ambient | the identity type of Cubical TT |
| `U`, `Glue`, `ua` | the rename as an equivalence of row types: once the backfill holds, `ua` of the old-row to new-row equivalence transports reads (law L3) | ambient | kept as in the precedent |
| `Step : State → State → U` | one schema or deployment step, typed by its source and target state | base, indexed (R2) | no type parameter; the dependent part of the description |
| `Chain : State → State → U` | a protocol from `s0` to `sn`: `nil` and `snoc` | base, indexed (R2) | the free category on the `Step` graph; no type parameter, so R1 does not apply |
| `Option A` | the lookup of a column in a state | CM | one 0-cell `none`, generator `some` |
| `Trunc A` | "a compatible stage exists", proof irrelevance in laws | CM | one 1-cell `squash` |
| `Guard A` | runtime obligations: a value that holds only if the runtime facts hold | CM | generator `done : A → Guard A`; one operation `need : Fact → Guard A → Guard A` with closed data `Fact` (the free cell monad of one operation, as mandate-lang 4.6) |
| `Stream A` | the batches of a backfill: `Stream KeyRange` | CC | observation `head : Stream A → A`, field `tail : Stream A → Stream A` |
| cons `List A` | none | REFUSED | `cons : A → List A → List A` uses `A` outside the generator; vectors over `Fin` do the work |
| `FMSet A`, `Bag A` (set quotients) | none | not admitted | requirement sets are vectors with a decidable membership test; `compat` is a for-all over `Fin k`, so order does not change it |
| `Either E A` | none | not admitted | `Verdict` (a base sum, section 3) carries the code |
| `data Pair (A : U) := mk A A` | none | REFUSED | `REFUSE_CELL` example of the precedent; the ambient `A × A` is the admitted form |

## 3. The minimal base signature (R2)

### 3.1 The description sentences

Each admitted name cites one of these sentences of `design/DESIGN.md`
section 1:

- D1 "Declare schemas"
- D2 "application read/write requirements"
- D3 "and a migration protocol"
- D4 "The compiler checks which application versions remain compatible at
  each intermediate stage."
- D5 the five rename stages: 1 "Add the new column." 2 "Install a
  compatible write path." 3 "Backfill with resumable progress." 4 "Switch
  readers." 5 "Remove the old column once its consumers are retired."
- D6 "including mixed application versions and partial rollout" and
  "leaving half the tenants migrated"
- D7 "PostgreSQL SQL plus a small runner and a machine-readable
  compatibility report" (the runner is deferred by R4)
- D8 "PostgreSQL's documented constraint-validation and locking behavior
  provides concrete semantics to model."
- D9 "Reject dropping a column while an active application version still
  requires it."
- D10 "runtime checks for facts the compiler cannot know"
- D11 "Avoid promising that static checking alone guarantees zero
  downtime."

### 3.2 Admission table

| Name | Kind | Serves | Slice |
|---|---|---|---|
| `Bool`, `ℕ` | base (kit) | indices, flags | kit |
| `table`, `column` declarations; `Col`, `Tbl` (finite per program, `Fin n` with a name table for the writers) | declaration form, base | D1 | S3 |
| `ColType` = `text`, `bigint` | base sum | D1; the demo needs a `bigint` key and `text` columns (Q13) | S3 |
| `Null` = `nullable`, `notNull` | base sum | D1, D8 | S3 |
| `CState` = `absent`, `notValid`, `valid` | base sum | D8: the state of the not-null CHECK of a column | S3 |
| `State` | Sigma | D1: tables, columns, constraint states, installed sync triggers, filled columns | S3 |
| `app` declaration; `App` = reads, writes, inserts (vectors of `Col`) | declaration form, Sigma | D2 | S3 |
| `addColumn t c ty` | step | D5.1 | S3 |
| `installSync t c c'`, `dropSync t c c'` | step | D5.2: the compatible write path (a trigger that copies a write of one column into the other, both ways) | S3 |
| `backfill t c ← c' batch n` | step | D5.3 | S3 |
| `deploy a`, `retire a` | step (fleet barrier) | D5.4, D5.5, D6 | S3 |
| `dropColumn t c` | step | D5.5, D9 | S3 |
| `addNotNullCheck t c`, `validate t c`, `setNotNull t c`, `dropCheck t c` | step | D8: CHECK (c IS NOT NULL) NOT VALID, VALIDATE, then SET NOT NULL with the scan skipped | S3 |
| `createIndexConcurrently t c` | step | D8 (locking) and the ruled option text of R4 (Q11) | S6 |
| `Step`, `Chain`, `protocol` declaration, `nil`, `snoc`, `_⨾_` | base indexed family, declaration form | D3 | S3 |
| `Stage` = `Fin (n + 1)` for a chain of `n` steps | defined | D4 | S3 |
| `compat`, `fresh`, `present`, `filled` | guest functions | D4 | S4 |
| `Segment`, `segments` | guest function | D6: the stages between two barriers, where tenants can be at mixed stages | S4 |
| `Verdict` = `compatible`, `refused Code`, `runtime Fact` | base sum | D4, D7, D10 | S4 |
| `Code` (the refusal codes of 4.4) | base sum | D4, D9 | S4 |
| `Fact` = `backfillDone t c`, `retired a`, `fleetAt s` | base sum | D10 | S5 |
| `Guard` | CM | D10 | S5 |
| `LockMode` = `accessExclusive`, `shareRowExclusive`, `shareUpdateExclusive`, `rowExclusive`, `noLock` | base sum | D8 | S4 |
| `ScanClass` = `metadata`, `scan`, `batched`, `noScan` | base sum | D8 (no admitted step rewrites a table) | S4 |
| `lockOf`, `scanOf` | guest functions | D8, D11: the report shows the lock of each stage | S4 |
| `KeyRange`, `Stream KeyRange` | Sigma, CC | D5.3 | S6 |
| `langc report` (JSON), SQL writer, Wasm writer | targets | D7, R4 | S5 to S7 |

### 3.3 Refused candidates

| Candidate | Reason |
|---|---|
| other SQL dialects (MySQL, SQLite, SQL Server) | the description names PostgreSQL only |
| an arbitrary SQL string or expression | the checker cannot model it; the description wants the protocol to be the checked object |
| general CHECK predicates, foreign keys, unique constraints | no description sentence needs them; the not-null CHECK is the one D8 form the demo uses |
| `RENAME COLUMN` in place | it breaks each live old version at once; refused with `REFUSE_RENAME_INPLACE` and the fix "use the five-stage protocol of examples/rename" |
| column types other than `text` and `bigint` | the demo does not need them (Q13) |
| volatile defaults, generated columns, identity columns, domain types | PG rewrites the table for them under ACCESS EXCLUSIVE (section 5.1) |
| views, sequences, partitioning, schemas (namespaces), roles, grants | no description sentence needs them |
| ORM models, application code | evolve-lang declares requirements; it does not parse an application |
| a static zero-downtime claim | D11 |
| strings (the kit refuses them) | names are declared identifiers; the writers print them as quoted identifiers |

## 4. The transition model

### 4.1 Objects

- A schema state `State` gives, for each table, its key column and its
  columns. Each column has a type, a nullability and a not-null CHECK
  state (`absent`, `notValid`, `valid`). The state also records the
  installed sync triggers and the columns that a backfill filled.
- An application version `App` gives three vectors of columns: the columns
  it reads, the columns it writes on update, and the columns it supplies on
  insert.
- A step `Step s s'` changes the schema state `s` into `s'`. The index
  is the precondition: `dropColumn t c` has a type only when `c` is in
  `s`. A deploy or retire step changes the set of live versions and not
  the schema. It is a fleet barrier (4.3).
- A protocol `Chain s0 sn` is a chain of steps. Stage `i` is the state
  after the first `i` steps. Stage 0 is `s0`.
- A live set gives, for each stage, the application versions that run
  against it. The `deploy` and `retire` steps of the chain give it.

### 4.2 Judgements

- J1 step typing: `s ⊢ step : s'`. A step that fails its index gets
  `REFUSE_STEP_INDEX`. Examples: add a column that exists; drop a column
  that does not exist; `validate` with no `notValid` CHECK.
- J2 compatibility `compat(a, s)` is decidable. It holds when:
  1. each column that `a` reads, writes or inserts is present in `s`;
  2. each column that `a` reads is fresh: each live writer of the same
     field writes this column, or writes a column with an installed sync
     trigger to this column;
  3. each column that `a` reads after a `deploy` that switches readers is
     filled, or a runtime fact says so (4.5);
  4. each `notNull` column, or a column with a `notValid` or `valid`
     CHECK, gets a value on each insert by a live version: the version
     inserts it, or a sync trigger fills it.
- J3 segments: the barriers cut the chain into segments. Tenants can be at
  any stage of the current segment at the same time (D6). The fleet passes
  a barrier only when each tenant has run each step before it. The emitted
  guard checks this (fact `fleetAt s`).
- J4 protocol acceptance: the checker accepts the protocol when J1 holds
  for each step and, for each segment, each version live in the segment
  and each stage of the segment, `compat` holds or depends only on runtime
  facts. Else it refuses with the first code of 4.4 in chain order.
- J5 lock class: each step has a lock mode and a scan class (5.1). The
  checker refuses a step that would scan a table under ACCESS EXCLUSIVE
  (`REFUSE_LOCK_SCAN`). No admitted step rewrites a table.

### 4.3 Mixed versions and partial rollout

In one segment, the fleet can hold tenants at each stage of the segment,
and each version that is live in the segment can reach each of these
tenants. So J4 asks `compat` for each pair (live version, stage of the
segment), not only for the pairs on the diagonal. A barrier ends the
segment: `deploy v3` runs only after each tenant passed each step before
it. This is the "half the tenants migrated" state of the description, made
explicit: the report gives each segment, its stages and its live versions.

A rollout plan with canary tenants and waves (Atlas, `design/DESIGN.md`
section 3) is an order inside a segment. It does not change J4, because J4
already covers each stage of the segment (Q10).

### 4.4 Refusal codes (`langc check --json`)

| Code | When | Fix hint |
|---|---|---|
| `REFUSE_DROP_REQUIRED` | a `dropColumn t c` stage while a live version reads, writes or inserts `c` (the demo, D9) | "retire VERSION before stage N" |
| `REFUSE_MISSING_COLUMN` | a live version needs a column that is not present at a stage of its segment | "move the deploy of VERSION after stage N" |
| `REFUSE_STALE_READ` | a live version reads a column that another live version updates through a different column, with no sync trigger | "install a sync trigger before stage N" |
| `REFUSE_UNFILLED_READ` | a live version reads a column that no earlier backfill fills | "add a backfill step before the deploy of VERSION" |
| `REFUSE_NULL_INSERT` | a live version inserts no value into a not-null or CHECK column and no trigger fills it | "install a sync trigger, or retire VERSION before stage N" |
| `REFUSE_STEP_INDEX` | J1 fails | the expected state |
| `REFUSE_LOCK_SCAN` | J5 fails, for example `setNotNull` with no `valid` CHECK before it | "add addNotNullCheck and validate before setNotNull" |
| `REFUSE_RENAME_INPLACE` | a rename in place (3.3) | "use the five-stage protocol" |

Each code has a stable spelling, a test with its JSON shape (S4) and a line
in `docs/DIAGNOSTICS.md`.

### 4.5 Runtime obligations

The checker cannot know these facts. Each one becomes a guard in the
emitted SQL and a `runtime` row in the report:

| Fact | Where | Guard |
|---|---|---|
| `backfillDone t c` | before a deploy that reads `c` | the progress ledger has `done` for the backfill of `t.c`; no row has `c` null where the source column is not null |
| `retired a` | before a stage that is safe only without `a` | the version registry has no live row for `a` |
| `fleetAt s` | at each barrier | each tenant ledger shows stage `s` or later |
| rows valid | `validate t c` | VALIDATE itself checks the old rows and fails the stage if one is bad |

### 4.6 The rename example (D5)

Versions: `v1` reads, writes and inserts `name`. `v3` reads, writes and
inserts `display_name`. The table groups the steps by the five stages of\nthe description (D5); the checker gives each step its own stage number.

| D5 stage | Step | Live | v1 | v3 |
|---|---|---|---|---|
| 0 | start: `users(id bigint, name text)` | v1 | compatible | refused: `REFUSE_MISSING_COLUMN` |
| 1 | `addColumn users display_name text` | v1 | compatible | refused: `REFUSE_UNFILLED_READ` |
| 2 | `installSync users display_name name` | v1 | compatible | refused: `REFUSE_UNFILLED_READ` |
| 3 | `backfill users display_name ← name batch 1000` | v1 | compatible | runtime: `backfillDone users display_name` |
| 4 | `deploy v3` (barrier) | v1, v3 | compatible (the sync trigger keeps `name` fresh) | runtime: `backfillDone users display_name` |
| 5 | `retire v1` (barrier), `dropSync`, `dropColumn users name` | v3 | refused: `REFUSE_MISSING_COLUMN` | compatible |

The demo variant drops `name` at stage 5 with no `retire v1`. The checker
refuses it with `REFUSE_DROP_REQUIRED` and the hint "retire v1 before stage
5".

## 5. Targets

### 5.1 SQL writer (PostgreSQL)

`langc sql FILE PROTOCOL OUTDIR` writes one file per stage,
`NN-STEP.sql`. Each file:

- starts with a comment: the source hash, the `langc` commit and the
  stage;
- sets `lock_timeout` for the stage (Q14);
- runs the guards of the stage first (4.5): a `DO` block that raises
  `EVOLVE_GUARD FACT` when a fact fails;
- updates the progress ledger `evolve_progress` and, for deploy and retire,
  the version registry `evolve_app_versions`.

Lock and scan facts. Each was checked on 2026-10-25 against the PostgreSQL
18.6 docs (`sql-altertable.html`, `explicit-locking.html`,
`sql-createindex.html`, `runtime-config-client.html`,
`plpgsql-transactions.html`):

| Step | SQL | Lock | Scan class |
|---|---|---|---|
| `addColumn` | `ADD COLUMN` with no default, nullable | ACCESS EXCLUSIVE (the ALTER TABLE default) | metadata: "In neither case is a rewrite of the table required" |
| `installSync`, `dropSync` | `CREATE TRIGGER`, `DROP TRIGGER` | SHARE ROW EXCLUSIVE for CREATE TRIGGER | metadata |
| `backfill` | `CALL evolve_backfill_...()`: a PL/pgSQL procedure that updates one key range, writes the ledger and commits, then goes to the next range | ROW EXCLUSIVE and row locks per batch | batched |
| `dropColumn` | `DROP COLUMN` | ACCESS EXCLUSIVE | metadata: the column becomes invisible, no rewrite |
| `addNotNullCheck` | `ADD CONSTRAINT ... CHECK (c IS NOT NULL) NOT VALID` | ACCESS EXCLUSIVE | metadata: NOT VALID skips the scan |
| `validate` | `VALIDATE CONSTRAINT` | SHARE UPDATE EXCLUSIVE (does not conflict with ROW EXCLUSIVE or ACCESS SHARE) | scan |
| `setNotNull` | `SET NOT NULL` after a valid CHECK | ACCESS EXCLUSIVE | metadata: "if a valid CHECK constraint exists ... which proves no NULL can exist, then the table scan is skipped" |
| `createIndexConcurrently` | `CREATE INDEX CONCURRENTLY` outside a transaction block | SHARE UPDATE EXCLUSIVE | scan; a failure leaves an INVALID index, which the stage drops and builds again |
| `deploy`, `retire` | registry update only | no table lock on user tables | none |

Other confirmed facts: `lock_timeout` aborts a statement that waits longer
than the limit for a lock, and "applies separately to each lock
acquisition attempt". A procedure run by a top-level `CALL` can `COMMIT`
in a loop; a `CALL` with an intervening command cannot. So the backfill
stage file has no outer transaction. The 18.6 ALTER TABLE page allows NOT
VALID for "foreign-key, CHECK, and not-null constraints" (O4).

NOT confirmed in the docs (2026-10-25): a lock request that waits blocks
the later requests behind it. `explicit-locking.html` does not say so. The
plan does not rely on it; `lock_timeout` bounds each wait (O5).

The offline gate compares the emitted files with golden files in
`test/sql/`. `make check-pg` (optional, not in `make check`) runs the stages
against a Docker PostgreSQL with two client loops that act as `v1` and
`v3`.

### 5.2 JSON compatibility report

`langc report FILE PROTOCOL` prints one JSON document with stable keys:
`evolve` (format version), `protocol`, `stages` (index, steps, lock, scan),
`apps`, `segments` (stages, live versions), `matrix` (one row per app and
stage: `verdict` is `compatible`, `refused` with `code` and `fix`, or
`runtime` with `fact` and `guard`), and `progress`. The writer is a port of
`hosts/tcc-json/src/json.c` (Q2).

### 5.3 Wasm compatibility oracle

`langc wasm FILE PROTOCOL` emits a Wasm MVP module for one protocol. It
exports `stages()`, `apps()`, `verdict(app, stage)` (the code as `i32`) and
`guard(stage, facts)` (the facts as a bit mask; the result is the first
failed fact or 0). CI and sandboxed agents with no C toolchain call it. The
kit differential gate compares each export with `langc eval` on the same
inputs. The module holds the computed table; it is NOT the type checker
(Q3).

### 5.4 Runner (deferred, R4)

`langc run` is not in v1. When it lands, it links libpq (TLS, each auth
method, COPY) and runs the stage files with retries on lock timeout. The
integration tests use a Docker PostgreSQL. libpq is not installed on this
machine on 2026-10-25 (O1, O2).

## 6. Laws (guest terms)

### 6.1 Laws

- L1 stage soundness: if `check` accepts a protocol, then for each segment,
  each live version and each stage of the segment, `compat` holds or is a
  runtime verdict. Proof by reflection: the decision procedure returns
  `true` only when the Pi statement has a proof.
- L2 backfill resume: a batch run two times gives the same table as one
  run, and a run that starts from the ledger point gives the same table as
  a run with no stop. Stated over `Stream KeyRange`.
- L3 write-path consistency: after `installSync` and while it is
  installed, each write keeps `display_name = name` on the rows it touches.
  After `backfillDone`, this holds for each row. Then `ua` of the row
  equivalence transports the reads of `v1` to the reads of `v3`.
- L4 drop safety: no version that is live at or after a `dropColumn t c`
  stage reads, writes or inserts `c`.
- L5 chain laws: `_⨾_` is associative and `nil` is its unit (`Path`).
- L6 lock map: no admitted step has scan class `scan` under ACCESS
  EXCLUSIVE.

### 6.2 Outside the denotation

The laws are about the declared model. They say nothing about: the time a
step waits for a lock; traffic and the load that a batch adds; long
transactions that hold locks; replication lag; the cost of triggers; disk
space; whether the real application code matches its declared
requirements; the server version; and an operator who runs the stage files
out of order. The guards (4.5) check some of these at run time. The rest
stay outside. So evolve-lang does not claim zero downtime (D11).

## 7. Agent adoption

Each bullet of the pasted list (`design/DESIGN.md` section 1), with its
deliverable, slice and gate. Anything beyond the six bullets is marked
EXTRA.

| Bullet | Deliverable | Slice | Gate |
|---|---|---|---|
| 1. One canonical form per operation and a `langc fmt` canonical printer | the Agda-like surface (Q7) with one spelling per form; `langc fmt`; `W_NONCANON` with a fix for non-canonical input | S1 | `fmt` is idempotent and round-trips on each example and on `test/fmt/`; each example is canonical |
| 2. `langc check --json` with fix hints and progress counts (parsed, typed, laws discharged) | JSON keys `code`, `span`, `expected`, `found`, `fix`, `retry`; `progress` with `parsed`, `typed`, `laws` (n of m) and, from S4, `stages` and `pairs` checked; `docs/DIAGNOSTICS.md` | S1, S4 | each refusal code has a fixture that checks its JSON shape; counts in each result |
| 3. `SKILL.md` with 3 to 5 graded examples | `skills/evolve-lang/SKILL.md` with 5 examples from easy to hard (add a nullable column; the rename; the rename with a too-early drop; NOT NULL by CHECK NOT VALID and VALIDATE; a two-segment rollout with a stale read), each with its expected `check --json` output | S8 | each example gives the output in the file |
| 4. An MCP server written in TinyCC (`langc mcp`) | a stdio MCP server in C that tcc builds; tools `check`, `fmt`, `report`, `explain-code`, `list-steps` | S8 | an MCP stdio test drives `check` on each graded example |
| 5. `AGENTS.md` and a README table saying when to use the language | `AGENTS.md` routing rule; README "When to use evolve-lang" table | S0, S8 | the table matches the admitted steps |
| 6. A token-count comparison against Python and Rust | `docs/TOKEN-BASELINE.md`: the rename protocol with its compatibility check in evolve-lang, Python and Rust; the tokenizer is named (Q12); a script gives the counts | S9 | the script output equals the counts in the file |
| EXTRA | the Wasm oracle (5.3) lets an agent with no C toolchain read verdicts | S7 | differential gate |

## 8. Port sources (pinned 2026-10-25)

| Repo | SHA | What |
|---|---|---|
| lang-template | 5265a2a | `hosts/tcc-wasm` (the seed); `hosts/tcc-json/src/json.{c,h}` (the report writer) |
| leaf-lang | e3e2338 | S1 surface: Agda-like Unicode, `langc fmt`, `check --json`; PLAN shape |
| quorum-lang | 7afa479 | S1 surface (second source); PLAN 2; D11 indexed families |
| intent-lang | 67445ae | `REFUSE_CELL`, cofamily; PLAN 2.2 |
| resolve-lang | 510c137 | `REFUSE_CELL` (second implementation) |
| mandate-lang | 17d50a4 | the free cell monad of operations (PLAN 4.6), for `Guard` |
| agent-lang | 06854bf | no JSON diagnostics (leaf-lang S1 read it; `json.c` writes the Json target) |
| dil-lang | 9e57528 | no JSON diagnostics (leaf-lang S1 read it; `--json` is a build output path) |

No *-lang repository has an MCP server in C yet (O3).

## 9. Slices

One session each. Each slice: scope, gate (`make check`, EXIT 0), mutants
on the new checks, docs, close (stage, commit message, next prompt).

| Slice | Scope | Gate and mutants |
|---|---|---|
| S0 | kit seed, `design/DESIGN.md`, this plan, README, `AGENTS.md` | `make check` EXIT 0: examples 10 checked, 0 failed; parse refusals 9; check refusals 19; evals 32; ir 10 lowered; wasm 13 modules valid; differential 813 cases, 0 mismatches; gate 0 failures; review and domain regressions passed |
| S1 | surface: Agda-like Unicode (Q7), `langc fmt`, `W_NONCANON`, `check --json` with progress counts; drop the refused kit names (port from leaf-lang e3e2338) | `make check` EXIT 0: examples 10 checked, 0 failed; parse refusals 9; check refusals 18; evals 30; ir 10 lowered; wasm 13 modules valid; differential 772 cases, 0 mismatches; fmt 50 canonical, 3 non-canonical pairs (round trip and idempotence); json 20 fixtures, diagnostics 31 codes (18 with a fixture, 13 with no fixture); gate 0 failures; review and domain regressions passed; mutants M1 to M6 killed (docs/STATUS.md, S1) |
| S2 | kernel port: Path, Sigma, Pi, U, `I`, `transp`, `hcomp`, `REFUSE_CELL`, cofamily, indexed families, `Fin`, `Option`, `Trunc`, `Stream` | port tests; `REFUSE_CELL` mutants |
| S3 | domain: `ColType`, `Null`, `CState`, `State`, `App`, the steps, `Step`, `Chain`; the `table`, `app` and `protocol` forms (Q9); `REFUSE_STEP_INDEX`, `REFUSE_RENAME_INPLACE` | one test per step index; mutants on each precondition |
| S4 | the checker: `compat`, `fresh`, `segments`, J4, J5, the codes of 4.4; `examples/rename` GREEN and the drop-early variant refused (the demo); L1, L4, L5, L6 | one test per code with its JSON shape; mutants on each judgement |
| S5 | JSON report (port of `json.c`), `Fact`, `Guard`, runtime rows | golden reports; mutants on the verdict map |
| S6 | SQL writer: stage files, `lock_timeout`, guards, ledger, backfill procedure, sync triggers, the CHECK path, `createIndexConcurrently` (Q11); L2, L3; `make check-pg` (optional) | golden SQL files; mutants on lock and guard output |
| S7 | Wasm oracle writer | differential gate against `langc eval` |
| S8 | agent kit: `SKILL.md` with 5 graded examples, `langc mcp`, `explain-code`, final `AGENTS.md` and README table | MCP stdio test; each example passes `check` |
| S9 | `docs/TOKEN-BASELINE.md` and its script | the script output equals the file |
| S10 | close: `docs/STATUS.md`, `docs/VALIDATION.md`, full gate, mutant summary | full gate EXIT 0 |

## 10. Open items

- O1 the runner `langc run` (R4: libpq when it lands); integration tests on
  a Docker PostgreSQL.
- O2 libpq, psql and postgres are not installed on this machine
  (2026-10-25); docker is. USER step: `brew install libpq` before O1.
- O3 the MCP server has no C precedent in the *-lang repositories. S8 pins
  the MCP protocol revision it implements.
- O4 the PG 18 form `NOT NULL ... NOT VALID`. The 18.6 ALTER TABLE page now
  lists not-null constraints for NOT VALID; the docs bug of 18.4
  (`design/DESIGN.md` section 3) looks fixed. A version-gated form (Q5 RULED (a)).
- O5 lock queue behavior (a waiting ACCESS EXCLUSIVE request blocks later
  readers) is not stated in `explicit-locking.html`. Confirm from another
  PostgreSQL source before a doc cites it.
- O6 the Reddit thread of the description was not found. Prior art for
  multi-tenant rollout: Atlas staged rollouts, Turso.
- O7 Prisma 8 (renamed from Prisma Next) GA was expected in October 2026.
  Check its status before the README compares with it (S8).
- O8 the kit sample domain (`domain/domain.lang`) and its examples stay
  until S3 replaces them. S1 moved them to the S1 surface.
- O9 column types beyond `text` and `bigint` on a USER ask only (Q13).
- O10 to O16 come from S1 (`docs/STATUS.md`, S1, open items): the ASCII
  alias policy, `let`, the kit `Sum` and `Unit`, `flagIf`, dead core paths,
  the ASCII value printer, and `nil` for `Chain` in S3.

## 11. Questions for the USER

Each item has options. Marks (2026-10-25): RULED (letter) is a USER ruling.
USER ruling 2026-10-25, verbatim: "Rule a for Q1-Q14".

- Q1 R1 reading. RULED (a). (a) the intent-lang ruled reading (2.1): ambient Pi,
  Sigma, Path, U, Glue are not limited; declared type constructors are
  only CM and CC; the cell criterion is syntactic; cons is refused;
  indexed base families such as `Step` are R2 base types. (b) the same
  without Glue and `ua`.
- Q2 kit. RULED (a). (a) tcc-wasm seed, report writer ported from tcc-json. (b)
  tcc-json seed and a port of the Wasm writer.
- Q3 Wasm target. RULED (a). (a) a compatibility oracle per protocol, emitted by the
  Wasm writer of `langc` (5.3); it is not the type checker. (b) build
  `langc` with clang for wasm32-wasi (this breaks R3). (c) drop the Wasm
  target from v1.
- Q4 runtime facts with no runner. RULED (a). (a) in the emitted SQL: the ledger, the
  backfill procedure with a commit per batch, guard `DO` blocks and the
  version registry. (b) report rows only; an operator does the checks.
- Q5 PostgreSQL floor. RULED (a). (a) PG 15 and later, with the CHECK NOT VALID,
  VALIDATE, SET NOT NULL path, and PG 18 `NOT NULL NOT VALID` as a
  version-gated form. (b) PG 18 only. (c) PG 14 and later. EOL dates
  (postgresql.org, 2026-10-25): 14 on 2026-11-12, 15 on 2027-11-11, 16 on
  2028-11-09, 17 on 2029-11-08, 18 on 2030-11-14.
- Q6 proofs. RULED (a). (a) guest language only. (b) also a Lean export.
- Q7 surface. RULED (a). (a) the Agda-like Unicode surface of quorum-lang (Q13) and
  leaf-lang (Q14). (b) ASCII keywords. Unicode symbols can cost more
  tokens; S9 measures it.
- Q8 demo. RULED (a). (a) `examples/rename` (4.6) GREEN; the variant with no `retire
  v1` gets `REFUSE_DROP_REQUIRED` with "retire v1 before stage 5". (b) a
  different first demo.
- Q9 declaration forms. RULED (a). (a) dedicated forms `table`, `app` and `protocol`
  that elaborate to guest terms. (b) plain guest definitions over the
  domain types only.
- Q10 rollout model. RULED (a). (a) barrier segments (4.3). (b) an explicit wave
  schedule: time and wave give a stage.
- Q11 `CREATE INDEX CONCURRENTLY`. RULED (a). (a) admit it as a step (the ruled option
  text names it; D8). (b) refuse it until a description sentence needs it.
- Q12 tokenizer for the baseline. RULED (a). (a) a fixed offline proxy, as leaf-lang
  Q13. (b) a named BPE tokenizer through a script.
- Q13 column types. RULED (a). (a) `text` and `bigint` only. (b) also `integer`,
  `boolean`, `timestamptz` and `uuid`.
- Q14 `lock_timeout` per stage. RULED (a). (a) 2 s by default, a protocol can set it.
  (b) 5 s. (c) no default; each protocol must set it.
