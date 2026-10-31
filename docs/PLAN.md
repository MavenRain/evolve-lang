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

USER ruling (2026-10-26, request and AskUserQuestion), the decoupling
from leaf-lang:

- The evolve-lang slices do not wait for a leaf-lang commit. S2 splits
  into S2a to S2e (section 9). Each part ports from a fixed source that
  is on disk now (section 8).
- S2a ports the leaf-lang S2 working tree, frozen on 2026-10-26 at its
  GREEN U4 point, as one patch on leaf-lang e3e2338 (section 8). After
  S2a, evolve-lang reads no later leaf-lang state (O17).

Readings (NOT ruled): the S2b to S2e sources in section 8 (quorum-lang
8dee55b, intent-lang 67445ae, zkpolicy-lang 1d263fb).

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
| Pi, `A → B` | every function; `compat n m s0 s a live cols v : Verdict n m` (Q-S4-10 (a), Q-S4-11 (a)); requirement vectors `ℕ → Col` (Q-S4-9 (a)) | ambient | LCC |
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

The demo is `examples/rename.lang`. Versions: `v1` reads, writes and
inserts `name`. `v3` reads, writes and inserts `display_name`. Each version
also reads and inserts the key `id`: an app that inserts into a table also
inserts its key (J2.4, Q-S4-8 (a)). The table groups the steps by the five
stages of the description (D5); the checker gives each step its own stage
number. D5 stage 5 is the checker stages 5, 6 and 7.

| D5 stage | Step | Live | v1 | v3 |
|---|---|---|---|---|
| 0 | start: `users(id bigint, name text)` | v1 | compatible | refused: `REFUSE_MISSING_COLUMN` |
| 1 | `addColumn users display_name text` | v1 | compatible | refused: `REFUSE_UNFILLED_READ` |
| 2 | `installSync users display_name name` | v1 | compatible | refused: `REFUSE_UNFILLED_READ` |
| 3 | `backfill users display_name ← name batch 1000` | v1 | compatible | runtime: `backfillDone users display_name` |
| 4 | `deploy v3` (barrier) | v1, v3 | compatible (the sync trigger keeps `name` fresh) | runtime: `backfillDone users display_name` |
| 5 | `retire v1` (barrier), `dropSync`, `dropColumn users name` | v3 | refused: `REFUSE_MISSING_COLUMN` | compatible |

The demo variant `test/check/rename-drop-early.lang` has no `retire v1`: it
runs `dropSync` and `dropColumn users name` while `v1` is live. The checker
refuses it at checker stage 5 (`dropSync`) with `REFUSE_STALE_READ`: with no
sync, a write of `display_name` by `v3` does not change the `name` that `v1`
reads (Q-S4-13 (a)). The check stops at the first refusal, so it does not
get to `REFUSE_DROP_REQUIRED` at the `dropColumn` stage.

## 5. Targets

### 5.1 SQL writer (PostgreSQL)

`langc sql FILE PROTOCOL OUTDIR` writes `00-setup.sql` (the schema
`evolve`, its tables and functions) and one file per stage, `NN-STEP.sql`
(S6, SPEC.md 6.3). Each stage file:

- starts with a comment: the format version, the source hash and the
  stage (Q-S6-2 (a));
- sets `lock_timeout` for the stage (Q14, Q-S6-6 (a));
- skips the step when the progress ledger has the stage (Q-S6-18 (a));
- runs the guards of the stage first (4.5): a `DO` block that raises
  `EVOLVE_GUARD FACT` when a fact fails (Q-S6-21 (a));
- updates the progress ledger `evolve.progress` and, for deploy and
  retire, the version registry `evolve.app_versions` (Q-S6-3 (a), Q-S6-4
  (a)).

Lock and scan facts. Each was checked on 2026-10-25 against the PostgreSQL
18.6 docs (`sql-altertable.html`, `explicit-locking.html`,
`sql-createindex.html`, `runtime-config-client.html`,
`plpgsql-transactions.html`):

| Step | SQL | Lock | Scan class |
|---|---|---|---|
| `addColumn` | `ADD COLUMN` with no default, nullable | ACCESS EXCLUSIVE (the ALTER TABLE default) | metadata: "In neither case is a rewrite of the table required" |
| `installSync` | `CREATE TRIGGER` | SHARE ROW EXCLUSIVE | metadata |
| `dropSync` | `DROP TRIGGER` | ACCESS EXCLUSIVE (F-U0-2 (a); see the DROP TRIGGER note below) | metadata |
| `backfill` | `CALL "evolve_P_NN_fill"()` (Q-S6-7 (a), Q-S6-11 (a)): a PL/pgSQL procedure that updates one key range, writes the ledger and commits, then goes to the next range | ROW EXCLUSIVE and row locks per batch | batched |
| `dropColumn` | `DROP COLUMN` | ACCESS EXCLUSIVE | metadata: the column becomes invisible, no rewrite |
| `addNotNullCheck` | `ADD CONSTRAINT ... CHECK (c IS NOT NULL) NOT VALID` | ACCESS EXCLUSIVE | metadata: NOT VALID skips the scan |
| `validate` | `VALIDATE CONSTRAINT` | SHARE UPDATE EXCLUSIVE (does not conflict with ROW EXCLUSIVE or ACCESS SHARE) | scan |
| `setNotNull` | `SET NOT NULL` after a valid CHECK | ACCESS EXCLUSIVE | metadata: "if a valid CHECK constraint exists ... which proves no NULL can exist, then the table scan is skipped" |
| `dropCheck` | `DROP CONSTRAINT` of the CHECK | ACCESS EXCLUSIVE (the ALTER TABLE default) | metadata (Q-S4-6 (a)) |
| `createIndexConcurrently` | `CREATE INDEX CONCURRENTLY` outside a transaction block | SHARE UPDATE EXCLUSIVE | scan; a failure leaves an INVALID index, which the stage drops and builds again |
| `deploy`, `retire` | registry update only | no table lock on user tables (`noLock`) | none (`noScan`) |

