# evolve-lang status

> Write here: one paragraph. Say which milestone the build implements, and
> which milestone it does not complete yet.

## Implemented

> Write here: one item for each feature that works. Name the former (F1 to
> F15), the core type or the core operation.

- <feature>.

## Remaining M0 work

> Write here: one numbered item for each missing feature. Say what each item
> needs first.

1. <work item>.

## Known limits

> Write here: one item for each limit of the current build, with its cause
> (a fuel budget, a host limit, a stack size).

- <limit>.

## Internal boundaries

> Write here: one paragraph for each module group. Say which module holds
> which job, which tests check which tables, and which types a program
> cannot use.

## S1: canonical surface, `langc fmt`, JSON diagnostics

S1 (2026-10-25) gives the Agda-like surface (Q7 RULED (a)) of SPEC.md
section 2, the canonical printer `langc fmt` and `langc check --json`
(docs/DIAGNOSTICS.md). S1 is a port: the evolve-lang seed and the
leaf-lang seed 3802170 are the same kit (lang-template 5265a2a), so S1
applies the leaf-lang S1 diff (3802170 to e3e2338) on the kit paths. The
language name changes on 8 lines. test/gate.sh has no dash exemption,
because design/DESIGN.md has no dash.

- Lexer: UTF-8 symbols `→`, `λ`, `×`, `ℕ` and their ASCII aliases; columns
  in code points; a comment list for `fmt`.
- Parser: signature and clause, `rec`, `postulate`, `data ... where`, the
  layout rule; new codes `PARSE_CLAUSE_NAME` and `PARSE_CTOR_RESULT`.
- `langc fmt` (src/front/fmt.c) and `W_NONCANON` with a `fix`.
- Kit names: `Flag`, `flagYes`, `flagNo` became `Bool`, `true`, `false` in
  the core. `Prod`, `pair`, `first`, `second`, `List`, `nil`, `cons`,
  `filter` and `unfold` left the builtin table. `NAME_UNKNOWN` names the
  replacement. The reserved names are only `Text`.
- Gate additions (test/gate.sh): canonical and idempotent `fmt` on each
  committed `.lang`, 3 non-canonical pairs in test/fmt/, a parse round trip
  through `build/fronttool dump`, 20 JSON fixtures in test/json/, and the
  code list of docs/DIAGNOSTICS.md against src/. Mutants M1 to M6 are
  killed.

Gate counts (`make check`): examples 10 checked, 0 failed; parse refusals
9; check refusals 18; evals 30; ir 10 lowered; wasm 13 modules valid;
differential 772 cases, 0 mismatches; fmt 50 canonical, 3 non-canonical
pairs; json 20 fixtures; diagnostics 31 codes, 18 with a fixture, 13 with
no fixture; gate 0 failures; review and domain regressions passed.

### Removed cases

Each case below used a kit name that S1 removed. Supported behavior stays
covered: `stackDemand` consumes a trapped recursive rest, and the scalar
inventory entries check all three Item fields and both `isRed` results.
The native C regressions also check JSON encoding of seven malformed
UTF-8 sequences and three valid Unicode scalars.

| File | Case | Reason |
|---|---|---|
| test/check/filter-sum.lang | `TYPE_MISMATCH` from `filter` on `Sum Flag Nat` | `filter` has no replacement. Check refusals 19 to 18. |
| test/eval/expect.txt | `formers xs`, `inventory stockTotal`, `inventory stockRed` | They use `List`. |
| test/eval/expect.txt | `algebra down`, `algebra stop` | They use `List` and `unfold`. Three scalar inventory checks restore projection and enum coverage. Eval lines 32 to 30. |
| test/gate.sh (spine list) | `foldOnes 3:6`, `mapUp 3:18`, `bindUp 3:36`, `filterUp 5:3`, `trapHead 5:1`, `trapSum 5:trap`, `trapSum 2:3`, `filterTrap 2:3`, `filterTrap 5:trap`, `bindTrap 5:1` | They use `List` and `filter`. Two `stackDemand` cases restore demanded-rest trap coverage. Spine cases 13 to 5; differential 813 to 772 cases. |
| test/fronttool.c | the `Prod` eta regression | `Prod` is removed. The Sigma eta regression stays. Regressions 14 to 13. |
| test/fronttool.c | the `first (pair ...)` wrapper of the `Lot` case | `witness (pack ...)` gives `TYPE_INFER`. The case is now `bad = either f f s`, with the same code. |

These definitions left the `.lang` files for the same reason: domain
`total`, `redTotal`; algebra `sumList`, `ones`, `down`, `stop`; formers
`xs`, `empty`; inventory `stock`, `stockTotal`, `stockRed`; laws `xs`,
`noNats`, `mapId`, `foldNil`, `foldSum`; monad `single`, `doubled`,
`small`, `kept`; test/ir/spine.lang `noNats`, `upFrom`, `upTrap`,
`foldOnes`, `mapUp`, `bindUp`, `filterUp`, `trapHead`, `trapSum`,
`filterTrap`, `bindTrap`.

The kit sample domain and its examples stay until S3 (PLAN O8). S1 moves
them to the S1 surface only.

### Remaining names against PLAN 2.2, 3.2 and 3.3

| Name | PLAN | S1 state |
|---|---|---|
| `Bool`, `true`, `false`, `ℕ`, `natAdd`, `natSub`, `natMul`, `natEq`, `natLe` | 3.2 base (kit) | admitted |
| `Option`, `none`, `some`, `option`, `pure`, `map`, `bind` | 2.2 CM | admitted |
| `pack`, `witness`, `payload` | 2.2 ambient Sigma | admitted |
| `Fin` | 2.2 defined Sigma `Fin n` | S2d added it (Q-S2d-1 (a)): a core name for `(k : ℕ) × Path Bool (natLe (natAdd k 1) n) true`, no new Op |
| `Glue`, `glue`, `unglue` | 2.2 ambient `Glue` | S2e added them: core formers, 3 new Ops (`OP_GLUE`, `OP_GLUE_ELEM`, `OP_UNGLUE`); the Kan rules transp_glue, hcomp_glue, hcomp_univ and Glue eta (Q-S2e-2 (a)); `langc ir` gives `REFUSE_RUNTIME_GLUE` |
| `isContr`, `fiber`, `isEquiv`, `Equiv`, `idEquiv`, `ua`, `transpEquiv` | 2.2 ambient `ua` (the others are its helpers) | S2e added them (Q-S2e-1 (a), Q-S2e-5 (a)): the kernel prelude `src/front/prelude.lang`; core names (`REFUSE_NAME`) |
| `fold` | F6 (the kit recursion) | admitted |
| `Eq`, `refl`, `symm`, `trans`, `cong`, `transport` | 2.2 `Path` | S2 renames `Eq` to `Path` |
| `Sum`, `inl`, `inr`, `either` | not in 2.2 (`Either E A` is not admitted) | S2d removed them (Q-S2d-4 (a)): `NAME_UNKNOWN`, hint "use Option or a base sum" |
| `Unit`, `unit` | PLAN 2.1 ambient | stays (Q-S2d-5 (a)) |
| `flagIf` | kit name | stays (O13) |
| `Text` | 3.3 strings refused | reserved |
| `nil` | 3.2 `Chain` constructor (S3) | refused as a dropped kit name; S3 admits it again (O16) |

### Port sources read

- leaf-lang e3e2338: the S1 diff on the kit paths, and its docs/STATUS.md
  S1 section (the removed cases above).
- leaf-lang S1 found no JSON diagnostic code in agent-lang 06854bf or
  dil-lang 9e57528 (`json.c` of agent-lang writes the Json target; dil-lang
  `--json OUT.json` is a build output path). Thus the JSON record and
  `retry` come from leaf-lang.

### Open items from S1

- O10 (reading, under Q7 RULED (a)): an ASCII alias parses and gives
  `W_NONCANON` with the Unicode form as `fix`; `fmt` prints only the
  Unicode form. Unicode can cost more tokens; the S9 baseline (Q12)
  measures it.
- O11: a `let` form. Open.
- O12: the kit `Sum` and `Unit` formers stay in S1. `Either` is not
  admitted (PLAN 2.2). S2 drops or keeps them with the `REFUSE_CELL`
  review. S2d closed O12: it removed `Sum`, `inl`, `inr`, `either`
  (Q-S2d-4 (a)) and kept `Unit`, `unit` (Q-S2d-5 (a)).
- O13: `flagIf` keeps its kit name (it is not in the S1 drop list). The
  USER rules the name of `if`.
- O14: dead core paths. The Op values and the eval and lowering code for
  `List`, `Prod`, `filter` and `unfold` stay, but no program reaches them.
  S2 removes them. S2d closed O14 (Q-S2d-7 (a)): see the S2d Op values.
- O15: the value printer and some check messages use the ASCII kit
  spelling (`->`, `fun x =>`, `Sigma (x : A) B`, `Nat`). The parser reads
  it back through the aliases.
- O16: `nil` is in the S1 list of dropped kit names (`NAME_UNKNOWN` with a
  hint). PLAN 3.2 admits `nil` and `snoc` as the `Chain` constructors. S3
  removes `nil` from that list when it adds `Chain`.

## S2a: kernel part 1 (interval, paths, `transp`, `hcomp`, systems)

S2a adds the interval `I` with `i0`, `i1`, `~`, `∧` and `∨`; `PathP` and
`Path`; `transp`; `hcomp`; `Partial` and systems `[ φ ↦ u , ... ]`. `Eq`
is a dropped name: use `Path A x y`. `refl`, `symm`, `trans`, `cong` and
`transport` stay.

### Source
- The frozen leaf-lang S2 working tree of 2026-10-26 (leaf-lang base
  e3e2338, its U4 point, gate EXIT 0). Patch
  `leaf-s2-wt-on-e3e2338.patch`, sha256 be7912102f3ee04f (first 16 hex
  digits). S2a read no file of the leaf-lang repository.
- `git apply --exclude=docs/PLAN.md`: no reject. The patch changes 21
  files and adds 17 files.

### Dropped hunks
- All `docs/PLAN.md` hunks. They are leaf-lang plan text.
- No other hunk. The name search (`leaf`, `xmss`, `wots`, `sphincs`,
  `lamport`, `signature`) finds 22 lines in the patch: 18 are in the
  `docs/PLAN.md` hunks, and 4 are the C name `parse_leaf` or "signature"
  as a type signature. No line names leaf-lang. No hunk has a leaf-lang
  domain name (PLAN 3.2).

### Gate counts against S1

| Count | S1 | S2a |
|---|---|---|
| examples | 10 | 10 |
| parse refusals | 9 | 10 |
| check refusals | 18 | 24 |
| evals | 30 | 50 |
| ir lowered | 10 | 10 |
| wasm modules | 13 | 13 |
| differential cases (0 mismatches) | 772 | 773 |
| fmt canonical, non-canonical pairs | 50, 3 | 64, 5 |
| json fixtures | 20 | 24 |
| diagnostics codes (fixture, no fixture) | 31 (18, 13) | 36 (22, 14) |

Each S2a count is equal to the count in the snapshot gate log. The gate
has 0 failures. The review, domain and diagnostic edit regressions pass.
The S1 review behavior stays: the `W_NONCANON` span covers exactly the
non-canonical text.

