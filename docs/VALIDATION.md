# Validation

Date: 2026-10-31. tcc 0.9.28rc 2026-09-04 mob@0fb54300 builds the host and
the review tools. Apple clang 21.0.0 (clang-2100.0.123.102) runs
`check-clang`. Host executable: `build/langc` at dd98209, with the S10
working tree.

`make -B check` passes. It builds the host and the review tools with tcc,
compiles the sources again with clang (`-Wall -Wextra -Wswitch-enum
-Werror`, `check-clang`), and runs `test/gate.sh`, `test/review.sh`,
`build/fronttool`, `build/diagnostics`, `build/cubical` and
`build/sql-writer`. The count lines are `examples: 12 checked, 0 failed`,
`parse refusals: 11 checked`, `check refusals: 80 checked`, `evals: 254
checked`, `ir: 12 lowered`, `wasm: 16 modules valid`, `differential: 892
cases, 0 mismatches`, `oracle: 6 golden modules, 175 calls, 0
mismatches`, `oracle differential: 143 cases, 0 mismatches`, `fmt: 173
canonical, 8 non-canonical pairs`, `json: 38 fixtures; diagnostics: 53
codes, 36 with a fixture, 17 with no fixture`, `report: 6 golden
reports`, `sql: 10 golden protocols, 60 files`, `mcp: 33 requests, 0
mismatches; 5 examples`, `explain-code: 53 codes match
docs/DIAGNOSTICS.md`, `list-steps: 12 steps match domain/domain.lang`,
`skill: 5 examples, 5 outputs match`, `readme: 7 rows, 12 steps named;
agents: 12 verbs in usage`, `tokens: 4 files, 2 rules, 6 ratios, 0
mismatches`, `tokens IO regressions: 5 cases, 0 failures`, `validation: 6
laws, 5 judgements, 11 rows, 0 mismatches; 4 bad fixtures refused`,
`validation IO regressions: 8 cases, 0 failures`,
`gate: 0 failures`, `review regressions: passed`, `domain regressions:
passed`, `diagnostic edit regressions: passed`, `cubical regressions: 52
checks, 29 round trips passed`, `json writer regressions: 21 cases
passed`, `report guard regressions: 4 cases passed`, `sql writer
regressions: 4 cases passed` and `sql protocol regressions: 13 cases
passed`.

The gate runs 1499 tests with zero failures. N is the sum of the first
number of each count line that counts cases (Q-S10-5 (a)). A count line
counts cases when the word after its first number is `checked`, `checks`
or `cases`. These 13 lines count cases: examples 12, parse refusals 11,
check refusals 80, evals 254, differential 892, oracle differential 143,
tokens IO regressions 5, validation IO regressions 8, cubical regressions
52, json writer regressions 21, report guard regressions 4, sql writer
regressions 4 and sql protocol
regressions 13. The S1 close has 839 of these cases, and the slices S2a
to S9 add 652 (see Slices). The S10 CLI regressions add 8. The review gate
took 537 seconds of wall time.

The other count lines are not in N. Their first number counts modules,
files, requests, codes or list items, not cases: `ir` (the modules that
`langc ir` lowers), `wasm` (the valid modules), `oracle` (the golden
modules; the 175 calls run in them), `fmt` (the canonical files and the
non-canonical pairs), `json` and `diagnostics` (the fixtures and the
codes), `report`, `sql` and `mcp` (the golden files and the requests),
`explain-code`, `list-steps`, `skill`, `readme` and `agents` (the items
that must match a doc or a source file), `tokens` (the files, the rules
and the ratios) and `validation` (the law and judgement rows). `gate: 0
failures` counts failures. The 29 round trips of the cubical line are not
in N. Three lines have no number: `review regressions: passed`, `domain
regressions: passed` and `diagnostic edit regressions: passed`.

## Differential and oracle checks

`test/gate.sh` builds a module with `langc build` for each of the 12
examples and the 4 `test/ir` programs (`spine`, `lazy`, `proof-trap`,
`walk`). `wasm-validate` and `wasm-objdump -x` check each module. The
differential makes a call grid from `langc ir` with `test/grid.awk`. It
runs each call through `langc eval` and through the module with `node
test/run-wasm.mjs`, and compares the two results. A trap must give a
trap. A `pick 2 4 9` call on `entries.wasm` must trap.