The domain gives these rows as `lockOf` and `scanOf` (S4, Q-S4-6 (a)), one
case for each `Step` constructor. `setNotNull` is `scan` when the CHECK of
the column is not `valid` in the source state, else `metadata`.
`createIndexConcurrently` is the 12th `Step` constructor (S6, Q11,
Q-S6-10 (a)): `lockOf` gives `shareUpdateExclusive` and `scanOf` `scan`.

Other confirmed facts: `lock_timeout` aborts a statement that waits longer
than the limit for a lock, and "applies separately to each lock
acquisition attempt". A procedure run by a top-level `CALL` can `COMMIT`
in a loop; a `CALL` with an intervening command cannot. So the backfill
stage file has no outer transaction. The 18.6 ALTER TABLE page allows NOT
VALID for "foreign-key, CHECK, and not-null constraints" (O4).

NOT confirmed in the docs (2026-10-25): a lock request that waits blocks
the later requests behind it. `explicit-locking.html` does not say so. The
plan does not rely on it; `lock_timeout` bounds each wait (O5).

DROP TRIGGER, NOT confirmed in the docs (2026-10-28): the 18
`explicit-locking.html` page does not name DROP TRIGGER, and the page does
not list each command that takes ACCESS EXCLUSIVE. A pgsql-docs thread of
June 2024 ("Re: DROP TRIGGER lock") says that DROP TRIGGER takes ACCESS
EXCLUSIVE and that the docs do not say so. The domain uses ACCESS EXCLUSIVE,
the stronger lock (F-U0-2 (a)). `dropSync` is `metadata`, so J5 does not
refuse it with either lock.

The offline gate compares the emitted files with golden files in
`test/sql/` (S6, Q-S6-13 (a)). The planned `make check-pg` (optional, not
in `make check`) runs the stages against a Docker PostgreSQL with two
client loops that act as `v1` and `v3`. S6 does not have it (Q-S6-14 (a),
O19).

### 5.2 JSON compatibility report

`langc report FILE PROTOCOL` prints one JSON document with stable keys:
`evolve` (format version), `protocol`, `stages` (index, steps, lock, scan),
`apps`, `segments` (stages, live versions), `matrix` (one row per app and
stage: `verdict` is `compatible`, `refused` with `code` and `fix`, or
`runtime` with `fact` and `guard`), and `progress`. The writer is a port of
`hosts/tcc-json/src/json.c` (Q2).

S5 builds the report (U2). The shape is Q-S5-10 (a): `evolve` is 1,
`steps` is an array (`[]` at stage 0), and a segment is
`{"stages":[each index],"live":[the live apps]}`. Each `stages` entry also
has `guards`: the facts of the guest `stageGuard` (Q-S5-4 (a)), each with
the 4.5 sentence (Q-S5-5 (a)). Stage guards include every runtime read,
not just the first fact shown in a matrix row. At the first barrier after
a backfill they also check any pending backfill with no live reader.
Retirement checks account for the effects of a retired app on current
and previously live apps (SPEC.md 6). The matrix has each app at each stage, in
app order and then stage order, with `live` (Q-S5-2 (a)). A pair that is
not live gets the guest `compat` verdict (Q-S5-9 (a)). `progress` is the
`check --json` progress object (Q-S5-6 (a)). The goldens are
`test/report/NAME.lang` and `NAME.json`, and `test/gate.sh` compares each
document byte for byte (Q-S5-7 (a)). For a program that is not in
canonical form, `report` writes the `W_NONCANON` line as text on stderr;
stdout stays one document and the exit is 0 (Q-S5-11 (a)).

### 5.3 Wasm compatibility oracle

`langc wasm PROG PROTOCOL -o OUT` emits a Wasm MVP module for one protocol.
It exports `stages()`, `apps()`, `verdict(app, stage)` (the code as `i32`) and
`guard(stage, facts)` (the facts as a bit mask; the result is the first
failed fact or 0). CI and sandboxed agents with no C toolchain call it. The
kit differential gate compares each export with `langc eval` on the same
inputs. The module holds the computed table; it is NOT the type checker
(Q3). DONE in S7: SPEC.md 6.4 gives the command, the exit codes and the
tests, and SPEC.md 7.1 gives the encoding. A stage with more than 32 guard
facts gives `WASM_LIMIT`; a checked program does not get to this bound.

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