### Mutants
Each mutant makes `make check` fail on its own `test/check` file:
- M1: no `transp` face constancy check (`rule_transp`):
  `transp-face.lang` gives exit 0. KILLED.
- M2: no system agreement check (`rule_system`): `system-disagree.lang`
  gives exit 0. KILLED.
- M3: no `hcomp` base face check (`rule_hcomp`): `hcomp-base.lang` gives
  exit 0. KILLED.

### Op values
50 Op values (S1: 47). S2a removes `OP_PROD`, `OP_LIST`, `OP_EQ`,
`OP_UNFOLD` and `OP_FILTER` (part of O14).

### Open items from S2a (PLAN O18)
- The face tests are conservative where an atom of a meet stays free.
- Two systems convert by structure, in order (no order-free conversion).
- A system has at most `FACE_MEETS_MAX` cells (`TYPE_SYSTEM`).

### Review corrections

- `hcomp` faces resolve names under the side binder, then must be
  independent of it. Its binder annotation is checked in the outer scope.
- Conversion uses the enclosing system faces in path endpoints, `refl`,
  path combinators and binder annotations.
- The value printer omits the internal type and face arguments of
  `hcomp`, and annotates the interval binder of a printed `PathP` line.
- `test/cubical.c` runs 17 checking regressions and 8 normalized-source
  round trips. The same suite fails against the original staged kernel.
  `make check` and an AddressSanitizer/UndefinedBehaviorSanitizer run pass.

## S2b: kernel part 2 (indexed families, path constructors, `REFUSE_CELL`)

S2b adds indexed families (the `Step` shape, D3), path constructors of
dimension 1 (`Path (F A) s t`, Q-S2b-4 (a), Q-S2b-5 (a)) and the CM
criterion for program data with a type argument (`REFUSE_CELL`, rule R1).
Program data with no type argument in its params or indices gets
`REFUSE_DATA` (Q-S2b-1 (a)). A family or a fold outside the S2 data rules
gets `DOMAIN_FAMILY`. The builtin `Option` stays (Q-S2b-3 (a)).
`test/eval/cells.lang` declares the CM twin `Maybe` and `Trunc` with the
1-cell `squash` (Q-S2b-2 (a)).

### Source
- quorum-lang 8dee55b, read only through `git show`. S2b is a hand port,
  not a patch. It changes 23 files and adds 17 files. S2b read no file of
  the leaf-lang repository.

### Gate counts against S2a

| Count | S2a | S2b |
|---|---|---|
| examples | 10 | 10 |
| parse refusals | 10 | 10 |
| check refusals | 24 | 37 |
| evals | 50 | 53 |
| ir lowered | 10 | 10 |
| wasm modules | 13 | 13 |
| differential cases (0 mismatches) | 773 | 773 |
| fmt canonical, non-canonical pairs | 64, 5 | 81, 5 |
| json fixtures | 24 | 26 |
| diagnostics codes (fixture, no fixture) | 36 (22, 14) | 38 (24, 14) |
| cubical checks, round trips | 17, 8 | 22, 8 |

The gate has 0 failures. The review, domain and diagnostic edit
regressions pass.

### Mutants
Each mutant edits one branch of `src/front/check.c`. The 14 mutants that
build are all KILLED. M3 does not build.

| Mutant | Branch | Result |
|---|---|---|
| M1 | type index | KILLED: `cell-type-index.lang` gives exit 0 |
| M2 | cell header (a parameter that is not a universe) | KILLED by the pin "is not a universe" of `cell-value-param.lang` |
| M3 | one field (`return 1 ? 1 :`) | STILLBORN: clang -Werror -Wunused-parameter stops the build |
| M3b | one field (`field_count >= 1u`) | KILLED by the pin "must be the one field of its constructor" of `cell-pair.lang` and `cell-list.lang`, and by the JSON message of `test/json/REFUSE_CELL.lang` |
| M4 | general field rule | KILLED: `cell-field.lang` (`DOMAIN_FAMILY`) |
| M5 | no generator | KILLED: `cell-ghost.lang` |
| M6 | more than one generator | KILLED: `cell-two-gen.lang` |
| M7 | path constructor shape | KILLED by the pin "must have the type Path T s t" of `cell-path-shape.lang` |
| M8 | path constructor dimension | KILLED by the pin "has dimension 2" of `cell-path-dim.lang` |
| M9 | path constructor end | KILLED: `cell-path-end.lang` |
| M10 | `REFUSE_DATA` | KILLED: `data.lang` and 2 more FAIL lines |
| M11 | fold over an indexed family | KILLED: `index-fold.lang` and 2 more FAIL lines |
| M12 | fold over a higher inductive family | KILLED: `hit-fold.lang` |
| A1 | admit side: the param split collapsed | KILLED: both entries of `test/eval/cells.lang` |
| A2 | admit side: each path end refused | KILLED: both entries of `test/eval/cells.lang` |

Without the pins, M2, M7 and M8 survive: a later branch gives the same
code (Q-S2b-7). A line of `test/check/expect.txt` can now have text after
the code. The first stderr line must then contain that text
(`test/gate.sh`).

### Op values
51 Op values (S2a: 50). S2b adds `OP_PCTOR`. `CtorInfo` has `depth` (0
for a point constructor, 1 for a path constructor), `type` (the path type
of a path constructor) and `ends` (2 * depth ends).

### Open items from S2b
- The builtin `Option` stays. S2d removes or keeps it (Q-S2b-3 (a), O12,
  O14).
- A square (dimension 2) gets `REFUSE_CELL` (Q-S2b-5 (a)).
- The `DOMAIN_FAMILY` guard "path constructor outside a cell signature"
  is not reachable from a program under Q-S2b-1 (a).
- The messages use two sets of rule numbers (Q-S2b-8). S2b corrects the
  docs that said that a program cannot declare `data` (`SPEC.md`,
  `domain/README.md`, `formers/tcc-wasm.md`, `docs/host/README.md`).
- Q-S2b-1 to Q-S2b-8 are open (PLAN section 11).

## S2c: kernel part 3 (codata, the CC branch of `REFUSE_CELL`)

S2c adds `codata` (Q-S2c-1 (a)). A `codata` type has one clause. The
clause name is the corecursor (`corec`). The clause fields are the
observation `head : A` and the field `tail : Stream A` (Q-S2c-3 (a)). The
destructors `head` and `tail` take an explicit type argument, as in
`head ℕ s` (Q-S2c-8 (a)). A field can have a label: `E → D A`
(Q-S2c-6 (a)). A program cofamily runs the CC criterion (the CC branch of
`REFUSE_CELL`, rule R1), and the checker admits a cofamily that passes it
(Q-S2c-2 (a)). `test/eval/streams.lang` declares `Stream`.

### Source
- intent-lang 67445ae. S2c is a hand port, not a patch. It changes 17
  files (10 source and test files, 7 docs) and adds 17 files: 14
  `test/check/codata-*.lang` files, `test/eval/streams.lang`,
  `test/fmt/codata.lang` and `test/fmt/codata.canon`.

### Gate counts against S2b

| Count | S2b | S2c |
|---|---|---|
| examples | 10 | 10 |
| parse refusals | 10 | 10 |
| check refusals | 37 | 51 |
| evals | 53 | 58 |
| ir lowered | 10 | 10 |
| wasm modules | 13 | 13 |
| differential cases (0 mismatches) | 773 | 773 |
| fmt canonical, non-canonical pairs | 81, 5 | 97, 6 |
| json fixtures | 26 | 26 |
| diagnostics codes (fixture, no fixture) | 38 (24, 14) | 38 (24, 14) |
| cubical checks, round trips | 22, 8 | 32, 8 |

The gate has 0 failures. The review, domain and diagnostic edit
regressions pass.

### Mutants
Each mutant edits one branch of `src/front/check.c`. C1 to C10 are the
mutants of the S2c survey. D1 drops the duplicate-field loop. These 11
mutants are all KILLED. Two more mutants test the U4 fixtures: C10b is
KILLED and S1 SURVIVES.

| Mutant | Branch | Result |
|---|---|---|
| C1 | observation test off by one | KILLED: 15 FAIL lines: the 5 evals of `streams.lang`, the 2 checks of `test/fmt/codata` and 8 check pins |
| C2 | admit two observations | KILLED: `codata-two-obs.lang` gives exit 0 (after the fix F-U5-1) |
| C3 | admit no observation | KILLED: `codata-no-obs.lang` gives exit 0 |
| C4 | one type parameter | KILLED: `codata-params.lang` gives `TYPE_NOT_FUNCTION` |
| C5 | the parameter is a universe | KILLED by the pin "is not a universe" of `codata-value-param.lang` |
| C6 | no index | KILLED: `codata-index.lang` gives `TYPE_NOT_FUNCTION` |
| C7 | the argument test of `cocell_app` | KILLED: `codata-other-arg.lang` and `codata-nested.lang` give exit 0 |
| C8 | the label is closed | KILLED: `codata-open-label.lang` gives `INTERNAL` |
| C9 | the walk order of the corecursor | KILLED by the cubical check "codata tail law" (`TYPE_REFL`) |
| C10 | one clause, not a path constructor | KILLED: `codata-clauses.lang` gives exit 0; `codata-result.lang` gives "it has 0 observations" |
| D1 | the duplicate-field loop | KILLED by the pin "tail is already a field of this clause" of `codata-dup-field.lang` |
| C10b | the result half of the clause rule | KILLED by the pin of `codata-result.lang` |
| S1 | the parameter check and the index check swapped | SURVIVED: each fixture has one top-level fault, so the order is not observable |

Without its pin, C5 survives: a later branch gives the same code
(Q-S2b-7 (a)). Without its pin, D1 survives: a later `REFUSE_NAME`
branch gives the same code. C8 shows that the closed-label check also
guards the evaluator: without it, an open label reaches `INTERNAL`.

F-U5-1 (FIXED): the second observation of `codata-two-obs.lang` was
`top`, a field of the domain constructor `push`. The shape check runs
first, so the pin passed. Under C2, the R4 field check gave
`REFUSE_NAME`. The fixture now uses `peek`. The counts do not change.

### Op values
51 Op values, no change (Q-S2c-4 (a)). The position encoding is built as
Core in C. `eval.c`, `lower.c` and `core.h` do not change.

### Review fix

Each position constructor has distinct internal field names. Reusing
`D.rest` and `D.label` caused valid signatures with multiple recursive
fields to fail with `REFUSE_NAME`. Two cubical regressions check mixed
observations of plain and labelled fields with different state steps.

### Open items from S2c
- `StreamBisim` goes to S3 (Q-S2c-5 (a)).
- S3 decides the domain place of `Stream` (Q-S2c-2 (a)).
- F-U4-1 is RULED (a), no work: 7 FAIL sites of cocell_shape give the
  texts of the 13 survey branches, and the S2c texts stay (PLAN section
  11).
- Q-S2c-1 to Q-S2c-8 are RULED (a). Q-S2b-1 to Q-S2b-8 stay open (PLAN
  section 11).

## S2d: kernel part 4 (`Fin n`, O12, the rest of O14, O15)

### Op values