`build/oracle-writer` checks the `WASM_LIMIT` bound and the cell byte of
each code 0 to 9 on reports that the test makes by hand. For each
`test/report/NAME.json`, the module that `langc wasm` writes must be byte
equal to `test/wasm/NAME.wasm` and pass `wasm-validate`. From the report,
the gate makes the calls `stages`, `apps`, `verdict A I`, `guard I 0` and
`guard I 4294967295`, and 3 calls that must trap. It runs them with `node
test/run-wasm.mjs`. The oracle differential builds each
`test/oracle/NAME.lang` to the same module. It runs each `verdict` call,
and each `guard` call with a mask below 2^min(k, 6) for a stage with k
facts, through `langc eval` and through the module, and compares the
results.

## Token baseline

`docs/TOKEN-BASELINE.md` (S9) gives the bytes, the code points and the
tokens of the PLAN 4.6 rename in 4 files: `examples/rename.lang`,
`test/baseline/rename-ascii.lang`, `test/baseline/rename.py` and
`test/baseline/rename.rs`. Rule A (from leaf-lang) counts a run of ASCII
letters, digits and `_` as 1 token. Rule B (from quorum-lang) counts a run
of length L as (L+3)/4 tokens. In both rules, each other ASCII byte that
is not white space is 1 token, and each byte from 0x80 is 1 token. The
ratios against `examples/rename.lang` are half up to 2 decimals: the ASCII
file 0.96 (A) and 0.97 (B), Python 4.36 and 4.08, Rust 6.79 and 6.09. The
gate runs `build/tokcount --check docs/TOKEN-BASELINE.md` on the 4 files.
It compares the table with the region between the tokcount markers,
checks the half-up rule, runs `langc check` on the 2 `.lang` files, and
checks that `langc fmt` of the ASCII file gives `examples/rename.lang`.
`build/tokcount-io` then runs its 5 CLI cases.

## Slices

There is one paragraph for each slice row of PLAN: S1, S2a to S2e, and
S3 to S9. The counts come from docs/STATUS.md.

The S1 review (2026-10-25) ports the leaf-lang S1 diff: the Agda-like
surface, the canonical printer `langc fmt` and `langc check --json`. It
adds no counted case. It adds 3 non-canonical pairs in `test/fmt/`, 20
JSON fixtures in `test/json/`, a parse round trip through
`build/fronttool dump`, a canonical and idempotent `fmt` check of each
committed `.lang` file, and a check of the code list of
`docs/DIAGNOSTICS.md` against `src/`. They check the UTF-8 symbols and
their ASCII aliases, the signature and clause form, `rec`, `postulate`,
`data ... where` and the layout rule. Refusals cover `PARSE_CLAUSE_NAME`,
`PARSE_CTOR_RESULT`, `W_NONCANON` with a `fix`, and `NAME_UNKNOWN` for a
dropped kit name. S1 removes 1 check refusal, 1 fronttool regression and
a net 2 evals that used a dropped kit name. At the S1 close, the gate
counts 10 examples, 9 parse refusals, 18 check refusals, 30 evals and 772
differential cases. Mutants M1 to M6 are KILLED.

The S2a review adds 45 cases: 1 parse refusal, 6 check refusals, 20
evals, 1 differential case and 17 checks in `test/cubical.c`. They check
the interval `I` (`i0`, `i1`, `~`, `∧`, `∨`), `PathP` and `Path`,
`transp`, `hcomp`, `Partial` and systems. `test/cubical.c` also runs 8
round trips of normalized source. Refusals cover a `transp` that is not
constant on its face (`transp-face.lang`), a system with faces that do
not agree (`system-disagree.lang`) and a bad `hcomp` base face
(`hcomp-base.lang`). Mutants M1 to M3 are KILLED, each by one of these 3
files.

The S2b review adds 21 cases: 13 check refusals, 3 evals and 5 cubical
checks. They check indexed families (the `Step` shape), path
constructors of dimension 1 and the CM criterion for program data with a
type argument. `test/eval/cells.lang` declares the CM twin `Maybe` and
`Trunc` with the 1-cell `squash`. Refusals cover `REFUSE_CELL` (rule R1)
in the `test/check/cell-*.lang` files, `REFUSE_DATA` (`data.lang`) and
`DOMAIN_FAMILY` for a family or a fold outside the S2 data rules
(`cell-field.lang`, `index-fold.lang`, `hit-fold.lang`). A line of
`test/check/expect.txt` can pin a text after the code. Of 15 mutants, 14
build and are KILLED. M3 is STILLBORN: clang `-Werror` stops the build.

