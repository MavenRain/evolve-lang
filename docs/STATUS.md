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
| `fold` | F6 (the kit recursion) | admitted |
| `Eq`, `refl`, `symm`, `trans`, `cong`, `transport` | 2.2 `Path` | S2 renames `Eq` to `Path` |
| `Sum`, `inl`, `inr`, `either` | not in 2.2 (`Either E A` is not admitted) | stays; S2 decides (O12) |
| `Unit`, `unit` | not in 2.2 | stays; S2 decides (O12) |
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
  review.
- O13: `flagIf` keeps its kit name (it is not in the S1 drop list). The
  USER rules the name of `if`.
- O14: dead core paths. The Op values and the eval and lowering code for
  `List`, `Prod`, `filter` and `unfold` stay, but no program reaches them.
  S2 removes them.
- O15: the value printer and some check messages use the ASCII kit
  spelling (`->`, `fun x =>`, `Sigma (x : A) B`, `Nat`). The parser reads
  it back through the aliases.
- O16: `nil` is in the S1 list of dropped kit names (`NAME_UNKNOWN` with a
  hint). PLAN 3.2 admits `nil` and `snoc` as the `Chain` constructors. S3
  removes `nil` from that list when it adds `Chain`.