S4 states L1, L4, L5 and L6 (Q-S4-7 (a)). L5 is
`test/eval/chain-laws.lang`: the right unit by `refl`, the left unit and
associativity by `fold`. L1, L4 and L6 are the domain decisions `accepts`,
`dropSafe` and `lockSafe` (`domain/domain.lang`). A program proves a
decision on its protocol by `refl`: `examples/rename.lang` has
`stageSoundProof`, `dropSoundProof` and `lockSoundProof`, and
`test/eval/checker.lang` proves the three decisions on each of the 12 step
forms (36 proofs). `accepts` and `dropSafe` take the live set of each stage
as an argument, as the C driver makes it from the `deploy` and `retire`
steps (Q-S4-16 (a)). S6 states L2 and L3 (Q-S6-12 (a)) in
`test/eval/backfill-laws.lang`, on the rename protocol over a table model
of 10 rows. L2 is the decisions `twiceOnce` and `resumed`, and
`ledgerBisim` (the plan from the ledger point is bisimilar to the tail of
the full plan). L3 is the decisions `writeKeep`, `rowsAfter` and
`readsMove`, and `movesRead` (the `ua` transport for each row). Each is
proved by `refl`. `StreamBisim` is a program definition, the pointwise
form: the CC rule refuses an indexed cofamily. The broken models
`addTwice`, `skipResume`, `writeNoSync` and `lateFill` give `false`.

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

## 8. Port sources (pinned 2026-10-25; S2 rows 2026-10-26)

| Repo | SHA | What |
|---|---|---|
| lang-template | 5265a2a | `hosts/tcc-wasm` (the seed); `hosts/tcc-json/src/json.{c,h}` (the report writer) |
| leaf-lang | e3e2338 | S1 surface: Agda-like Unicode, `langc fmt`, `check --json`; PLAN shape |
| quorum-lang | 7afa479 | S1 surface (second source); PLAN 2; D11 indexed families |
| intent-lang | 67445ae | `REFUSE_CELL`, cofamily; PLAN 2.2; S2c: the CC branch of `REFUSE_CELL`, `Stream` |
| resolve-lang | 510c137 | `REFUSE_CELL` (second implementation) |
| mandate-lang | 17d50a4 | the free cell monad of operations (PLAN 4.6), for `Guard` |
| agent-lang | 06854bf | no JSON diagnostics (leaf-lang S1 read it; `json.c` writes the Json target) |
| dil-lang | 9e57528 | no JSON diagnostics (leaf-lang S1 read it; `--json` is a build output path) |
| leaf-lang | e3e2338 + working-tree snapshot (2026-10-26, patch sha256 be7912102f3ee04f) | S2a: `I`, `PathP`, `Path`, `transp`, `hcomp`, systems; the unreachable Ops removed (leaf-lang S2 units U1 to U4). Frozen: no later leaf-lang state is read (O17) |
| quorum-lang | 8dee55b | S2b: indexed families (D11), higher constructors, `REFUSE_CELL` for CMs (no `record` form, no parameter sections) |
| zkpolicy-lang | 1d263fb | S2e: `Glue`, `glue`, `unglue`, `ua` (`transp_glue`, `hcomp_glue`) |
| quorum-lang | 0c729b6 | S8: the MCP server shape (NDJSON, the lifecycle); `src/front/jsonin.{c,h}` and `utf8_char_size` of `src/front/base.c` (copies, Q-S8-3 (a)) |

## 9. Slices

One session each. Each slice: scope, gate (`make check`, EXIT 0), mutants
on the new checks, docs, close (stage, commit message, next prompt).