The S2c review adds 29 cases: 14 check refusals, 5 evals and 10 cubical
checks. They check `codata` with one clause, the corecursor, the
destructors `head` and `tail`, labelled fields and the CC branch of
`REFUSE_CELL`. `test/eval/streams.lang` declares `Stream`, and
`test/fmt/codata.lang` gives its canonical form. Refusals cover the 14
`test/check/codata-*.lang` files: two observations or none, a bad type
parameter, a parameter that is not a universe, an index, an open label, a
duplicate field and a bad clause result. Of 13 mutants, 12 are KILLED and
S1 SURVIVES. Each fixture has one top-level fault, so the order of the
parameter check and the index check is not observable.

The S2d review adds 11 cases: 3 check refusals, 4 evals and 4 cubical
checks. The cubical round trips go from 8 to 22. They check `Fin n` as
the Sigma `(k : ℕ) × Path Bool (natLe (natAdd k 1) n) true`
(`test/eval/fin.lang` and the cubical checks of `Fin`). `Sum`, `inl`,
`inr` and `either` leave the core (O12), and 4 Op values leave (O14): a
program reaches 46 of the 47 Op values (the S2d Op table in
`docs/STATUS.md`). The value printer follows `fmt` for 3 binder forms
(O15). Refusals cover `pack 3 refl : Fin 3` (`fin-bound.lang`) and
`pack 0 refl : Fin 0` (`fin-empty.lang`) with `TYPE_REFL`. Mutants F1 to
F7 of `rule_fin` are KILLED. F7 lived first, and the cubical check "Fin
with a neutral bound" kills it.

The S2e review adds 136 cases: 1 example, 5 check refusals, 7 evals, 110
differential cases and 13 cubical checks. They check `Glue`, `glue`,
`unglue`, the kernel prelude `src/front/prelude.lang` (`isContr`,
`fiber`, `isEquiv`, `Equiv`, `idEquiv`, `ua`, `transpEquiv`) and the Kan
rules transp_glue, hcomp_glue, hcomp_univ and Glue eta
(`examples/glue.lang`, `test/eval/hcomp-glue.lang`,
`test/eval/hcomp-line.lang`). A transport along `ua` applies the
equivalence. Refusals cover `GLUE_EQUIV`, `GLUE_COVER`, `GLUE_BOUNDARY`,
`UNGLUE_TYPE` and, in `langc ir`, `REFUSE_RUNTIME_GLUE`. Mutants M1 to M5
of the Kan rules are KILLED. M1, M3 and M5 lived first, and a new test
kills each one.

The S3 review adds 33 cases: 12 check refusals, 14 evals, 6 differential
cases and 1 cubical check. They check the evolve domain in
`domain/domain.lang` (`State n m`, the 11 `Step` constructors, `Chain`,
`compose`, `Stage`), the forms `table`, `app` and `protocol`
(`test/fmt/forms`), the dependent eliminator over indexed families
(`test/eval/index-fold.lang`, `test/ir/walk.lang`, 3
`test/check/fold-*.lang` files) and the steps (`test/eval/steps.lang`).
Refusals cover `REFUSE_STEP_INDEX` for a false pre of each step (9
`test/check/step-*.lang` files) and `REFUSE_RENAME_INPLACE`
(`rename-inplace.lang`). The 15 mutants M1 to M10 and M-U0-1 to M-U0-5
are KILLED. Each of M1 to M9 sets one pre to `true`.

The S4 review adds 116 cases: 1 example, 8 check refusals, 102 evals, 3
differential cases and 2 cubical checks. They check the compatibility
checker: J2 `compat` (J2.1 to J2.4), the live set of each stage, `lockOf`
and `scanOf` for the 11 steps, and J1, the drop check, J5 and J4 for each
protocol step. `test/eval/checker.lang` has 33 proofs over the 11 step
forms for L1, L4 and L6 (`accepts`, `dropSafe`, `lockSafe`).
`test/eval/compat.lang` gives the PLAN 4.6 table, and
`test/eval/chain-laws.lang` gives L5. `examples/rename.lang` is the demo.
Refusals cover 6 of the 7 `test/check/j4-*.lang` files and their 6 json
fixtures (`REFUSE_MISSING_COLUMN`, `REFUSE_STALE_READ`,
`REFUSE_UNFILLED_READ`, `REFUSE_NULL_INSERT`, `REFUSE_DROP_REQUIRED`,
`REFUSE_LOCK_SCAN`), `rename-drop-early.lang` and the review case
`j4-readd-unfilled.lang`. The 9 mutants M1 to M9 are KILLED.