47 Op values (51 before S2d). U2 removed `OP_SUM`, `OP_INL`, `OP_INR`
and `OP_EITHER` (Q-S2d-4 (a)). The table (Q-S2d-7 (a)) names one
program that reaches each Op: its checked Core or its evaluation holds
the Op. 46 of 47 Ops are reached.

| Op | Surface | Program |
|---|---|---|
| OP_NAT | `ℕ` | examples/entries.lang:2 |
| OP_BOOL | `Bool` | examples/entries.lang:11 |
| OP_UNIT | `Unit` | test/review.sh:11 (heredoc program; also test/cubical.c:78) |
| OP_OPTION | `Option ℕ` | examples/formers.lang:20 |
| OP_FAMILY | a domain data type used as a type (`Item`) | examples/inventory.lang:2 |
| OP_UNIT_VAL | `unit` | test/review.sh:15 (also test/cubical.c:78) |
| OP_TRUE | `true` | examples/formers.lang:6 |
| OP_FALSE | `false` | examples/monad.lang:15 |
| OP_NONE | `none` | examples/formers.lang:27 |
| OP_SOME | `some` | examples/formers.lang:21 |
| OP_PACK | `pack` | examples/formers.lang:6 |
| OP_CTOR | a data constructor (`makeItem`, `red`) | examples/inventory.lang:3 |
| OP_OPTION_ELIM | `option` | examples/formers.lang:24 |
| OP_PURE | `pure` | examples/monad.lang:9 |
| OP_MAP | `map` | examples/monad.lang:3 |
| OP_BIND | `bind` | examples/monad.lang:6 |
| OP_FOLD_NAT | `fold` over ℕ | examples/algebra.lang:3 |
| OP_FOLD_FAMILY | `fold` over a data type | examples/algebra.lang:6 |
| OP_WITNESS | `witness` | examples/formers.lang:9 |
| OP_PAYLOAD | `payload` | examples/formers.lang:12 |
| OP_NAT_ADD | `natAdd` | examples/entries.lang:3 |
| OP_NAT_SUB | `natSub` | examples/entries.lang:9 |
| OP_NAT_MUL | `natMul` | examples/entries.lang:6 |
| OP_NAT_EQ | `natEq` | examples/entries.lang:29 |
| OP_NAT_LE | `natLe` | examples/entries.lang:12 |
| OP_FLAG_IF | `flagIf` | examples/entries.lang:18 |
| OP_PROJ | a constructor field projection (`lotPrice`) | examples/inventory.lang:27 |
| OP_SSET | no surface name; the sort of `I` and `Partial` (V only, built by op0 in check.c; never a Core node) | test/eval/path.lang:10 |
| OP_I | `I` | test/eval/path.lang:10 |
| OP_I0 | `i0` | examples/equality.lang:27 |
| OP_I1 | `i1` | test/eval/path.lang:8 |
| OP_PATHP | `PathP` / `Path` | examples/equality.lang:8 |
| OP_PLAM | path lambda (`refl`, `λ i`) | examples/equality.lang:9 |
| OP_PAPP | path application `p r` | test/eval/path.lang:8 |
| OP_INEG | `~` | test/eval/path.lang:41 |
| OP_IMIN | `∧` | test/eval/path.lang:50 |
| OP_IMAX | `∨` | test/eval/path.lang:53 |
| OP_PARTIAL | `Partial` | test/eval/path.lang:61 |
| OP_SYSTEM | `[ φ ↦ u , ... ]` | test/eval/path.lang:62 |
| OP_SYSNIL | `[]` | test/eval/path.lang:65 |
| OP_FROMPARTIAL | none (see Not reached) | NOT REACHED |
| OP_TRANSP | `transp` | test/eval/path.lang:73 |
| OP_TPART | internal; template op run by `hcomp` over Sigma | test/eval/path.lang:99 (via `langc eval test/eval/path.lang hSig`; also :102 tPath) |
| OP_TCOD | internal; template op run by `hcomp` over Sigma | test/eval/path.lang:99 (via `langc eval test/eval/path.lang hSig`) |
| OP_HCOMP | `hcomp` | test/eval/path.lang:93 |
| OP_SYSMAP | internal; template op run by `hcomp` over Sigma or a path | test/eval/path.lang:99 (via `langc eval ... hSig`; also :102 tPath) |
| OP_PCTOR | path constructor application (`squash`) | test/eval/cells.lang:13 |

`OP_FROMPARTIAL` is not reached. `check.c` does not build it. `eval.c`
builds it only in `reduce_from_partial`, for an `hcomp` with a holding
face when the side at `i1` is not a system with a holding first face. No
program in `examples` or `test` reaches that branch. The Op stays, listed
as unreached (F-U2-2, RULED (a)).

### `Fin n` (U3)

`Fin` is a builtin with no Op (the Op count stays 47). The check gives
`Fin n` the Core `(k : ℕ) × Path Bool (natLe (natAdd k 1) n) true`
(Q-S2d-1 (a), Q-S2d-2 (a)). A value is `pack k refl`, and `witness`
gives `k`. Messages print the Sigma.

| Program | Result |
|---|---|
| test/eval/fin.lang `two` (`pack 2 refl : Fin 3`) | `pack 2 (λ i → true)` |
| test/eval/fin.lang `first` (`pack 0 refl : Fin 1`) | `pack 0 (λ i → true)` |
| test/eval/fin.lang `index` (`witness two`) | `2` |
| test/eval/fin.lang `big` (`pack 18446744073709551614 refl : Fin 18446744073709551615`) | `pack 18446744073709551614 (λ i → true)` |
| test/check/fin-bound.lang (`pack 3 refl : Fin 3`) | `TYPE_REFL`: the left side normalizes to false |
| test/check/fin-empty.lang (`pack 0 refl : Fin 0`) | `TYPE_REFL`: the left side normalizes to false |
| test/cubical.c, 4 checks | `Fin 2` is equal to its Sigma; `pack 1 refl : Fin 2` checks; `pack 2 refl : Fin 2` gets `TYPE_REFL`; with a neutral bound `n`, `payload x` checks for `x : Fin n` |

Probes with no fixture (evolve-lang-work/S2d-U3-probes):
`pack 18446744073709551615 refl : Fin 18446744073709551615` gets
`TYPE_REFL` (the left side normalizes to trap). `Fin true` gets
`TYPE_MISMATCH`. A declaration named `Fin` gets `REFUSE_NAME` (rule R4).
`List ℕ` gets `NAME_UNKNOWN` with the hint "use a vector Fin n → A"
(Q-S2d-9 (a)). The `fix` field of `List`, `nil` and `cons` stays NULL
(F-U3-2, RULED (a)). `last : (n : ℕ) → Fin (natAdd n 1)` with
`last = λ n → pack n refl` gets `TYPE_REFL`: `natLe` has no rule for a
neutral `n`.

F-U3-1 (RULED (a)): no probe gets `REFUSE_RUNTIME_PATH`. `langc ir`
leaves out the definitions of type `Fin 3` and `Fin 3 → ℕ`, and it folds
a closed use (`index` lowers to 2). The lowering stays, and the Q-S2d-3
reading text tells what `langc ir` does.

### Mutants
Each mutant edits one site of `rule_fin` in `src/front/check.c`. F1 to
F6 are the mutants of the S2d survey. F7 is not in the survey. These 7
mutants are all KILLED.

| Mutant | Site | Result |
|---|---|---|
| F1 | `natLe k n` for `natLe (natAdd k 1) n` (le for lt) | KILLED: `fin-bound.lang` and `fin-empty.lang` give exit 0; the cubical checks "Fin unfolds to a Sigma" and "Fin value at the bound" |
| F2 | the 2 arguments of `natLe` swapped | KILLED: `fin-bound.lang` and `fin-empty.lang` give exit 0; the same 2 cubical checks. The 4 evals pass: each value has `k + 1 = n` |
| F3 | `false` for `true` in the path type | KILLED: `fin-bound.lang` and `fin-empty.lang` give exit 0; the 4 evals of `test/eval/fin` give `TYPE_REFL`; the 3 cubical checks of `Fin` |
| F4 | the bound `natAdd n 1` for `n` | KILLED: `fin-bound.lang` and `fin-empty.lang` give exit 0; the 4 evals give `TYPE_REFL` on `big` (`natAdd n 1` traps at n = 18446744073709551615); the cubical checks "Fin unfolds to a Sigma" and "Fin value at the bound" |
| F5 | the path part dropped (`Fin n` is `ℕ`) | KILLED: `fin-bound.lang` and `fin-empty.lang` give `TYPE_INFER`, not `TYPE_REFL`; the 4 evals give `TYPE_INFER`; the 3 cubical checks of `Fin` |
| F6 | the bound from `k` (`natLe (natAdd k 1) k`) | KILLED: the 4 evals give `TYPE_REFL`; the cubical checks "Fin unfolds to a Sigma" and "Fin value below the bound". `fin-bound.lang` and `fin-empty.lang` pass |
| F7 | the quote level `c->level` for `c->level + 1u` | KILLED: the cubical check "Fin with a neutral bound" gets `TYPE_MISMATCH`. The other fixtures have a closed bound, and the quote of a closed value does not use the level |

F-U4-3 (RULED (a)): in U4, F7 SURVIVED. The probe
evolve-lang-work/S2d-U4-probes/neutral.lang (`value : (n : ℕ) → (x :
Fin n) → Path Bool (natLe (natAdd (witness x) 1) n) true`, `value n x =
payload x`) checks on the S2d code. With F7 it gets `TYPE_MISMATCH`: the
bound changes from `n` to `witness x`. U5 adds this program as the
cubical check "Fin with a neutral bound" that wants ok (cubical 35 to 36
checks). F7 is KILLED by this check.

### Printer (U5)
F-U1b-1 (RULED (a)) and F-U5-1 (RULED (a)): `print_binder` in
`src/front/eval.c` follows fmt.c for 3 forms. A Sigma in the domain of
`→` prints as `ℕ × ℕ → ℕ`, a universe in the domain of `→` as `Type 0 →
ℕ`, and a universe left of `×` as `Type 0 × ℕ`. No expectation line
changes. The cubical round trip "universe left of arrow" is new (21 to
22 round trips).

## S2e: univalence (`Glue`, `glue`, `unglue`, `ua`)

S2e adds the core formers `Glue`, `glue` and `unglue`, and the kernel
prelude `src/front/prelude.lang` with `isContr`, `fiber`, `isEquiv`,
`Equiv`, `idEquiv`, `ua` and `transpEquiv` (Q-S2e-1 (a), core names).
`Glue A [ φ ↦ pack T e , ... ]` is a type. On each face, `e` is an
`Equiv T A`. The checker refuses a bad Glue term with `GLUE_EQUIV`,
`GLUE_COVER`, `GLUE_BOUNDARY` or `UNGLUE_TYPE`. `langc ir` refuses a
Glue value at run time (`REFUSE_RUNTIME_GLUE`). The Kan rules are
transp_glue, hcomp_glue, hcomp at `Type 0` (hcomp_univ, with
`transpEquiv`) and Glue eta (Q-S2e-2 (a)). A transport along `ua`
applies the equivalence (law L3).

### Source
- zkpolicy-lang 1d263fb (the PLAN port source table). S2e is a hand
  port, not a patch. The USER commit 86c99ed changes 20 files and adds
  16 files.