| Slice | Scope | Gate and mutants |
|---|---|---|
| S0 | kit seed, `design/DESIGN.md`, this plan, README, `AGENTS.md` | `make check` EXIT 0: examples 10 checked, 0 failed; parse refusals 9; check refusals 19; evals 32; ir 10 lowered; wasm 13 modules valid; differential 813 cases, 0 mismatches; gate 0 failures; review and domain regressions passed |
| S1 | surface: Agda-like Unicode (Q7), `langc fmt`, `W_NONCANON`, `check --json` with progress counts; drop the refused kit names (port from leaf-lang e3e2338) | `make check` EXIT 0: examples 10 checked, 0 failed; parse refusals 9; check refusals 18; evals 30; ir 10 lowered; wasm 13 modules valid; differential 772 cases, 0 mismatches; fmt 50 canonical, 3 non-canonical pairs (round trip and idempotence); json 20 fixtures, diagnostics 31 codes (18 with a fixture, 13 with no fixture); gate 0 failures; review and domain regressions passed; mutants M1 to M6 killed (docs/STATUS.md, S1) |
| S2a | kernel part 1: `I`, `i0`, `i1`, `PathP`, `Path`, `transp`, `hcomp`, systems and partial elements; remove the Ops that no program reaches (part of O14). Port of the frozen leaf-lang snapshot (section 8) | `make check` EXIT 0 with the snapshot counts: examples 10; parse refusals 10; check refusals 24; evals 50; ir 10; wasm 13; differential 773 cases, 0 mismatches; fmt 64 canonical, 5 non-canonical pairs; json 24 fixtures, diagnostics 36 codes (22 with a fixture, 14 with no fixture); diagnostic edit regressions passed; mutants on the `transp` face check, the system agreement check and the `hcomp` base check. Measured 2026-10-26: all counts as above, EXIT 0; M1 (`transp` face), M2 (system agreement), M3 (`hcomp` base) KILLED, 3 of 3 |
| S2b | kernel part 2: indexed families (the `Step : State → State → U` shape, D3), higher constructors, `REFUSE_CELL` for type-parameter `data` (the CM criterion), `Option` (0-cell `none`, generator `some`), `Trunc` (1-cell `squash`). Port from quorum-lang 8dee55b | one check fixture for each refused shape (cons `List`, `Pair`, a type index, a ghost parameter); `Option` and `Trunc` check; `REFUSE_CELL` mutants. Measured 2026-10-26: `make check` EXIT 0: examples 10; parse refusals 10; check refusals 37; evals 53; ir 10; wasm 13; differential 773 cases, 0 mismatches; fmt 81 canonical, 5 non-canonical pairs; json 26 fixtures, diagnostics 38 codes (24 with a fixture, 14 with no fixture); cubical 22 checks, 8 round trips. The builtin `Option` stays; the declared `Maybe` and `Trunc` check (`test/eval/cells.lang`). 14 mutants KILLED of 14 (docs/STATUS.md, S2b); M3 stillborn |
| S2c | kernel part 3: CC (`Stream`: observation `head`, field `tail`), cofamily, the CC branch of `REFUSE_CELL`. Port from intent-lang 67445ae | `Stream` checks; CC refusal fixtures; mutants on the CC criterion. Measured 2026-10-26: `make check` EXIT 0: examples 10; parse refusals 10; check refusals 51; evals 58; ir 10; wasm 13; differential 773 cases, 0 mismatches; fmt 97 canonical, 6 non-canonical pairs; json 26 fixtures, diagnostics 38 codes (24 with a fixture, 14 with no fixture); cubical 32 checks, 8 round trips. `Stream` checks and evaluates (`test/eval/streams.lang`). 11 mutants KILLED of 11 (docs/STATUS.md, S2c); a swap of two checks SURVIVED (the order is not observable) |
| S2d | kernel part 4: `Fin n` as the defined Sigma of 2.2; O12 (kit `Sum`, `Unit`), the rest of O14, O15 (ASCII value printer); `docs/STATUS.md` S2 section | `Fin` tests; full gate; mutants on the `Fin` bound. DONE. Measured 2026-10-27: `make check` EXIT 0: examples 10; parse refusals 10; check refusals 54; evals 62; ir 10; wasm 13; differential 773 cases, 0 mismatches; fmt 101 canonical, 6 non-canonical pairs; json 26 fixtures, diagnostics 38 codes (24 with a fixture, 14 with no fixture); cubical 36 checks, 22 round trips. U1: the value printer (O15, Q-S2d-8 (a)), round trips 8 to 21. U2: `Sum`, `inl`, `inr`, `either` removed (O12, Q-S2d-4 (a)), Ops 51 to 47 (O14), `map` and `bind` on `Option` only. U3: the builtin `Fin` (Q-S2d-1 (a)), the Sigma `(k : ℕ) × Path Bool (natLe (natAdd k 1) n) true`, no new Op. U4: 7 mutants on the `Fin` bound. U5: the printer agrees with `langc fmt` (F-U1b-1, F-U5-1), the check "Fin with a neutral bound" (F-U4-3), the ruled docs. 7 mutants KILLED of 7 (docs/STATUS.md, S2d) |
| S2e | univalence: `Glue`, `glue`, `unglue`, `ua` (2.2, law L3). Port from zkpolicy-lang 1d263fb. It must land before S6; it can run at any time after S2d | `ua` transport test; mutants on `transp_glue` and `hcomp_glue`. DONE. Measured 2026-10-27: `make check` EXIT 0: examples 11; parse refusals 10; check refusals 59; evals 69; ir 11; wasm 14; differential 883 cases, 0 mismatches; fmt 116 canonical, 6 non-canonical pairs; json 30 fixtures, diagnostics 43 codes (28 with a fixture, 15 with no fixture); cubical 49 checks, 24 round trips. U1: the core formers `Glue`, `glue`, `unglue` (Ops 47 to 50), the kernel prelude `src/front/prelude.lang` with `isContr`, `fiber`, `isEquiv`, `Equiv`, `idEquiv`, `ua` (Q-S2e-1 (a), core names), the codes `GLUE_EQUIV`, `GLUE_COVER`, `GLUE_BOUNDARY`, `UNGLUE_TYPE` and `REFUSE_RUNTIME_GLUE`, 5 new check refusals. U2: the Kan rules transp_glue, hcomp_glue, hcomp at `Type 0` (hcomp_univ, with the prelude `transpEquiv`) and Glue eta (Q-S2e-2 (a)). U3: the `ua` transport checks in `test/cubical.c` (Q-S2e-7 (a)), `examples/glue.lang`, the eval fixtures, `test/ir/glue.lang`. U4: 5 mutants (Q-S2e-8 (a)); the rows for M1, M3 and M5 (Q-S2e-13 (a), Q-S2e-14 (c)). U5: the docs. 5 mutants KILLED of 5: M1 transp_glue (cubical "ua fiber center not in normal form"), M2 transp_glue (`examples/glue.lang` `uaBetaSwap`), M3 hcomp_glue (cubical "unglue of an hcomp at a Glue type"), M4 hcomp_glue (`examples/glue.lang` `hcompGlue`), M5 hcomp_univ (`test/eval/hcomp-line` `hcompEAt1`) |
| S3 | domain: `ColType`, `Null`, `CState`, `State`, `App`, the steps, `Step`, `Chain`; the `table`, `app` and `protocol` forms (Q9); `REFUSE_STEP_INDEX`, `REFUSE_RENAME_INPLACE` | one test per step index; mutants on each precondition. DONE. Measured 2026-10-28: `make check` EXIT 0: examples 11; parse refusals 10; check refusals 71; evals 83; ir 11; wasm 15; differential 889 cases, 0 mismatches; fmt 134 canonical, 7 non-canonical pairs; json 32 fixtures, diagnostics 45 codes (30 with a fixture, 15 with no fixture); cubical 50 checks, 29 round trips. U0: the eliminator over indexed families, `fold` with a motive (Op `OP_ELIM_FAMILY`, Ops 50 to 51), `data Walk`. U1: the domain in `domain/domain.lang` (`ColType`, `Null`, `CState`, `State`, `App`, 9 pre and 9 post functions, `Step`, `Chain`, `compose`, `length`, `Stage`), no `nil` row (Q-S3-7 (a)). U2: the `table`, `app` and `protocol` forms (Q-S3-22 to Q-S3-27). U3: J1, the step words and the codes `REFUSE_STEP_INDEX` and `REFUSE_RENAME_INPLACE` (Q-S3-28 to Q-S3-30). U4: one `test/check` file for each pre, `rename-inplace.lang`, `test/eval/steps.lang`. U5: the docs (SPEC.md sections 5 and 6, Q-S3-31 (a); `docs/STATUS.md` S2e and S3, Q-S3-8 (a)). 10 mutants KILLED of 10: M1 to M9 set one pre to `true` (the `test/check/step-*.lang` file of that pre), M10 removes the rename refusal of `check_protocol` (`test/check/rename-inplace.lang`); also M-U0-1 to M-U0-5 KILLED (docs/STATUS.md, S3) |
| S4 | the checker: `compat`, `fresh`, `segments`, J4, J5, the codes of 4.4; `examples/rename` GREEN and the drop-early variant refused (the demo); L1, L4, L5, L6 | one test per code with its JSON shape; mutants on each judgement. DONE. Measured 2026-10-28: `make check` EXIT 0: examples 12; parse refusals 10; check refusals 79; evals 185; ir 12; wasm 16; differential 892 cases, 0 mismatches; fmt 153 canonical, 7 non-canonical pairs; json 38 fixtures, diagnostics 51 codes (36 with a fixture, 15 with no fixture); cubical 52 checks, 29 round trips. U0: the 9th `State` field `link`, `Code`, `Fact` (only `backfillDone`), `Verdict`, `LockMode`, `ScanClass`, `lockOf` and `scanOf` (Q-S4-3, Q-S4-4, Q-S4-6 (a), F-U0-2). U1: J2 (`compat`, J2.1 to J2.4), `fresh`, `segments`, the live set, the ℕ-indexed `Cols` and `Live` (Q-S4-2, Q-S4-8 to Q-S4-12 (a)). U2: J4 and J5 in `check_protocol`, the 6 codes of 4.4 with 6 json fixtures and 7 `test/check/j4-*.lang` files (Q-S4-1, Q-S4-5, Q-S4-13, Q-S4-14 (a)). U3: `examples/rename.lang` GREEN and `test/check/rename-drop-early.lang` refused (`REFUSE_STALE_READ` at stage 5). U4: L5 (`test/eval/chain-laws.lang`); L1, L4 and L6 as the decisions `accepts`, `dropSafe` and `lockSafe`, by `refl` (Q-S4-7, Q-S4-15, Q-S4-16 (a)). U5: the docs (SPEC.md sections 5 and 6; PLAN 2.2, 4.6, 5.1 and 6.1; `docs/STATUS.md` S4; `README.md`). 9 mutants KILLED of 9: M1 to M4 set one J2 clause to `true`, M5 removes the drop-required refusal, M6 changes the scan class of `setNotNull`, M7 makes `addColumn` a barrier, M8 changes the J4 first-code order, M9 changes the `runtime` phase (docs/STATUS.md, S4) |
| S5 | JSON report (port of `json.c`), `Fact`, `Guard`, runtime rows | golden reports; mutants on the verdict map: 6 golden reports (`test/report/`, Q-S5-7 (a)); 11 mutants KILLED of 11: M1 to M3 change one cell verdict, M4 inverts `live`, M5 loses the last guard fact, M6 joins two segments, M7 writes `fix` as `code`, M8 swaps the app and the stage of a matrix row, M9 swaps the `backfillDone` fields, M10 moves `fleetAt` one stage, M11 removes `retired` from `stageGuard` (docs/STATUS.md, S5) |
| S6 | SQL writer: stage files, `lock_timeout`, guards, ledger, backfill procedure, sync triggers, the CHECK path, `createIndexConcurrently` (Q11); L2, L3; `make check-pg` (optional) | golden SQL files; mutants on lock and guard output. DONE. Measured 2026-10-30: `make check` EXIT 0: examples 12; parse refusals 11; check refusals 80; evals 218; ir 12; wasm 16; differential 892 cases, 0 mismatches; fmt 165 canonical, 8 non-canonical pairs; json 38 fixtures, diagnostics 52 codes (36 with a fixture, 16 with no fixture); golden reports 6; sql 9 golden protocols, 53 files; cubical 52 checks, 29 round trips; sql writer regressions 4. U0: the 12th `Step` constructor `createIndexConcurrently`, `preCreateIndex`, `KeyRange` and the `lockTimeout` clause (Q-S6-6, Q-S6-10, Q-S6-12, Q-S6-15 (a)). U1: L2 and L3 in `test/eval/backfill-laws.lang`, 7 `refl` proofs and 4 broken models. U2: `src/sql.c`, `src/sql.h` and the command `langc sql`: `00-setup.sql`, the stage shape and the SQL of the 12 steps, `SQL_NAME_LONG` (Q-S6-1, Q-S6-2, Q-S6-5, Q-S6-11, Q-S6-16 to Q-S6-20 (a)). U3: the tenants, the version registry, the guards, the sync trigger, the backfill procedure, the CHECK path and the INVALID index drop (Q-S6-3, Q-S6-4, Q-S6-7 to Q-S6-9, Q-S6-21, Q-S6-22 (a)). U4: 9 golden protocols with 53 files in `test/sql/` (Q-S6-13, Q-S6-23 to Q-S6-25 (a)). U5: OUTDIR must exist (Q-S6-26 (a)), the mutants and the docs (SPEC.md 6.3; PLAN 5.1, O4, O19; `docs/STATUS.md` S6; `README.md`). No native PostgreSQL server run and no `make check-pg` (Q-S6-14 (a), O19). 8 mutants KILLED of 8: M1 adds 1 to the lock timeout, M2 removes the `fleetAt` DO block, M3 swaps the `backfillDone` columns, M4 moves `fleetAt` one stage, M5 names the wrong app in `retired`, M6 puts a bare stage in a transaction, M7 changes the retry SQLSTATE, M8 lets S win in the sync trigger (docs/STATUS.md, S6) |
| S7 | Wasm oracle writer | differential gate against `langc eval`. DONE. Measured 2026-10-30: `make check` EXIT 0: examples 12; parse refusals 11; check refusals 80; evals 254; ir 12; wasm 16; differential 892 cases, 0 mismatches; oracle 6 golden modules, 175 calls, 0 mismatches; oracle differential 143 cases, 0 mismatches; fmt 173 canonical, 8 non-canonical pairs; json 38 fixtures, diagnostics 53 codes (36 with a fixture, 17 with no fixture); golden reports 6; sql 10 golden protocols, 60 files; cubical 52 checks, 29 round trips; sql writer regressions 4; sql protocol regressions 13. U0: `src/wasmbin.c`, the byte helpers of `src/wasm.c` and `src/oracle.c`; the i32 calls of `test/run-wasm.mjs` (Q-S7-1, Q-S7-2 (a)). U1: the guest folds `verdictCode` and `guardFirst` (Q-S7-3, Q-S7-4 (a)), 36 evals. U2: `src/oracle.c`, `langc wasm` and `WASM_LIMIT` (Q-S7-5 (a)). U3: 6 golden modules `test/wasm/` and the differential on `test/oracle/` (Q-S7-6, Q-S7-7 (a)); `test/oracle-writer.c` checks `WASM_LIMIT` and the cell byte of each code 0 to 9. U4: 8 mutants KILLED of 8 (docs/STATUS.md, S7); the docs (Q-S7-8 (a)) |
| S8 | agent kit: `SKILL.md` with 5 graded examples, `langc mcp`, `explain-code`, final `AGENTS.md` and README table | MCP stdio test; each example passes `check`. DONE. Measured 2026-10-31: `make check` EXIT 0: examples 12; parse refusals 11; check refusals 80; evals 254; ir 12; wasm 16; differential 892 cases, 0 mismatches; oracle 6 golden modules, 175 calls, 0 mismatches; oracle differential 143 cases, 0 mismatches; fmt 173 canonical, 8 non-canonical pairs; json 38 fixtures, diagnostics 53 codes (36 with a fixture, 17 with no fixture); golden reports 6; sql 10 golden protocols, 60 files; mcp 33 requests, 0 mismatches, 5 examples; explain-code 53 codes; list-steps 12 steps; skill 5 examples, 5 outputs match; readme 7 rows, 12 steps named, agents 12 verbs in usage; cubical 52 checks, 29 round trips; sql writer regressions 4; sql protocol regressions 13. U0: the verbs of `src/main.c` write to a FILE* argument; U1: `src/mcp.c` and the JSON reader `src/front/jsonin.c`; U2: `explain-code` (`src/codes.c`) and `list-steps` (`src/steps.c`); U3: `skills/evolve-lang/SKILL.md` and the 5 programs of `examples/agent/`; U4: the README table, AGENTS.md and the docs. the mutants M1 to M10 (M9 in 3 parts) are all KILLED (`docs/STATUS.md` S8). |
| S9 | `docs/TOKEN-BASELINE.md` and its script | the script output equals the file. DONE. Measured 2026-10-31: `make check` EXIT 0: examples 12; parse refusals 11; check refusals 80; evals 254; ir 12; wasm 16; differential 892 cases, 0 mismatches; oracle 6 golden modules, 175 calls, 0 mismatches; oracle differential 143 cases, 0 mismatches; fmt 173 canonical, 8 non-canonical pairs; json 38 fixtures, diagnostics 53 codes (36 with a fixture, 17 with no fixture); golden reports 6; sql 10 golden protocols, 60 files; mcp 33 requests, 0 mismatches, 5 examples; explain-code 53 codes; list-steps 12 steps; skill 5 examples, 5 outputs match; readme 7 rows, 12 steps named, agents 12 verbs in usage; tokens 4 files, 2 rules, 6 ratios, 0 mismatches; cubical 52 checks, 29 round trips; sql writer regressions 4; sql protocol regressions 13. U0: `test/tokcount.c` (`build/tokcount`: the rules A and B, the ratio lines and the check mode; Q-S9-1 (c), Q-S9-2 (a)); U1: the samples of `test/baseline/`, `docs/TOKEN-BASELINE.md` and the gate line (Q-S9-3 to Q-S9-6 (a)); U2: the mutants M1 to M10, all KILLED (`docs/STATUS.md` S9), and the docs (Q-S9-7 (a)). |
| S10 | close: `docs/STATUS.md`, `docs/VALIDATION.md`, full gate, mutant summary | full gate EXIT 0 |

