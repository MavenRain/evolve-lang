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