### USER fix (86c99ed)
86c99ed is the staged S2e tree d939e170 with a USER edit of 4 files
(53 lines added, 5 removed):
- `src/front/check.c`: `glue_branch_hint` checks each Glue branch
  against its type under the complete branch face: pairs, lambdas,
  nested Glue, and covers with a different partition.
- `src/front/eval.c`: `print_skips` prints a Glue element and a neutral
  `unglue` term with their source arguments, so that the checker can
  check a normalized term again.
- `test/cubical.c`: 6 new checks (the last is `GLUE_BOUNDARY` on a pair
  boundary that does not agree) and 2 new round trips (a Glue element,
  a neutral `unglue`).
- `docs/PLAN.md`: the S2e row gives cubical 49 checks, 24 round trips.

### Gate counts against S2d

| Count | S2d | S2e (staged) | 86c99ed |
|---|---|---|---|
| examples | 10 | 11 | 11 |
| parse refusals | 10 | 10 | 10 |
| check refusals | 54 | 59 | 59 |
| evals | 62 | 69 | 69 |
| ir lowered | 10 | 11 | 11 |
| wasm modules | 13 | 14 | 14 |
| differential cases (0 mismatches) | 773 | 883 | 883 |
| fmt canonical, non-canonical pairs | 101, 6 | 116, 6 | 116, 6 |
| json fixtures | 26 | 30 | 30 |
| diagnostics codes (fixture, no fixture) | 38 (24, 14) | 43 (28, 15) | 43 (28, 15) |
| cubical checks, round trips | 36, 22 | 43, 22 | 49, 24 |

The S2d column is the S2e base gate, the S2e (staged) column is the S2e
final gate on d939e170, and the 86c99ed column is the S3 base gate. Each
gate has 0 failures. The review, domain and diagnostic edit regressions
pass.

### Mutants
Each mutant edits one Kan rule in `src/front/eval.c` (Q-S2e-8 (a)).
These 5 mutants are all KILLED. M1, M3 and M5 lived first. The new test
in their row kills them (Q-S2e-13 (a), Q-S2e-14 (c)).

| Mutant | Site | Result |
|---|---|---|
| M1 | `transp_glue`: `&& fib_def == UINT32_MAX` added to the `delta` test, so `fiber_extend` does not run | KILLED by the new cubical check "ua fiber center not in normal form" (`TYPE_SYSTEM`) |
| M2 | `transp_glue`: the 3 `OP_I0` of the a0 `unglue` changed to `OP_I1` | KILLED: 6 FAIL lines: `examples/glue.lang` gives `TYPE_REFL` on `uaBetaSwap`; the evals `moved`, `first` and `uaSwapped`; the ir and wasm lines of `examples/glue.lang` |
| M3 | `hcomp_glue`: the side system (`ψ ↦ unglue (side i)`) changed to the empty system | KILLED by the new cubical check "unglue of an hcomp at a Glue type" (`TYPE_REFL`) |
| M4 | `hcomp_glue`: `tsys == NULL ? NULL :` added before the `OP_GLUE_ELEM` result | KILLED: 6 FAIL lines: `examples/glue.lang` gives `TYPE_REFL` on `hcompGlue`; the same 5 lines as M2 |
| M5 | `hcomp_univ`: an `OP_I1` changed to `OP_I0` | KILLED by the new eval `hcompEAt1` of `test/eval/hcomp-line.lang` (`TYPE_REFL`) |

### Op values
50 Op values (47 before S2e). U1 adds `OP_GLUE`, `OP_GLUE_ELEM` and
`OP_UNGLUE` (Q-S3-8 (a)). Each row names one program that reaches the
Op, in the form of the S2d table.

| Op | Surface | Program |
|---|---|---|
| OP_GLUE | `Glue` | examples/glue.lang:30 |
| OP_GLUE_ELEM | `glue` | examples/glue.lang:31 |
| OP_UNGLUE | `unglue` | test/eval/hcomp-glue.lang:11 |

### Open items from S2e
- Q-S2e-1 to Q-S2e-14 are RULED. Q-S2b-1 to Q-S2b-8 stay open (PLAN
  section 11).
- S3 U5 adds this section (Q-S3-8 (a)).

## S3: domain (`table`, `app`, `protocol`, the steps)

S3 adds the evolve domain to `domain/domain.lang`, the forms `table`,
`app` and `protocol`, the eliminator over indexed families, and the
codes `REFUSE_STEP_INDEX` and `REFUSE_RENAME_INPLACE`. SPEC.md sections
5 and 6 give the domain (Q-S3-31 (a)).

### Source
- No port source: the PLAN port source table has no S3 row. S3 changes
  26 files, removes 1 file and adds 20 files.

### Units
- U0, the eliminator over indexed families: the Op `OP_ELIM_FAMILY`.
  `fold` with a motive over an indexed family is the dependent
  eliminator, with one induction hypothesis for each recursive field
  (Q-S3-15). `domain/domain.lang` has `data Walk`.
  `test/check/index-fold.lang` moves to `test/eval/index-fold.lang`. New
  tests: 3 `test/check/fold-*.lang` files, the cubical check "Walk left
  unit by the eliminator" and `test/ir/walk.lang`. The `DOMAIN_FAMILY`
  texts (Q-S3-16) and `langc ir` (Q-S3-17) change.
- U1, the domain: `ColType`, `Null`, `CState`, `State n m`, `Cols`,
  `App`, 9 pre and 9 post functions, `Step` (11 constructors), `Chain`,
  `compose`, `length` and `Stage`. The checker has no `nil` row (Q-S3-7
  (a)): `test/fmt/system` uses `emptySystem`, and `test/review.sh` uses
  `someSeven`.
- U2, the forms `table`, `app` and `protocol` (Q-S3-22 to Q-S3-27): the
  tokens `.` and `←`, the parser, the printer and the elaboration in
  `src/front/check.c`. New test: `test/fmt/forms`.
- U3, J1 (Q-S3-28 to Q-S3-30): the step words (with `renameColumn`),
  the step check of each `protocol` and the two codes, with their rows
  in `docs/DIAGNOSTICS.md` and 2 new json fixtures.
- U4, the tests: 10 new `test/check` files (one for each pre, and
  `rename-inplace.lang`), `test/eval/steps.lang`, and the mutants M1 to
  M10.
- U5, the docs: SPEC.md sections 5 and 6, the PLAN S3 row and the Q
  lines (Q-S3-9 (a)), `docs/host/README.md`, the S2e section and this
  section.

### Gate counts against S2e

| Count | S2e (86c99ed) | S3 |
|---|---|---|
| examples | 11 | 11 |
| parse refusals | 10 | 10 |
| check refusals | 59 | 71 |
| evals | 69 | 83 |
| ir lowered | 11 | 11 |
| wasm modules | 14 | 15 |
| differential cases (0 mismatches) | 883 | 889 |
| fmt canonical, non-canonical pairs | 116, 6 | 134, 7 |
| json fixtures | 30 | 32 |
| diagnostics codes (fixture, no fixture) | 43 (28, 15) | 45 (30, 15) |
| cubical checks, round trips | 49, 24 | 50, 29 |

The gate has 0 failures. The review, domain and diagnostic edit
regressions pass.

### Op values
51 Op values (50 before S3). U0 adds `OP_ELIM_FAMILY` (the inst is the
family; the arguments are the motive, the cases and the scrutinee). The
program is the dependent fold over `Walk` that `langc ir` lowers
(Q-S3-32 (b)).

| Op | Surface | Program |
|---|---|---|
| OP_ELIM_FAMILY | `fold` with a motive over an indexed family (`walkSum`) | test/ir/walk.lang:10 |

### Mutants
M1 to M9 change the body of one pre function in `domain/domain.lang` to
`true` (Q-S3-10 (a): one mutant for each pre). With the pre `true`, the
step is legal and the killer gets exit 0. The killer text names the
conjunct that is false. M10 is on `check_protocol` in
`src/front/check.c`. M-U0-1 to M-U0-5 are the U0 mutants. These 15
mutants are all KILLED.

| Mutant | Site | Result |
|---|---|---|
| M1 | `preAddColumn` | KILLED by `test/check/step-add-column.lang` (`boolNot (present n m s c) is false`) |
| M2 | `preInstallSync` | KILLED by `test/check/step-install-sync.lang` (`boolNot (sameCol n c d) is false`) |
| M3 | `preDropSync` | KILLED by `test/check/step-drop-sync.lang` (`sync n m s (witness c) (witness d) is false`) |
| M4 | `preBackfill` | KILLED by `test/check/step-backfill.lang` (`natLe 1 k is false`) |
| M5 | `preDropColumn` | KILLED by `test/check/step-drop-column.lang` (`boolNot (sameCol n c (key n m s t)) is false`) |
| M6 | `preAddNotNullCheck` | KILLED by `test/check/step-add-not-null-check.lang` (`isAbsent (check n m s c) is false`) |
| M7 | `preValidate` | KILLED by `test/check/step-validate.lang` (`isNotValid (check n m s c) is false`); also the json compare and `test/json/REFUSE_STEP_INDEX.lang` |
| M8 | `preSetNotNull` | KILLED by `test/check/step-set-not-null.lang` (`isNullable (null n m s c) is false`) |
| M9 | `preDropCheck` | KILLED by `test/check/step-drop-check.lang` (`boolNot (isAbsent (check n m s c)) is false`) |
| M10 | `check_protocol`: `!form_no_rename(c, d) \|\|` changed to `(0 && !form_no_rename(c, d)) \|\|` (no rename refusal) | KILLED by `test/check/rename-inplace.lang` (`REFUSE_RENAME_INPLACE`); also the json compare and `test/json/REFUSE_RENAME_INPLACE.lang` (`NAME_UNKNOWN`) |
| M-U0-1 | `elim_family` in `src/front/eval.c`: the induction hypothesis loop before the field loop | KILLED: the eval `twoSteps` gives `INTERNAL`; the differential `walk.lang` has 5 mismatches |
| M-U0-2 | `elim_case_type`: the induction hypothesis at `motive_at(m, motive, st, p)` | KILLED: the cubical check "Walk left unit by the eliminator" gets `TYPE_MISMATCH` |
| M-U0-3 | the case result at `st->args[p + i]` | KILLED: the same cubical check gets `TYPE_MISMATCH` |
| M-U0-4 | both refusals of a fold over a higher inductive family removed | KILLED: 4 FAIL lines: `hit-fold.lang` and `fold-hit-motive.lang` (`INTERNAL` in the evaluator), the json compare and `test/json/DOMAIN_FAMILY.lang` |
| M-U0-5 | `fam_case` in `src/front/lower.c`: the accumulator at the recursive field, no raw field | KILLED: `langc ir` of `test/ir/walk.lang` gives `INTERNAL` ("the lowering found a missing value"); the wasm line |

M10 does not remove the call: with no call, the static function
`form_no_rename` has no use, and `-Wall -Werror` stops the build. Each
of M1 to M9 pins one false conjunct. The other conjuncts of a pre have
no mutant (Q-S3-10 (a)).

### S3 review fixes

Neutral dependent folds retain annotated motive binders through evaluation
and quotation. Their printed normal forms can be checked again. Five C
round trips cover explicit and constant indexed motives, function-valued
motives, recursive families and parameterized families.