## 10. Open items

- O1 the runner `langc run` (R4: libpq when it lands); integration tests on
  a Docker PostgreSQL.
- O2 libpq, psql and postgres are not installed on this machine
  (2026-10-25); docker is. USER step: `brew install libpq` before O1.
- O3 CLOSED (S8): `langc mcp` implements the MCP protocol revision
  2025-11-25 (Q-S8-1 (a); SPEC.md 6.7). A C MCP server exists in quorum-lang
  0c729b6 (section 8).
- O4 the PG 18 form `NOT NULL ... NOT VALID`. The 18.6 ALTER TABLE page now
  lists not-null constraints for NOT VALID; the docs bug of 18.4
  (`design/DESIGN.md` section 3) looks fixed. A version-gated form (Q5 RULED (a)).
  S6 writes the PostgreSQL 15 path only: `CHECK (c IS NOT NULL) NOT VALID`,
  `VALIDATE`, then `SET NOT NULL` (Q-S6-9 (a)). The PG 18 form stays open.
- O5 lock queue behavior (a waiting ACCESS EXCLUSIVE request blocks later
  readers) is not stated in `explicit-locking.html`. Confirm from another
  PostgreSQL source before a doc cites it.
- O6 the Reddit thread of the description was not found. Prior art for
  multi-tenant rollout: Atlas staged rollouts, Turso.