The S5 review adds 42 cases: 17 evals, 21 json writer regressions and 4
report guard regressions in `test/cubical.c`. They check `Guard n m A` and
`stageGuard` (`test/eval/guard.lang`), the facts `retired` and `fleetAt`,
and `langc report FILE PROTOCOL`. The gate compares the report of each
`test/report/NAME.lang` (bare, fleet, order, single, strict) and of
`examples/rename.lang` with its golden `.json` file, byte for byte. S5
adds no refusal: the check refusals stay at 79. The 4 report guard
regressions fail on the code before the review fixes (`runtimeReads`,
`retiredGuards`, `barrierFills`). The golden reports kill the 11 mutants
M1 to M11.

The S6 review adds 35 cases: 1 parse refusal, 1 check refusal, 16 evals,
4 sql writer regressions in `test/cubical.c` and 13 sql protocol
regressions in `test/sql-writer.c`. They check `langc sql PROG PROTOCOL
OUTDIR`: `00-setup.sql` and one `NN-WORD.sql` file for each stage, the
progress ledger of each tenant, `stageGuard` as SQL guards, a
`lock_timeout` for each stage and the `evolve.ddl` retry on SQLSTATE
55P03. The step `createIndexConcurrently` (`step-create-index.lang`) and
the clause `lockTimeout` are new. `test/eval/backfill-laws.lang` gives L2
(`twiceOnce`, `resumed`) and L3 (`writeKeep`, `rowsAfter`, `movesRead`,
`readsMove`). The gate compares the 60 files of the 10 golden protocols
in `test/sql/`, byte for byte. Refusals cover a `lockTimeout` outside 1
to 2147483647 (`test/parse/lock-timeout-zero.lang`, `PARSE_EXPECT`) and
the new code `SQL_NAME_LONG`. The golden files kill the 8 mutants M1 to
M8 of `src/sql.c`.

The S7 review adds 179 cases: 36 evals in `test/eval/oracle.lang` and 143
oracle differential cases. They check `langc wasm PROG PROTOCOL -o OUT`:
a Wasm 1.0 module that holds the compatibility table and exports
`stages`, `apps`, `verdict` and `guard`. The guest folds `verdictCode`,
`halfOdd`, `guardWalk` and `guardFirst` are new in `domain/domain.lang`.
The 6 modules must be byte equal to `test/wasm/NAME.wasm`, and their 175
calls must agree with `test/report/NAME.json`. The 16 `langc build`
modules stay byte equal. Refusals cover `WASM_LIMIT` in
`test/oracle-writer.c` only, because a checked program does not reach
it. A call out of range must trap. Call mismatches kill the 8 mutants M1
to M8, and `build/oracle-writer` also kills M1 and M2.

The S8 review adds no counted case. It adds `langc mcp` (an MCP server,
revision 2025-11-25, 5 tools), `langc explain-code`, `langc list-steps`,
`skills/evolve-lang/SKILL.md` with 5 graded examples in
`examples/agent/`, the README Steps column and `AGENTS.md`.
`build/mcp-test` sends the 33 requests of `test/mcp/requests.jsonl` and
compares the replies with `test/mcp/responses.jsonl`. `build/verbs-test`
compares `src/codes.c` with `docs/DIAGNOSTICS.md` and `src/steps.c` with
`domain/domain.lang`. `build/skill-test` compares each program block of
SKILL.md with its file, and each json block with `langc check --json`.
`build/readme-test` checks the README table and the verbs of `AGENTS.md`.
Refusals cover the examples 3 (a column dropped too early) and 5 (a
stale read), a line of 1048576 bytes (-32700) and `isError`. The 12
mutants M1 to M8, M9a, M9b, M9c and M10 are KILLED. Only a copy kills
M10 (see the list below).