The protocol column-definition pass looks up a column only for
`addColumn`. `deploy` and `retire` have no column index to read.

### Open items from S3
These items are open for the USER. S3 does not change them.
- A second `key` gives "expected ',' or ')', found a name". The template
  prints the token kind for a name token. Q-S3-24 rules "existing code
  and template", but its sample text has "found 'key'".
- `deploy v3`, with `app v3` after the protocol, gets the R3 refusal
  ("v3 is this definition or a later one").
- Stage 0 of a protocol takes the type term of a column that a later
  protocol adds first (Q-S3-22 (a), "the addColumn type"). A bad type
  name there is reported at the earlier protocol (def p_0), with the
  span in the later one.
- The survey count plan gives one fmt pair for each form (3 or more).
  U2 adds 1 pair (`test/fmt/forms`) with all three forms.
- The fmt canonical count moves 121 to 123 (2 json fixtures) to 134 (10
  `test/check` files and `test/eval/steps.lang`). The survey count plan
  does not list these moves.
- The `REFUSE_RENAME_INPLACE` hint names `examples/rename`. No
  `protocol` line is under `examples/` now: `examples/rename` is S4.
- J1 wraps each `Fin` argument as `(λ (x : Fin K) → x) (pack J refl)`,
  so that a conjunct such as `witness c` can infer it. A false pre uses
  evaluator fuel on each conjunct (one budget for the program).
- The SPEC 2.2 keyword list and `LEX_CHAR` symbol list are old: S2 adds
  `∧`, `∨` and `↦`; S3 adds `table`, `app`, `protocol` and `←`. SPEC
  5.2 gives the S3 additions.
- Q-S3-9 (a): `Maybe`, `Trunc` and `Stream` stay in the program test
  files. `StreamBisim` goes to S6 with `Stream KeyRange` and L2.
- Q-S3-1 to Q-S3-32 are RULED. Q-S2b-1 to Q-S2b-8 stay open (PLAN
  section 11).

## S4: the checker (compat, J4, J5, the laws)

S4 adds the compatibility checker. The domain gets the judgement
`compat` (J2), the live set of each stage and the decisions for the laws.
`src/front/check.c` drives the checker over the chain of each `protocol`
in chain order: J1, the drop check, J5 and J4 (Q-S4-1 (a)). S4 adds the
6 codes of PLAN 4.4 and the demo `examples/rename.lang`. SPEC.md sections
5 and 6 give the checker.

### Source
- No port source: the PLAN port source table has no S4 row. S4 changes
  15 files and adds 19 files. It removes no file.

### Units
- U0, the State and the codes: the 9th `State` field
  `link : ℕ → ℕ → Bool` (`putLink`). `postInstallSync` and
  `postBackfill` set it, and no step clears it (Q-S4-3 (a)). New types:
  `Code` (8 codes, in the PLAN 4.4 order), `Fact n m` (only
  `backfillDone`, Q-S4-4 (a)), `Verdict n m`, `LockMode` (5 modes) and
  `ScanClass` (4 classes). `lockOf` and `scanOf` give the lock and the
  scan class of each of the 11 steps (Q-S4-6 (a), F-U0-2). `fm_start`
  gets a 9th argument. New test: `test/eval/checker.lang`. In
  `test/check/cell-list.lang` the constructor `link` becomes `more`
  (F-U0-1: the name clash with the new field).
- U1, J2 (`compat`): `Cols n` is `(k : ℕ) × (ℕ → Fin n)` (Q-S4-9 (a)).
  `src/front/check.c` gets `fm_pick`, `fm_cols` (default `pack 0 refl`,
  Q-S4-12 (a)) and `form_live`. The domain gets `Live` and, among other
  functions, `readsOf`, `writesOf`, `insertsOf`, `isBarrier`,
  `segments`, `phase`, `fresh`, `needsValue`, `getsValue`, `runtimeOf`
  and `compat` with the clauses J2.1 to J2.4 (Q-S4-2, Q-S4-8, Q-S4-10,
  Q-S4-11 (a)). New test:
  `test/eval/compat.lang` (the PLAN 4.6 table).
- U2, J4 and J5 in the C driver: among other
  functions, `fm_refusal`, `fm_j4_refuse`, `fm_drop_required`,
  `fm_lock_scan` and `fm_stage`. `check_protocol`
  makes the live set before the step loop. It runs J4 at stage 0 on
  `nil`. Then, for each step, it runs the drop check
  (`REFUSE_DROP_REQUIRED`), J5 (`REFUSE_LOCK_SCAN`) and J4 (Q-S4-1,
  Q-S4-5, Q-S4-13, Q-S4-14 (a)). The 6 codes have their rows in
  `docs/DIAGNOSTICS.md` and 6 json fixtures. New tests: the 6
  `test/check/j4-*.lang` files. Q-S4-8 (a) edits the test protocols that
  S4 refuses: `test/eval/steps.lang` (the inserts, and `retire v0`
  before `dropSync`) and `test/fmt/forms`.
- U3, the demo: `examples/rename.lang` passes check, fmt, ir and wasm.
  `test/check/rename-drop-early.lang` drops the old column early and
  gets `REFUSE_STALE_READ` at checker stage 5 (Q-S4-13 (a)).
- U4, the laws and the mutants: L5 is `test/eval/chain-laws.lang`
  (`Path` proofs). L1, L4 and L6 are the domain decisions `accepts`,
  `dropSafe` and `lockSafe` (Q-S4-7, Q-S4-16 (a)).
  `examples/rename.lang` proves the three decisions on its protocol by
  `refl` (Q-S4-15 (a)). `test/eval/checker.lang` proves them on each of
  the 11 step forms (33 proofs) and has the negative
  `lockSafeSetNotNullNoCheck`.
  `test/eval/compat.lang` has the negatives `acceptsV1Live` and
  `dropSafeV1Live`. The mutants M1 to M9.
- U5, the docs: SPEC.md sections 5, 5.1, 5.2, 6 and 6.1; PLAN 2.2, 4.6,
  5.1, 6.1 and the S4 row; the `README.md` status; and this section.

### Gate counts against S3

| Count | S3 (9595979) | S4 |
|---|---|---|
| examples | 11 | 12 |
| parse refusals | 10 | 10 |
| check refusals | 71 | 79 |
| evals | 83 | 185 |
| ir lowered | 11 | 12 |
| wasm modules | 15 | 16 |
| differential cases (0 mismatches) | 889 | 892 |
| fmt canonical, non-canonical pairs | 134, 7 | 153, 7 |
| json fixtures | 32 | 38 |
| diagnostics codes (fixture, no fixture) | 45 (30, 15) | 51 (36, 15) |
| cubical checks, round trips | 50, 29 | 52, 29 |

The gate has 0 failures. The review, domain and diagnostic edit
regressions pass.

### Op values
51 Op values. S4 adds no Op. `src/front/eval.c` and the kernel do not
change.

### Mutants
M1 to M4 set one clause of `compat` (J2.1 to J2.4) to `true`. M5 removes
the drop-required refusal. M6 changes the scan class of `setNotNull`. M7
makes `addColumn` a barrier. M8 moves `fm_stage` before the drop and
lock checks. M9 changes the phase of the `runtime` verdict. These 9
mutants are all KILLED.

| Mutant | Site | Result |
|---|---|---|
| M1 | `compat`: the J2.1 present clause changed to `true` | KILLED by `test/check/j4-missing-column.lang` (got `REFUSE_UNFILLED_READ`); also the evals `v3At0`, `v1At7` and `acceptsV1Live`, the json compare and `test/json/REFUSE_MISSING_COLUMN.lang` |
| M2 | `compat`: the J2.2 fresh clause changed to `true` | KILLED by `test/check/j4-stale-read.lang` (got `REFUSE_DROP_REQUIRED` at stage 6); also `test/check/rename-drop-early.lang`, the eval `v1At6`, the json compare and `test/json/REFUSE_STALE_READ.lang` |
| M3 | `compat`: the J2.3 filled clause changed to `true` | KILLED by `test/check/j4-unfilled-read.lang` (got exit 0); also the evals `v3At1` and `v3At2`, the json compare and `test/json/REFUSE_UNFILLED_READ.lang` |
| M4 | `compat`: the J2.4 insert clause changed to `true` | KILLED by `test/check/j4-null-insert.lang` (got exit 0); also the evals `noKeyAt0` and `v1StrictUnsynced`, the json compare and `test/json/REFUSE_NULL_INSERT.lang` |
| M5 | `fm_drop_required` in `src/front/check.c`: `if (strcmp(` changed to `if (0 && strcmp(` (no drop-required refusal) | KILLED by `test/check/j4-drop-required.lang` (got `REFUSE_MISSING_COLUMN` at stage 5); also the json compare and `test/json/REFUSE_DROP_REQUIRED.lang` |
| M6 | `scanOf` of `setNotNull`: always `metadata` | KILLED by `test/check/j4-lock-scan.lang` (got exit 0); also the evals `scanSetNotNullNoCheck`, `scanSetNotNullNotValid` and `lockSafeSetNotNullNoCheck`, the json compare and `test/json/REFUSE_LOCK_SCAN.lang` |
| M7 | `isBarrier` of `addColumn`: `false` changed to `true` | KILLED by the evals `segmentAt3`, `segmentAt4` and `segmentAt5` in `test/eval/compat.lang` (got 1, 2, 3) |
| M8 | `check_protocol`: `fm_stage` before `fm_drop_required` and `fm_lock_scan` | KILLED by `test/check/j4-drop-required.lang` (got `REFUSE_MISSING_COLUMN` at stage 5); also the json compare and `test/json/REFUSE_DROP_REQUIRED.lang` |
| M9 | `runtimeCol`: `boolOr (phase = 1) (phase = 2)` changed to `phase = 2` | KILLED by the eval `v3At3` in `test/eval/compat.lang` (want `runtime`, got `compatible 3 1`) |

- M9 reading: "the runtime verdict accepted as compatible only
  before the barrier" is read as follows. In phase 1 (after the
  backfill, before the first barrier after it) a read gets `compatible`, not
  `runtime`. Phase 2 (at the barrier) keeps `runtime`. Only the compat evals see
  the difference: the C driver continues for the two verdicts.
- M5 and M8 have the same killer. With no drop-required refusal, or with
  the stage verdicts first, the `dropColumn` stage gives
  `REFUSE_MISSING_COLUMN`.
- The evals also kill domain mutants: `acceptsV1Live` kills M1 (it gives
  1 when J2.1 is `true`), and `lockSafeSetNotNullNoCheck` kills M6.
- M7 is on the domain `isBarrier`, not on the C `form_is_barrier`. The C
  function only selects the `deploy` and `retire` steps for the live
  set. A step that is not `deploy` or `retire` names a table, not an
  app. Thus a C mutant of the same kind does not change the live set.
  This is a reading of the code: that C mutant was not run.

### Staged review fixes

- J2.3 tracks recreated columns through `needsFill`, so initial presence
  cannot bypass the unfilled-read check or the backfill runtime verdict.
  `test/check/j4-readd-unfilled.lang` and `test/eval/backfill-reuse.lang`
  cover the refused read and the runtime verdict after a new backfill.