- O7 CLOSED (S8): Prisma ORM 8 (renamed from Prisma Next) is a release
  candidate, 8.0.0-rc.29 on 2026-10-30
  (https://www.prisma.io/docs/orm/release-status). The README gives this
  status and makes no claim about its features.
- O8 the kit sample domain (`domain/domain.lang`) and its examples stay
  until S3 replaces them. S1 moved them to the S1 surface.
- O9 column types beyond `text` and `bigint` on a USER ask only (Q13).
- O10 to O16 come from S1 (`docs/STATUS.md`, S1, open items): the ASCII
  alias policy, `let`, the kit `Sum` and `Unit`, `flagIf`, dead core paths,
  the ASCII value printer, and `nil` for `Chain` in S3.
- O17 (2026-10-26) evolve-lang does not follow leaf-lang after S2a. S2a
  ports a frozen leaf-lang working tree that the USER did not review.
  Later leaf-lang changes (the rest of its S2, a USER review fix of its
  S2) come to evolve-lang only by a hand port on a USER ask. The S1
  review fix (`W_NONCANON` spans, `test/diagnostics.c`) is in evolve-lang
  and not in leaf-lang.
- O18 (2026-10-26) open items from S2a (`docs/STATUS.md`, S2a): the face
  tests are conservative where an atom of a meet stays free; two systems
  convert by structure, in order; a system has at most `FACE_MEETS_MAX`
  cells; `Eq` is a dropped name (`NAME_UNKNOWN`, hint `Path A x y`). S2a
  removes the `Prod`, `List`, `unfold` and `filter` Ops (part of O14).
  S2d closes the rest of O14.
- O19 (2026-10-30) open items from S6 (`docs/STATUS.md`, S6): S6 has no
  native PostgreSQL server run of the stage files and no `make check-pg`
  (Q-S6-14 (a)); `psql` is not installed (O2). The gate compares the text
  of the files with the goldens. The staged review also executed the
  scripts in PostgreSQL WASM (`docs/STATUS.md`, S6 review). Two doc facts
  are not cited in 5.1:
  a procedure that commits cannot run in a block with an EXCEPTION clause
  (`plpgsql-transactions.html`; the S6 survey planned a U2 check in the 18
  docs, and no unit did it), and `DROP INDEX CONCURRENTLY` cannot run in
  a transaction block (`sql-dropindex.html`, read in S6 U3). Confirm and
  cite them before the first live run.

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

Questions from S2b (2026-10-26). OPEN: not ruled. The S2b build takes
reading (a) of each.

- Q-S2b-1 program data rule (2.1 against the S2b row). OPEN, reading (a).
  (a) `REFUSE_DATA` for a program family with no type argument in its
  params or indices; a family with a type argument runs the CM criterion,
  and a passing CM is admitted in a program. (b) refuse closed data only
  (quorum-lang); this contradicts 2.1. (c) `REFUSE_DATA` for all program
  data, and a C unit test for the criterion.
- Q-S2b-2 the place of the declared `Maybe` and `Trunc`. OPEN, reading
  (a). (a) a program test file in S2b (`test/eval/cells.lang`); S3 decides
  the domain placement. (b) `domain/domain.lang` now; the criterion must
  then run on domain families. S3 (Q-S3-9 (a), USER 2026-10-27): `Maybe`
  and `Trunc` stay in the program test files; the S3 domain uses neither.
- Q-S2b-3 the builtin `Option`. OPEN, reading (a). (a) keep it in S2b, add
  the declared CM twin `Maybe` (a declared `Option` gives `REFUSE_NAME`),
  and remove or keep the builtin in S2d (O12, O14). (b) replace it now
  (`lower.c`, `eval.c`, 3 examples, 2 fixtures, the differential).
- Q-S2b-4 the path constructor surface. OPEN, reading (a). (a) `Path (F A)
  s t`, no new token. (b) the quorum-lang `≡` token (lexer, fmt, grammar).
- Q-S2b-5 dimension. OPEN, reading (a). (a) 1-cells only; a square gets
  `REFUSE_CELL`; 2.2 needs only the 1-cell `squash`. (b) squares (quorum-lang
  `Bag₀`, `trunc₀`).
- Q-S2b-6 the names `List`, `nil`, `cons`. OPEN, reading (a). (a) keep the
  dropped `NAME_UNKNOWN` rows (O16: `nil` returns for `Chain` in S3); the
  hint now says "(S2d)". (b) the quorum-lang `REFUSE_CELL` rows.
- Q-S2b-7 code-equivalent mutants. OPEN, reading (a). Three `REFUSE_CELL`
  branches (cell header, path shape, path dimension) give the same code as
  a later branch. (a) a line of `test/check/expect.txt` can pin a fixed
  part of the message; M2, M7 and M8 are then KILLED. (b) record them as
  code-equivalent.
- Q-S2b-8 rule numbers in messages. OPEN, reading (a). The S2b messages
  use the numbers of section 1: `REFUSE_CELL` is rule R1 and `REFUSE_DATA`
  is rule R2. The kit messages keep the kit numbers: `REFUSE_AXIOM` is rule
  R2, `REFUSE_REC` is rule R3 and `REFUSE_NAME` is rule R4. In section 1,
  R3 is the host and R4 is the targets. (a) keep the messages in S2b (the
  JSON fixtures pin them); a later slice gives the kit rules their own
  names. (b) change the kit messages and their fixtures now.

Questions from S2c (2026-10-26). The USER ruled (a) for all eight on
2026-10-26.

- Q-S2c-1 keyword. RULED (a) (USER 2026-10-26). `codata`, the Agda-like
  twin of `data`. Not `cofamily` (intent-lang and the S2c row).
- Q-S2c-2 place and program rule. RULED (a) (USER 2026-10-26). As Q-S2b-1
  (a) and Q-S2b-2 (a): a program cofamily runs the CC criterion and a
  passing one is admitted; `Stream` is in `test/eval/streams.lang`; S3
  decides the domain place; the CC fixtures are `test/check` files. S3
  (Q-S3-9 (a), USER 2026-10-27): `Stream` stays in
  `test/eval/streams.lang`; the S3 domain does not use it.
- Q-S2c-3 clause surface. RULED (a) (USER 2026-10-26). One clause whose
  name is the corecursor and whose fields are the observation and the
  fields of the cofamily (types `A` and `Stream A`), as intent-lang;
  parse_ctor is reused; the destructors `head` and `tail` get the types
  of 2.2.
- Q-S2c-4 representation. RULED (a) (USER 2026-10-26). The intent-lang
  position encoding (a pos family, `Stream A = pos → A`, a fold walk),
  built as Decl or Core in C with legal evolve-lang names; `eval.c` and
  `lower.c` do not change.
- Q-S2c-5 bisimulation. RULED (a) (USER 2026-10-26). `StreamBisim` goes
  to S3 (the dependent eliminator); evolve-lang has no `elim` and no
  `funExt`. Q-S3-9 (a) (USER 2026-10-27) moves `StreamBisim` to S6, with
  `Stream KeyRange` and L2.
- Q-S2c-6 labelled fields `E → D A` (2.1 admits them; 2.2 does not use
  them). RULED (a) (USER 2026-10-26). Port them with the closed-label rule
  and the label universe rule.
- Q-S2c-7 message text. RULED (a) (USER 2026-10-26). Keep "is not a cocell
  signature" and "(rule R1)" (as Q-S2b-8 (a)); say "field" for "cofield"
  (2.2); no and-block branch (evolve-lang has no `and` blocks).
- Q-S2c-8 destructor arity. RULED (a) (USER 2026-10-26). An explicit type
  argument, `head ℕ s`, as `just ℕ 5` and `squash A u v` in S2b.
- F-U4-1 shared `REFUSE_CELL` texts (S2c U4). OPEN, reading (a). The 13
  survey branches come from 7 FAIL sites of cocell_shape, so some
  branches give the same text. (a) keep the texts (Q-S2c-7 (a)). (b)
  split the texts by branch.