The S9 review adds 5 cases: the 5 CLI cases of `test/tokcount-io.c`.
They check `build/tokcount` (`test/tokcount.c`) on the 4 files of the
token baseline. The CLI cases check a valid input, an unwritable stdout in
the two modes and a document with NUL bytes. S9 adds no `langc` refusal.
The 10 mutants M1 to M10 are KILLED. Only a copy kills M7 (see the list
below).

The slices S2a to S9 ran 115 mutants: 114 are KILLED and 1 SURVIVED (S2c
S1). S2b M3 is STILLBORN and is not in the 115. S1 also kills M1 to M6,
which docs/STATUS.md gives in prose only. S10 runs 6 mutants of the
law-id check, and all 6 are KILLED.

The S10 review adds 8 CLI cases in `test/validation-io.c`: a valid map,
blank and indented rows, an unknown row after a blank line, an indented
duplicate row, a fourth cell without a closing pipe, NUL bytes in each
input, and an unwritable stdout. They check that the whole laws region
and every cell is read, and that NUL bytes or output errors give exit 2.

## What the gate does not check

- PLAN 6.2 (outside the denotation): the time that a step waits for a
  lock, traffic and the load that a batch adds, long transactions that
  hold locks, replication lag, the cost of triggers, disk space, whether
  the application code agrees with its declared requirements, the server
  version, and an operator who runs the stage files out of order. The
  guards of PLAN 4.5 check some of these at run time. evolve-lang does
  not claim zero downtime (D11).
- O19: no native PostgreSQL server runs the stage files, and there is no
  `make check-pg` (Q-S6-14 (a)). `psql` is not installed (O2). PLAN 5.1
  does not cite two PostgreSQL facts: a procedure that commits cannot
  run in a block with an EXCEPTION clause, and `DROP INDEX CONCURRENTLY`
  cannot run in a transaction block. The S6 review ran the 10 golden
  protocols in PGlite 0.5.13 by hand, outside the gate.
- No Python or Rust run: the gate counts the tokens of `rename.py` and
  `rename.rs`, but it does not run them. The S9 hand run (Q-S9-5 (a))
  ran `python3 test/baseline/rename.py` (exit 0, no output) and `rustc
  --edition 2021 --test test/baseline/rename.rs` (6 tests pass), with
  python3 3.15.0 and rustc 1.96.0-nightly.
- S8 M10 and S9 M7 change a gate check, not `langc`. The committed inputs
  do not reach the changed branch, so only a copy kills them. S8 M10
  (`find_name` of `test/readme.c` gives row 0 for an unknown name) needs
  a README copy with `deplo` in row 2. S9 M7 (`check_lang` skips the
  `fmt` compare) needs a copy of the ASCII file with one comment word
  changed, and a TOKEN-BASELINE copy whose region uses that copy.
- The statements of the law map: `build/validation-test` checks the ids
  and the paths, not the text.

## Laws and judgements

This map gives each law of PLAN 6.1 and each judgement of PLAN 4.2, and the
tests and the gate blocks that check it. The gate checks the ids of the map
(Q-S10-2 (c), Q-S10-9 (a)). `build/validation-test` reads the L items of
PLAN 6.1, the J items of PLAN 4.2 and the rows between the laws markers.
Each id must have exactly one row, each row id must be a PLAN id, and each
path in a tests cell must exist. The gate does not check the statements.
Four bad fixtures in `test/validation/` show that the check refuses a
missing id, an extra id, a duplicate id and a missing path.
`build/validation-io` also checks whitespace between rows, trailing cells,
NUL bytes in either input, and output errors.