- `phase` restarts at each backfill. A later backfill cannot reuse the
  runtime discharge of an earlier barrier. `test/eval/backfill-reuse.lang`
  covers a second backfill after a barrier.
- `fm_stage` uses the protocol name as its local live-set index. An app
  cannot have that name, so generated lambdas preserve apps named `j`.
  Two C regressions in `test/cubical.c` cover single and multiple live apps.

### Open items from S4
These items are open for the USER. S4 does not change them.
- S5: `Fact` gets `retired`, `fleetAt` and `Guard` (Q-S4-4 (a)). The J3
  guard on `fleetAt` is S5 or S6.
- The survey count plan gives about 100 evals. S4 has 185 evals (with
  the 33 checker proofs).
- The survey U3 line (`REFUSE_DROP_REQUIRED` for the drop-early variant)
  and the "stage 6" of Q-S4-5 are old: Q-S4-13 (a) replaces them
  (`REFUSE_STALE_READ` at stage 5).
- Q-S4-12: `app v0 ()` in a program with no column gets `TYPE_REFL`. No
  test covers it.
- At stage 0, the `REFUSE_MISSING_COLUMN` hint says "move the deploy of
  V after stage 0" also for an app with no `deploy` step (as built).
- The C driver and the domain overlap. `REFUSE_DROP_REQUIRED` is in C,
  and L4 is the domain `dropHits` and `dropSafe`. `accepts` runs
  `compat` again. The `lives` and `cols` of `examples/rename.lang` are
  hand copies of the values that `form_live` and `fm_cols` make.
- A domain term cannot iterate over `Fin k → X` for a variable `k`
  (`TYPE_REFL` on `natLe`). This gave Q-S4-9 to Q-S4-11 (the ℕ-indexed
  `Cols` and `Live`).
- The progress line keeps `"laws":"0 of 0"`. `docs/DIAGNOSTICS.md` says
  that `affine` and `laws` stay `"0 of 0"` until S3 and S4.
- Q-S4-1 to Q-S4-16 and F-U0-2 are RULED (Q-S4-15 (a), Q-S4-16 (a):
  USER 2026-10-28). Q-S2b-1 to Q-S2b-8 stay open (PLAN section 11).

## S5: the JSON compatibility report (`Fact`, `Guard`, `langc report`)

S5 adds the runtime obligations and the JSON compatibility report. The
domain gets the facts `retired` and `fleetAt`, the type `Guard n m A` and
the guest `stageGuard`. `check_protocol` fills a C `Report` for each
protocol, and `langc report FILE PROTOCOL` writes it as one JSON document
(PLAN 5.2). SPEC.md sections 5, 6 and 6.2 give the report.

### Source
- The `Out` core of lang-template 5265a2a `hosts/tcc-json/src/json.c`,
  with no `Machine` (Q-S5-1 (a)). The typed value printer is not ported.
  S5 changes 15 files and adds 14 files. It removes no file.

### Units
- U0, the facts and the guards: `Fact n m` gets `retired (a : ℕ)` (the
  code of an app, in program app order) and `fleetAt (s : ℕ)` (Q-S5-3
  (a)). New type `Guard n m A` (`done`, `need`) and the guest
  `stageGuard` with `sameFact`, `guardHas`, `needNew`, `runtimeNew` and
  `eachApp`: `fleetAt` at a barrier, then each distinct `backfillDone` of
  a live `runtime` verdict, then `retired` for each retired app whose
  `compat` is refused (Q-S5-4 (a), Q-S5-8 (a): one `need` node for each
  distinct fact). New test: `test/eval/guard.lang`.
- U1, the writer: `src/json.c` and `src/json.h` (`JsonOut` on the arena;
  `json_put`, `json_put_text`, `json_put_string`). `src/front/base.c`
  exports `json_escape`, and `check --json` and the writer use it (UTF-8
  raw, controls `\u00xx`, `\"` and `\\`). The `Makefile` builds
  `src/json.c`. New regressions in `test/cubical.c`: 21 writer cases.
- U2, the report: the C `Report` in `src/front/check.h` (stages, apps,
  segments, cells, guard facts). `check_protocol` fills it
  (`fm_report_start`, `fm_report_stage`, `fm_report_cell`, `fm_fact`).
  A pair that is not live gets the guest `compat` verdict (Q-S5-9 (a)).
  `json_report` writes the keys of Q-S5-10 (a), and `progress` is the
  `check --json` progress object (Q-S5-6 (a)). `src/main.c` adds the
  command `report` (Q-S5-2 (a): the program must pass `check`).
  `W_NONCANON` on `report` goes to stderr as text, and the exit is 0
  (Q-S5-11 (a)).
- U3, the goldens: `test/report/NAME.lang` and `NAME.json` for bare,
  fleet, order, single and strict, and `rename.json` for
  `examples/rename.lang` (Q-S5-7 (a)). `test/gate.sh` compares each
  document byte for byte and prints `report: 6 golden reports`.
- U4, the mutants and the docs: SPEC.md 6.2, PLAN 5.2 and the S5 row,
  the `docs/DIAGNOSTICS.md` progress sentence, the `README.md` status and
  this section.

### Gate counts against S4

| Count | S4 (d10e389) | S5 |
|---|---|---|
| examples | 12 | 12 |
| parse refusals | 10 | 10 |
| check refusals | 79 | 79 |
| evals | 185 | 202 |
| ir lowered | 12 | 12 |
| wasm modules | 16 | 16 |
| differential cases (0 mismatches) | 892 | 892 |
| fmt canonical, non-canonical pairs | 153, 7 | 159, 7 |
| json fixtures | 38 | 38 |
| diagnostics codes (fixture, no fixture) | 51 (36, 15) | 51 (36, 15) |
| golden reports | none | 6 |
| cubical checks, round trips | 52, 29 | 52, 29 |
| json writer regressions | none | 21 |

The gate has 0 failures. The review, domain and diagnostic edit
regressions pass.

### Op values
51 Op values. S5 adds no Op. `src/front/eval.c` and the kernel do not
change.

### Mutants
Each mutant makes one change, then `make -B` and the golden report loop
run. These 11 mutants are all KILLED.

| Mutant | Site | Result |
|---|---|---|
| M1 | `fm_report_cell`: a `compatible` cell becomes `runtime` | KILLED by fleet, order, rename, single and strict |
| M2 | `fm_report_cell`: a `runtime` cell becomes `compatible` | KILLED by order and rename |
| M3 | `fm_report_cell`: a `refused` cell becomes `compatible` | KILLED by fleet, order, rename and strict |
| M4 | `fm_report_cell`: the `live` bit is inverted | KILLED by fleet, order, rename, single and strict |
| M5 | `fm_report_stage`: the last guard fact of a stage is lost | KILLED by fleet, order, rename and strict |
| M6 | `fm_report_start`: no new segment when the live set changes | KILLED by fleet, order, rename and strict |
| M7 | `json_put_verdict`: a `refused` row writes `fix` as `code` | KILLED by fleet, order, rename and strict |
| M8 | `json_report`: the `stage` of a matrix row is the app index | KILLED by fleet, order, rename, single and strict |
| M9 | `fm_fact`: the table and column fields of `backfillDone` swap | KILLED by order and rename (exit 1) |
| M10 | `fm_fact`: the stage of `fleetAt` is one more | KILLED by fleet, order, rename and strict |
| M11 | `stageGuard` (`domain/domain.lang`): no `retired` fact | KILLED by fleet, order, rename and strict |

### S5 staged review fixes

Three missing runtime obligations were reproduced with four failing C
regressions in `test/cubical.c`:

- An app reading two backfilled columns supplied only its first fact.
  `runtimeReads` now retains every distinct fact in app and read order.
- A write-only retired app could stale another app without refusing its
  own accesses. `retiredGuards` checks its effect on the current live set
  and its own accesses with historical writers present. The fourth
  regression covers two retired apps whose combined presence is unsafe.
- An unrelated first barrier could move a backfill to phase 3 without
  ever checking its completion. `barrierFills` now checks any remaining
  phase-2 backfill at that barrier.

The report matrix still shows its first runtime fact. Stage guards are
deduplicated and include all required facts. The four regression cases
failed on the original staged implementation and pass with these fixes.

### Open items from S5
- Q-S5-1 to Q-S5-11 are RULED (a) (USER 2026-10-29). Q-S2b-1 to Q-S2b-8
  stay open (PLAN section 11).
- The `guard` text is the PLAN 4.5 sentence. S6 writes the SQL guards.

## S6: the SQL protocol writer (`langc sql`)

S6 adds the PostgreSQL target. `langc sql PROG PROTOCOL OUTDIR` checks the
program and writes `00-setup.sql` and one `NN-WORD.sql` file for each stage
of the protocol (PLAN 5.1). The files keep a progress ledger for each
tenant, run the guards of `stageGuard` as SQL, and set `lock_timeout` for
each stage. The domain gets the 12th step `createIndexConcurrently`, and
the laws L2 and L3 get proofs. SPEC.md 6.3 gives the protocol.

### Source
- No port. `src/sql.c` (603 lines) and `src/sql.h` are new. The writer
  uses the S5 `JsonOut` buffer (`src/json.h`). S6 changes 18 files and
  adds 63 files. It removes no file.

### Units
- U0, the domain and the clause: `domain/domain.lang` adds
  `preCreateIndex` and the 12th `Step` constructor
  `createIndexConcurrently` (the target is the source), with a case in
  `lockOf` (`shareUpdateExclusive`), `scanOf` (`scan`), `isBarrier`,
  `fillsCol` and `dropHits` (Q-S6-10 (a)). It also adds `KeyRange = ℕ ×
  ℕ` (Q-S6-12 (a)). The parser reads the step word and the clause
  `lockTimeout NAT`: 1 to 2147483647, else `PARSE_EXPECT`; with no
  clause, 2000 ms (Q-S6-6 (a), Q-S6-15 (a)). `langc fmt` keeps the
  clause. New tests: `test/check/step-create-index.lang`,
  `test/parse/lock-timeout-zero.lang`, the fmt pair
  `test/fmt/protocol.lang` and `protocol.canon`, and 3 proofs in
  `test/eval/checker.lang`.
- U1, the backfill laws: `test/eval/backfill-laws.lang` (Q-S6-12 (a)).
  `StreamBisim` is a program definition, because the CC rule refuses a
  cofamily with a value parameter. The table model has 10 sampled rows.
  L2: `twiceOnce` and `resumed`. L3: `writeKeep`, `rowsAfter` and the
  `ua` part (`movesRead`, `readsMove`). 7 `refl` proofs and 4 broken
  models.
- U2, the writer: `src/sql.c`, `src/sql.h`, the command `sql` in
  `src/main.c` and the code `SQL_NAME_LONG` (Q-S6-16 (a)). The writer
  makes `00-setup.sql` (Q-S6-1 (a)) with `evolve.progress`,
  `evolve.stage_done` and the `evolve.ddl` retry on `55P03`, 5 tries in
  all (Q-S6-5 (a), Q-S6-17 (a)). Each stage file gets the header (Q-S6-2
  (a)), the transaction shape and `lock_timeout` (Q-S6-6 (a)), the `\if`
  skip (Q-S6-18 (a)) and the SQL of its step, with quoted
  `evolve_P_NN_KIND` names (Q-S6-11 (a)). New texts: the `USAGE` text of
  `sql` (Q-S6-20 (a)) and the `INTERNAL` text for a stage with no step
  SQL (Q-S6-19 (a)).
- U3, the seams: the tenants table and row, the backfill table and the
  version registry with `seen_at` (Q-S6-3 (a), Q-S6-4 (a)); the guard
  `DO` blocks for `fleetAt`, `retired` and `backfillDone` (Q-S6-21 (a),
  Q-S6-22 (a)); the sync trigger, where C wins (Q-S6-8 (a)); the keyset
  backfill procedure (Q-S6-7 (a)); the CHECK path on PostgreSQL 15 only
  (Q-S6-9 (a)); the drop of an INVALID index (Q-S6-10 (a)). New
  regressions in `test/cubical.c`: 4 cases (`sql_put_quoted`,
  `sql_name`).
- U4, the goldens: `test/sql/ix.lang`, `lt.lang` and
  `long_name_of_forty_eight_bytes_for_sql_name_long.lang`, and 53 golden
  files in 9 directories `test/sql/NAME/` (Q-S6-13 (a), Q-S6-25 (a)). The
  procedure variables get the `evolve_` prefix (Q-S6-23 (a)), and the
  guards keep `IF NOT (COND)` (Q-S6-24 (a)). `test/gate.sh` checks the
  files and prints `sql: 9 golden protocols, 53 files`.
- U5, the mutants and the docs: OUTDIR must exist, else `IO` (Q-S6-26
  (a)). SPEC.md 6.3; PLAN 5.1, O4, O19 and the S6 row; the `README.md`
  status; this section.

### Gate counts against S5

| Count | S5 (550e73b) | S6 |
|---|---|---|
| examples | 12 | 12 |
| parse refusals | 10 | 11 |
| check refusals | 79 | 80 |
| evals | 202 | 218 |
| ir lowered | 12 | 12 |
| wasm modules | 16 | 16 |
| differential cases (0 mismatches) | 892 | 892 |
| fmt canonical, non-canonical pairs | 159, 7 | 165, 8 |
| json fixtures | 38 | 38 |
| diagnostics codes (fixture, no fixture) | 51 (36, 15) | 52 (36, 16) |
| golden reports | 6 | 6 |
| golden SQL protocols, files | none | 9, 53 |
| cubical checks, round trips | 52, 29 | 52, 29 |
| json writer regressions | 21 | 21 |
| report guard regressions | 4 | 4 |
| sql writer regressions | none | 4 |

The gate has 0 failures. The review, domain and diagnostic edit
regressions pass.

### Op values
51 Op values. S6 adds no Op. `src/front/eval.c` and the kernel do not
change.

### Mutants
Each mutant makes one change in `src/sql.c`, then `make -B` and the golden
SQL loop run on a copy of `test/sql/`. These 8 mutants are all KILLED.
Each FAIL line is a file text FAIL; no mutant changes the file names.

| Mutant | Site | Result |
|---|---|---|
| M1 | `put_stage`: the lock timeout is one more (`'2001ms'`) | KILLED by each stage file of the 9 protocols (44 files) |
| M2 | `seam_guard`: no `DO` block for a `fleetAt` guard | KILLED by fleet, long_name, lt, order, rename and strict (15 files) |
| M3 | `guard_backfill_done`: the C and S columns swap | KILLED by long_name, lt, order and rename (4 files) |
| M4 | `guard_fleet_at`: the stage of `fleetAt` is one more | KILLED by the 15 files of M2 |
| M5 | `guard_retired`: the guard reads the app `v3` | KILLED by fleet, long_name, lt, order, rename and strict (16 files) |
| M6 | `put_stage`: a bare stage gets `BEGIN`, `SET LOCAL` and `COMMIT` | KILLED by ix, long_name, lt, order, rename and strict (6 files) |
| M7 | `SETUP_SQL`: the retry SQLSTATE is `55P02` | KILLED by each `00-setup.sql` (9 files) |
| M8 | `seam_sync_function`: the trigger tests S first, so S wins | KILLED by long_name, lt and rename (3 files) |

### Open items from S6
- Q-S6-1 to Q-S6-26 are RULED (a) (USER 2026-10-29 and 2026-10-30).
  Q-S2b-1 to Q-S2b-8 stay open (PLAN section 11).
- No native PostgreSQL server run of the files and no `make check-pg`
  (Q-S6-14 (a), PLAN O19). The gate compares the text of the files; the
  staged review also executed the SQL in PostgreSQL WASM, below.
- The PostgreSQL 18 form `NOT NULL ... NOT VALID` (Q-S6-9 (a), PLAN O4).
- Two doc facts of the backfill and index stages are not cited in PLAN
  5.1 (PLAN O19).
- Each app instance must write `seen_at` (Q-S6-4 (a)). The writer does
  not make this code, and no test checks it.
### S6 review (2026-10-30)

Reviewed all 81 originally staged paths. Fixed seven findings:

- CRITICAL: a tenant identifier over 63 bytes could select another
  tenant's schema while recording progress under its full text. Every
  file now requires an exact existing schema name before any write.
- MEDIUM: temporary tables could receive a tenant's migration. The
  explicit search order keeps catalog built-ins first and the tenant
  ahead of temporary tables. Each table step
  verifies resolution against the exact tenant schema before its guards.
- MEDIUM: a same-named index on the wrong column could satisfy a stage.
  Index cleanup and completion now check the table, column, btree options
  and definition. Completion also requires a valid, ready, live index.
- MEDIUM: a key named `evolve_last_key` made the backfill ambiguous.
  The cursor has a block qualification and key references have aliases.
- MEDIUM: reversed `dropSync` arguments missed the installed trigger.
  Origin lookup now follows the domain's symmetric pair semantics.
- MEDIUM: long table and column identifiers were silently truncated.
  The writer now refuses them before writing any file, including a
  backfill's declared key. Registry text values retain their full names.
- MEDIUM: 100-stage protocols emitted `000-setup.sql`. Setup keeps its
  advertised `00-setup.sql` name independently of stage-number width.

Validation: full `make check` passed with 0 failures, 166 canonical
fixtures, 10 SQL protocols and 60 golden SQL files. The new C executable
`test/sql-writer.c` adds 13 protocol regressions to the gate. The original
writer failed its long table, column, key and setup-filename checks.
`test/sql/edges.lang` covers the colliding key and reversed sync pairs.

Exploratory PostgreSQL WASM execution (PGlite 0.5.13) completed all ten
golden protocols twice. Eighteen additional executions checked exact and
overlong tenants, missing schemas and tables, temporary-table shadowing,
multi-batch copying, matching and mismatched index definitions, and
INVALID-index recovery, catalog built-in precedence and tenant routine
placement. Rejected cases preserved the original objects
and did not record the rejected stage. This does not replace a native
PostgreSQL server run or concurrent-client validation (`PLAN` O19).

No CI weakening and no unresolved findings from this review.

## S7: the Wasm oracle writer (`langc wasm`)

S7 adds the Wasm oracle. `langc wasm PROG PROTOCOL -o OUT` checks the
program and writes a Wasm 1.0 (MVP) module that holds the compatibility
table of the protocol (PLAN 5.3): the exports `stages`, `apps`, `verdict`
and `guard`. An agent or a CI job with no C toolchain can read the
verdicts from it. The module is not the type checker (Q3). A differential
compares each module call with the guest folds `verdictCode` and
`guardFirst` under `langc eval`. SPEC.md 6.4 gives the command and SPEC.md
7.1 the encoding.

### Source
- No port. `src/oracle.c` (213 lines), `src/oracle.h`, `src/wasmbin.c`
  (58 lines) and `src/wasmbin.h` (46 lines) are new. S7 changes 13 files
  and adds 18 files: the 4 source files, `test/oracle-writer.c`,
  `test/eval/oracle.lang`, 6 files `test/oracle/NAME.lang` and 6 files
  `test/wasm/NAME.wasm`. It removes no file.

### Units
- U0, the byte writer and the runner (Q-S7-1 (a), Q-S7-2 (a)): the Wasm
  byte helpers (the LEB128 numbers, the names and the sized sections) move
  from `src/wasm.c` to `src/wasmbin.c`, so `src/wasm.c` and
  `src/oracle.c` use the same helpers. The 16 modules of `langc build` are
  byte equal before and after. `test/run-wasm.mjs` calls an entry with
  BigInt arguments, and again with Number arguments on a TypeError (an i32
  parameter). It prints an i32 result as an unsigned 32-bit decimal.
- U1, the guest side: `domain/domain.lang` adds the folds `verdictCode`
  (the cell byte of a `Verdict`, Q-S7-3 (a)), `halfOdd`, `guardWalk` and
  `guardFirst` (the first failed fact for a mask, Q-S7-4 (a)).
  `test/eval/oracle.lang` checks them: 36 new evals.
- U2, the writer and the command: `src/oracle.c`, `src/oracle.h`, the
  command `wasm` in `src/main.c` and the code `WASM_LIMIT` (no fixture).
  The module has one data segment at offset 0: the cells in the Report
  order, then the fact count of each stage (Q-S7-5 (a)). The USER ruled
  (2026-10-30) that `test/oracle-writer.c` checks `WASM_LIMIT` on a
  hand-made Report, because a checked program does not get to the bound:
  with the 1 GiB arena, `langc check` gives `OOM` at 18 backfilled
  columns (a stage of 19 facts) and at 12 retired apps; 17 columns (18
  facts) is the largest program found.
- U3, the goldens and the differential (Q-S7-6 (a), Q-S7-7 (a)): 6 golden
  modules `test/wasm/NAME.wasm` (189 to 214 bytes), one for each golden
  report, and 6 programs `test/oracle/NAME.lang` with the guest entries
  `verdict` and `guard`. The goldens have the cell codes 0 (50 cells), 2
  (13), 3 (1), 4 (3), 5 (4) and 9 (4). The codes 1, 6, 7 and 8 refuse the
  full protocol, so no golden has them. The USER ruled (2026-10-30) that
  `test/oracle-writer.c` checks the cell byte of each code 0 to 9 on a
  hand-made Report. Negative controls on the gate part (a wrong module
  for the calls, a changed program) are KILLED.
- U4, the mutants and the docs: the mutants M1 to M8 below. M7 lived on
  the module calls, so the gate adds 3 calls for each golden that must
  trap: `verdict APPS 0`, `verdict 0 STAGES` and `guard STAGES 0`
  (157 to 175 calls). SPEC.md 6.4 and 7.1; PLAN 5.3 and the S7 row;
  `docs/host/CAPABILITY.md` (the `src/wasm.c` lines of `ADD_CARRY` and
  `MUL_CARRY` after U0); the `README.md` status; this section.

### Gate counts against S6

| Count | S6 (68ee873) | S7 |
|---|---|---|
| examples | 12 | 12 |
| parse refusals | 11 | 11 |
| check refusals | 80 | 80 |
| evals | 218 | 254 |
| ir lowered | 12 | 12 |
| wasm modules | 16 | 16 |
| differential cases (0 mismatches) | 892 | 892 |
| oracle golden modules, calls (0 mismatches) | none | 6, 175 |
| oracle differential cases (0 mismatches) | none | 143 |
| fmt canonical, non-canonical pairs | 166, 8 | 173, 8 |
| json fixtures | 38 | 38 |
| diagnostics codes (fixture, no fixture) | 52 (36, 16) | 53 (36, 17) |
| golden reports | 6 | 6 |
| golden SQL protocols, files | 10, 60 | 10, 60 |
| cubical checks, round trips | 52, 29 | 52, 29 |
| json writer regressions | 21 | 21 |
| report guard regressions | 4 | 4 |
| sql writer regressions | 4 | 4 |
| sql protocol regressions | 13 | 13 |