<!-- laws begin -->
| id | statement | tests |
|---|---|---|
| L1 | Stage soundness: when `check` accepts a protocol, `compat` holds or is a runtime verdict for each segment, each live version and each stage of the segment. | `domain/domain.lang` (`accepts`), `examples/rename.lang` (`stageSoundProof`), `test/eval/checker.lang`, `test/eval/compat.lang` (`acceptsV1Live`); gate: `examples`, `evals` |
| L2 | Backfill resume: two runs of a batch give the same table as one run, and a run from the ledger point gives the same table as a run with no stop. | `test/eval/backfill-laws.lang` (`twiceOnce`, `resumed`, `ledgerBisim`, `addTwice`, `skipResume`); gate: `evals` |
| L3 | Write-path consistency: while the sync trigger is installed, each write keeps `display_name = name` on the rows that it touches. After `backfillDone` this holds for each row, and `ua` moves the reads of `v1` to the reads of `v3`. | `test/eval/backfill-laws.lang` (`writeKeep`, `rowsAfter`, `readsMove`, `movesRead`, `writeNoSync`, `lateFill`); gate: `evals` |
| L4 | Drop safety: no version that is live at or after a `dropColumn t c` stage reads, writes or inserts `c`. | `domain/domain.lang` (`dropSafe`), `examples/rename.lang` (`dropSoundProof`), `test/eval/checker.lang`, `test/eval/compat.lang` (`dropSafeV1Live`); gate: `examples`, `evals` |
| L5 | Chain laws: `_⨾_` is associative and `nil` is its unit (`Path`). | `test/eval/chain-laws.lang`; gate: `evals` |
| L6 | Lock map: no admitted step has the scan class `scan` under ACCESS EXCLUSIVE. | `domain/domain.lang` (`lockSafe`), `examples/rename.lang` (`lockSoundProof`), `test/eval/checker.lang` (`lockSafeSetNotNullNoCheck`); gate: `examples`, `evals` |
| J1 | Step typing `s ⊢ step : s'`: a step that fails its index gets `REFUSE_STEP_INDEX`. | `test/check/step-add-column.lang`, `test/check/step-add-not-null-check.lang`, `test/check/step-backfill.lang`, `test/check/step-create-index.lang`, `test/check/step-drop-check.lang`, `test/check/step-drop-column.lang`, `test/check/step-drop-sync.lang`, `test/check/step-install-sync.lang`, `test/check/step-set-not-null.lang`, `test/check/step-validate.lang`, `test/check/rename-inplace.lang`, `test/eval/steps.lang`, `test/json/REFUSE_STEP_INDEX.lang`, `test/json/REFUSE_RENAME_INPLACE.lang`; gate: `check refusals`, `evals`, `json` |
| J2 | Compatibility `compat(a, s)` is decidable. The clauses J2.1 to J2.4: each column is present, each read is fresh, each read after a reader switch is filled, and each insert gives a value to each strict column. | `domain/domain.lang` (`compat`), `test/eval/compat.lang` (the PLAN 4.6 table), `test/check/j4-missing-column.lang` (J2.1), `test/check/j4-stale-read.lang` (J2.2), `test/check/j4-unfilled-read.lang` (J2.3), `test/check/j4-readd-unfilled.lang` (J2.3), `test/eval/backfill-reuse.lang` (J2.3), `test/check/j4-null-insert.lang` (J2.4); gate: `check refusals`, `evals` |
| J3 | Segments: the barriers cut the chain into segments. The fleet passes a barrier only when each tenant has run each step before it. | `domain/domain.lang` (`segments`, `isBarrier`), `test/eval/compat.lang` (`segmentAt3`, `segmentAt4`, `segmentAt5`), `test/report/fleet.lang` (the `fleetAt` guard, S5); gate: `evals`, `report` |
| J4 | Protocol acceptance: the checker accepts the protocol when J1 holds for each step and `compat` holds or depends only on runtime facts for each version and each stage of each segment. Else it refuses with the first code of PLAN 4.4 in chain order. | `examples/rename.lang`, `test/check/j4-drop-required.lang`, `test/check/rename-drop-early.lang`, `test/cubical.c` (one and more live apps), `test/json/REFUSE_MISSING_COLUMN.lang`, `test/json/REFUSE_STALE_READ.lang`, `test/json/REFUSE_UNFILLED_READ.lang`, `test/json/REFUSE_NULL_INSERT.lang`, `test/json/REFUSE_DROP_REQUIRED.lang`, `test/json/REFUSE_LOCK_SCAN.lang`; gate: `examples`, `check refusals`, `json`, `cubical regressions` |
| J5 | Lock class: each step has a lock mode and a scan class. The checker refuses a step that scans a table under ACCESS EXCLUSIVE (`REFUSE_LOCK_SCAN`). | `domain/domain.lang` (`lockOf`, `scanOf`), `test/check/j4-lock-scan.lang`, `test/eval/checker.lang` (`scanSetNotNullNoCheck`, `lockSafeSetNotNullNoCheck`), `test/json/REFUSE_LOCK_SCAN.lang`; gate: `check refusals`, `evals`, `json` |
<!-- laws end -->