The gate has 0 failures, and `build/oracle-writer` has no FAIL line. The
review, domain and diagnostic edit regressions pass. The 7 new fmt
programs are `test/eval/oracle.lang` and the 6 files `test/oracle/NAME.lang`.

### Op values
51 Op values. S7 adds no Op. `src/front/eval.c` and the kernel do not
change.

### Mutants
Each mutant makes one change in `src/oracle.c`, then `make -B`,
`build/oracle-writer` and the oracle part of `test/gate.sh`. These 8
mutants are all KILLED. Each mutant also changes the module bytes, so the
`cmp` with `test/wasm/NAME.wasm` gives 12 FAIL lines (6 goldens, 6
differential modules). The table gives the killer on the module calls.

| Mutant | Site | Result |
|---|---|---|
| M1 | `cell_byte`: a refused code is K + 2 | KILLED by `build/oracle-writer` (the code map) and 21 call mismatches |
| M2 | `cell_byte`: a runtime cell is 0 | KILLED by `build/oracle-writer` (the code map) and 4 call mismatches |
| M3 | the table: the cells in stage order | KILLED by 36 call mismatches |
| M4 | `body_guard`: the result is K, not K + 1 | KILLED by 21 call mismatches |
| M5 | `body_guard`: it tests bit N - 1 - K | KILLED by 4 call mismatches |
| M6 | `body_guard`: the loop stops at K + 1 >= N | KILLED by 17 call mismatches |
| M7 | `body_verdict`: an app out of range gives 0, not a trap | KILLED by 7 call mismatches (after the 3 trap calls; before, only by the `cmp`) |
| M8 | `body_stages`: the result is S - 1 | KILLED by 6 call mismatches |

### Open items from S7
- Q-S7-1 to Q-S7-8 are RULED (a) (USER 2026-10-30). Q-S2b-1 to Q-S2b-8
  stay open (PLAN section 11).
- No checked program gets to `WASM_LIMIT`, and no golden module has a
  cell with the codes 1, 6, 7 or 8. `test/oracle-writer.c` checks these
  on hand-made Reports only.
- The differential tests the masks below 2^min(k, 6) of a stage with k
  facts, because the guest fold on a natural number is linear in its
  value.

## S8: the agent kit (`langc mcp`, `explain-code`, `list-steps`, `SKILL.md`)

S8 adds the agent kit (PLAN 7). `langc explain-code CODE` gives the row of
a diagnostic code as one JSON line, and `langc list-steps` gives the 12
protocol steps, one JSON line each. `langc mcp` is a Model Context
Protocol server on stdin and stdout (revision 2025-11-25) with 5 tools:
`check`, `fmt`, `report`, `explain-code` and `list-steps`.
`skills/evolve-lang/SKILL.md` gives the verbs and 5 graded examples, and
the gate runs each example. The README table names the steps of each yes
row, and `AGENTS.md` gives the routing rule and the loop. SPEC.md 6.5, 6.6
and 6.7 give the commands.

### Source
- Port from quorum-lang 0c729b6 (PLAN 8): the MCP server shape (NDJSON,
  the lifecycle). `src/front/jsonin.c` (259 lines) and
  `src/front/jsonin.h` are copies, and `utf8_char_size` in
  `src/front/base.c` is a copy of quorum-lang `base.c` lines 109 to 123.
- New: `src/mcp.c` (245 lines), `src/mcp.h`, `src/codes.c` (193 lines),
  `src/codes.h`, `src/steps.c`, `src/steps.h`, `test/mcp.c`,
  `test/verbs.c`, `test/skill.c`, `test/readme.c`,
  `test/mcp/requests.jsonl`, `test/mcp/responses.jsonl`,
  `skills/evolve-lang/SKILL.md` and the 5 programs of `examples/agent/`.
  S8 changes 10 files and adds 20 files. It removes no file.

### Units
- U0, the verbs write to a FILE* (no behaviour change): `run_check`,
  `run_report` and `run_fmt` of `src/main.c` take the arguments `out` and
  `err`, so `langc mcp` can capture the output of a verb. `src/main.c` has
  no file-scope static that is not const. The gate counts are those of S7.
- U1, the MCP server (Q-S8-1 to Q-S8-6 (a), Q-S8-13 (a)): `src/mcp.c`
  reads one message on each line with a 1 MiB line cap, and `initialize`
  always gives the revision 2025-11-25. `src/main.c` gives the verbs to
  `mcp_serve` as the callback `McpRunTool` (Q-S8-6 (a)), so
  `build/diagnostics` and `build/sql-writer` link without `src/main.c`. A
  tool result has the stdout of the verb as item 1, the stderr as item 2
  when it is not empty, and `isError` (Q-S8-5 (a)). The goldens
  `test/mcp/requests.jsonl` and `test/mcp/responses.jsonl` and
  `build/mcp-test` (Q-S8-11 (a)). The mutants M1, M2, M4 and M9.
- U2, `explain-code` and `list-steps` (Q-S8-7 (a), Q-S8-8 (a), Q-S8-15
  (a)): the C tables `src/codes.c` (the 53 rows of
  `docs/DIAGNOSTICS.md`) and `src/steps.c` (the 12 `Step` constructors),
  the two commands and the two tools. The `McpRunTool` parameter `path` is
  now `arg`: the program path, the code of `explain-code`, or NULL for
  `list-steps` (Q-S8-15 (a)). `build/verbs-test` compares the tables with
  `docs/DIAGNOSTICS.md` and `domain/domain.lang`. The mutants M5 and M6.
- U3, the skill (Q-S8-9 (a), Q-S8-10 (a), Q-S8-11 (a)): the 5 programs
  of `examples/agent/`. Examples 1, 2 and 4 are copies of existing
  programs, and examples 3 and 5 are new: a column dropped too early and
  a stale read, both refused. `SKILL.md` has the verbs and, for each
  example, a program block and a json block (two fences, Q-S8-9 (a)).
  `build/skill-test` compares each program block with its file and each
  json block with `langc check --json`. `build/mcp-test` sends a `check`
  of each program and compares the reply with the command line. The
  mutants M3, M7 and M8.
- U4, the README, `AGENTS.md` and the docs (Q-S8-12 (a), Q-S8-14 (a)):
  the README table has a Steps column and a new yes row (an index build
  that does not block writes, `createIndexConcurrently`). The README gives
  the Prisma ORM 8 status (O7). `AGENTS.md` has the routing rule, the loop
  and the 12 verbs. `build/readme-test` checks the table and the verbs of
  `AGENTS.md`. SPEC.md 6.5 to 6.7; PLAN 8, the S8 row, O3 and O7
  CLOSED; this section. The mutant M10.

### Gate counts against S7

| Count | S7 (031ca60) | S8 |
|---|---|---|
| examples | 12 | 12 |
| parse refusals | 11 | 11 |
| check refusals | 80 | 80 |
| evals | 254 | 254 |
| ir lowered | 12 | 12 |
| wasm modules | 16 | 16 |
| differential cases (0 mismatches) | 892 | 892 |
| oracle golden modules, calls (0 mismatches) | 6, 175 | 6, 175 |
| oracle differential cases (0 mismatches) | 143 | 143 |
| fmt canonical, non-canonical pairs | 173, 8 | 173, 8 |
| json fixtures | 38 | 38 |
| diagnostics codes (fixture, no fixture) | 53 (36, 17) | 53 (36, 17) |
| golden reports | 6 | 6 |
| golden SQL protocols, files | 10, 60 | 10, 60 |
| mcp requests (0 mismatches), examples | none | 33, 5 |
| explain-code codes | none | 53 |
| list-steps steps | none | 12 |
| skill examples, outputs | none | 5, 5 |
| readme rows, steps named; agents verbs | none | 7, 12; 12 |
| cubical checks, round trips | 52, 29 | 52, 29 |
| json writer regressions | 21 | 21 |
| report guard regressions | 4 | 4 |
| sql writer regressions | 4 | 4 |
| sql protocol regressions | 13 | 13 |

The gate has 0 failures, and no line is a FAIL line. The 28 count lines of
S7 are the same, in the same order. The review, domain and diagnostic edit
regressions pass.

### Op values
51 Op values. S8 adds no Op. `src/front/eval.c` and the kernel do not
change.

### Mutants
Each mutant makes one change, then `make -B` of the binary and the test
that kills it. Each mutant is KILLED. After each mutant the file is
restored, `cmp` with its backup is equal, and the test passes again.

| Mutant | Site | Result |
|---|---|---|
| M1 | `src/mcp.c` `initialize`: the result echoes the revision of the client | KILLED by `build/mcp-test` (1 mismatch) |
| M2 | `src/mcp.c`: a notification gets a reply | KILLED by `build/mcp-test` (23 mismatches) |
| M3 | `src/mcp.c` `tools/call`: `isError` is always false | KILLED by `build/mcp-test` (6 mismatches: 4 goldens, 2 examples) |
| M4 | `src/mcp.c`: a number id is echoed as a string | KILLED by `build/mcp-test` (18 mismatches) |
| M5 | `src/codes.c` `code_find`: the next row | KILLED by `build/verbs-test` (53 mismatches) |
| M6 | `src/steps.c` `step_count`: one row less | KILLED by `build/verbs-test` (2 mismatches) |
| M7 | `SKILL.md`: the json block of example 2 says `12 of 13` | KILLED by `build/skill-test` (1 mismatch) |
| M8 | `src/mcp.c` `tools/call`: item 1 stops before `{"parsed"` | KILLED by `build/mcp-test` (8 mismatches) |
| M9a | `src/mcp.c`: no line cap (the buffer and the cap times 4) | KILLED by `build/mcp-test` (the long ping gets a reply) |
| M9b | `src/mcp.c`: the server stops after a long line | KILLED by `build/mcp-test` (the last ping gets no reply) |
| M9c | `src/mcp.c`: the cap is one byte less | KILLED by `build/mcp-test` (the ping of 1048576 bytes gets -32700) |
| M10 | `test/readme.c` `find_name`: an unknown name gives row 0 | KILLED by a README copy where row 2 names `deplo` (see below) |

M10 changes the gate check, not `langc`. The unchanged
`build/readme-test COPY` exits 1 with `FAIL: readme: row 2: deplo is not
a step of build/langc list-steps`. The mutant exits 0 on the copy, and it
also exits 0 on `README.md`.

### Open items from S8
- Q-S8-1 to Q-S8-15 are RULED (a) (USER 2026-10-30). O3 and O7 are
  CLOSED (PLAN section 10).
- The gate does not kill M10: `README.md` names only steps of `langc
  list-steps`, so the check of an unknown name runs only on the copy.
- The server keeps no session state. It answers `tools/call` also before
  `initialize`.
- S9 (PLAN 9): `docs/TOKEN-BASELINE.md` and its script.
