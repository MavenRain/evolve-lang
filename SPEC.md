# evolve-lang specification (draft)

Status: draft before milestone M0. `evolve-lang` is a working name.

Fill each section. The quoted line under each heading says what to write
there. Delete the quoted line when the section is complete.

## 1. Purpose

> Write here: what one program describes, what the compiler checks, and what
> it writes. Name the host and the target. Say that the core types and core
> operations are only those of `design/DESIGN.md`.

evolve-lang is a language for <the domain>. Its type formers are F1 to F15 of
`formers/FORMERS.md`. Its core data types and core operations are only the
types and operations of `design/DESIGN.md` (the design).

## 2. Programs

A program is a UTF-8 file with a sequence of declarations. The surface is
Agda-like (Q7 (a), USER ruling 2026-10-25). Each operation has one
canonical form. `langc fmt FILE` prints the canonical text (section 2.6).

### 2.1 Declarations

A definition is a signature line and a clause line:

```
twice : (f : ℕ → ℕ) → ℕ → ℕ
twice f x = f (f x)
```

The clause starts with the name of the signature above it. If the names
are not the same, or if a clause has no signature, the parser gives
`PARSE_CLAUSE_NAME`. Each name after the clause name is a bare binder:
`f x y = t` is the same term as `f = λ x y → t`. The clause `f = λ x → t`
parses, and `fmt` prints it as `f x = t`. An annotated lambda
`λ (x : A) → t` is a different term, and `fmt` keeps it.

The domain file (`domain/domain.lang`) can also use these forms:

```
rec name : T
name x = t

postulate name : T

data Lot (size : ℕ) : Type where
  makeLot : (lotPrice : ℕ) → Lot size
```

- `rec` before a signature marks a recursive definition.
- `postulate` gives a name and a type with no definition.
- `data` declares a data type, with zero or more parameter groups. Each
  constructor is on its own line in the block. A constructor type must end
  in the data type applied to its parameters in order. If it does not, or
  if a field has no name, the parser gives `PARSE_CTOR_RESULT`. Each named
  Pi binder of a constructor type is a field: `(itemPrice : ℕ)` gives the
  field `itemPrice`.
- `codata` declares a cofamily (S2c). The block has one clause. The
  clause name is the corecursor, and the clause fields are the
  observation and the fields of the cofamily:
  `corec : (head : A) → (tail : Stream A) → Stream A`. Each field name is
  a destructor with an explicit type argument: `head ℕ s`, `tail ℕ s`.
  `corec A S h t s` gives the stream of the seed `s : S`. The parser takes
  more than one clause; the checker refuses it.

The compiler refuses these forms in a program: `data` with no type
argument (`REFUSE_DATA`, rule R2), `data` with a type argument that is not
a cell signature and `codata` that is not a cocell signature
(`REFUSE_CELL`, rule R1), `postulate` (`REFUSE_AXIOM`),
and `rec` or a reference to the same or a later definition (`REFUSE_REC`,
rule R3). It also refuses a declaration that uses a core name, a domain
name, the reserved word `Text` or a name that is already declared
(`REFUSE_NAME`). Thus a program cannot add a base type, an unproved fact
or general recursion. Recursion comes only from
`fold` (F6). The compiler writes the target; a program cannot.

`Fin` is a core name (S2d). `Fin n` is the Sigma
`(k : ℕ) × Path Bool (natLe (natAdd k 1) n) true`. A term of `Fin n` is
`pack k refl`, and `witness x` gives the `ℕ`. A message prints the Sigma,
not `Fin n`. S2d removed the kit names `Sum`, `inl`, `inr` and `either`:
they give `NAME_UNKNOWN` with the hint "use Option or a base sum".

`Glue`, `glue` and `unglue` are core formers (S2e).
`Glue A [ φ ↦ pack T e , ... ]` is a type: on each face, `e` is an
`Equiv T A`. `glue [ φ ↦ t , ... ] a` is a term of a Glue type, and
`unglue g` gives the `A` part of `g`. The kernel prelude
`src/front/prelude.lang` defines `isContr`, `fiber`, `isEquiv`, `Equiv`,
`idEquiv`, `ua` and `transpEquiv`. The checker reads it before the domain
file. Its names are core names: a declaration of one gives `REFUSE_NAME`.
`ua A B e` is a `Path (Type 0) A B`. The Kan rules transp_glue,
hcomp_glue and hcomp_univ (hcomp at `Type 0`) compute transport and
composition at these types. `langc ir` refuses a Glue value at run time
(`REFUSE_RUNTIME_GLUE`).

### 2.2 Grammar

`×` binds tighter than `→`. Both nest to the right.

```
decl   := 'postulate' NAME ':' term
        | 'rec'? NAME ':' term NAME NAME* '=' term
        | 'data' NAME group* ':' 'Type' 'where' ctor+
        | 'codata' NAME group* ':' 'Type' 'where' ctor+
ctor   := NAME ':' term
group  := '(' NAME+ ':' term ')'
term   := 'λ' (NAME | group)+ '→' term
        | prod ('→' term)?
prod   := group '→' term | group '×' prod | app ('×' prod)?
app    := atom atom*
atom   := NAME | NAT | 'Type' NAT | '(' term ')'
```

A NAME starts with an ASCII letter or `_`. The next characters are ASCII
letters, digits, `_` or `'`. The symbol `ℕ` is the name of the natural
numbers. A NAT is a decimal numeral from 0 to 2^64-1 (`LEX_NAT_RANGE` if
it is larger). The keywords are `rec`, `postulate`, `data`, `codata`,
`where`, `Type`, `fun` and `Sigma`. A non-ASCII character other than `→`, `λ`, `×`
and `ℕ` gives `LEX_CHAR`.

### 2.3 Spellings and ASCII aliases

The grammar uses the canonical spellings. The parser also reads the ASCII
aliases in this table. An alias gives the same term as its canonical form.
`langc check` gives `W_NONCANON` on the first declaration that is not in
canonical form, with the canonical text as `fix` (O14). `fmt` prints only
the canonical spellings.

| Term | Canonical spelling | ASCII alias |
|---|---|---|
| Pi | `(x : A) → B`, `A → B` | `->` for `→` |
| Lambda | `λ x → t`, `λ (x : A) → t` | `\` for `λ`; `fun x => t` |
| Sigma | `(x : A) × B`, `A × B` | `*` for `×`; `Sigma (x : A) B` |
| Natural numbers | `ℕ` | `Nat` |
| Universe | `Type n` | none |

The core names do not change: `ℕ` and `Nat` give the one core name `Nat`.
The value printer of `langc eval`, and some diagnostic messages, use the
ASCII spelling (`->`, `fun x =>`, `Sigma`, `Nat`). The parser reads that
text back through the aliases (O19).

### 2.4 Layout rule

The surface has no keyword at the start of a definition, and an
application continues across lines. Thus the parser uses the columns:

- A declaration starts at column 1.
- A token that is the first token on its line, at a column less than or
  equal to the stop column, ends the current term (as the end of the file
  does). At the top level, the stop column is 1. Thus a term that continues
  on the next line must start that line at column 2 or more.
- In a `data` block, the first constructor sets the constructor column c
  (more than 1). A token at the start of a line at column c starts the next
  constructor. A token at the start of a line at a column less than c ends
  the block.
- A column counts code points, not bytes. `→` is one column.

### 2.5 Comments

A comment starts with `--` and continues to the end of the line. `fmt`
never drops a comment. In canonical form, each comment is a full line:

- A comment above a declaration is directly above it, with no blank line.
- A comment after the last declaration comes after one blank line.
- `fmt` moves an end-of-line comment, or a comment inside a declaration, to
  its own line above that declaration. `langc check` gives `W_NONCANON` for
  that input.

### 2.6 Canonical layout (`langc fmt`)

`langc fmt FILE` parses the file and prints the canonical text on stdout
(exit 0). A lex or parse error prints the diagnostic (exit 1). `fmt` does
not type-check. The canonical text has these properties:

- One line for each signature, each clause and each constructor. A
  constructor line starts with two spaces. `fmt` does not wrap a line.
- One blank line between declarations. No blank at the end of a line. The
  file ends in one newline.
- Only the canonical spellings of section 2.3.
- Parentheses only where the grammar needs them: a Pi, a Sigma or a lambda
  in a Pi domain or on the left of `×`, and an argument that is not an
  atom. For example, `(ℕ → ℕ) → ℕ` keeps its parentheses.
- One lambda for adjacent binders: `λ x y → t`, and
  `λ (g : ℕ → ℕ) (x : ℕ) → t`.
- One binder for each Pi: `(a k : ℕ) → ℕ` prints as
  `(a : ℕ) → (k : ℕ) → ℕ`.
- A definition whose body is a chain of bare-binder lambdas prints as a
  clause with arguments.

`langc check` compares the input with the canonical text one region at a
time. A region is the text from the end of the last line of the previous
declaration to the end of the last line of this declaration. The first
region that is not the same as its canonical text gives `W_NONCANON`. The
end of the file is the last region.

## 3. Type formers

> Write here: only the deviations from the formers. Do not copy the forms.
> Give each former whose status in `formers/tcc-wasm.md` is not DONE, and the
> open item (section 9) that records it.

The type formers are F1 to F15 of `formers/FORMERS.md`. This language uses
the tcc-wasm column of the realization matrix. `formers/tcc-wasm.md` gives
the host form of each former.

| ID | Status on tcc-wasm | Effect on this language | Open item |
|---|---|---|---|
| | | | |

## 4. Structures

> Write here: the carriers of Monad (F5), fold and unfold (F6, F7) and
> filter (F8) that the design uses. Name each carrier that the host does not
> give.

- **Monad**: `pure`, `map`, `bind` on <carriers>.
- **Algebra**: `fold` on <carriers>. `unfold` on <carriers>, or the reason
  that there is no `unfold`.
- **Filterable**: `filter` on <carriers>.

## 5. Core types

> Write here (DOMAIN SLOT): only the types of `design/DESIGN.md`. Give each
> type its meaning in the design and its definition in host forms. Mark each
> definition that a probe fact forces with "probe-forced".

The domain is in `domain/domain.lang` (S3, Q-S3-31 (a)). A program has `n`
columns and `m` tables. A column is a `Fin n` and a table is a `Fin m`. `n`
counts each column name of the program, over all tables and all `addColumn`
steps. Each data type, constructor, field and definition name of the domain
is a core name.

| Type | Meaning (design section) | Definition |
|---|---|---|
| `ColType` | The type of a column (PLAN 4.1) | `data` with `text`, `bigint` (`domain/domain.lang:5-7`) |
| `Null` | The nullability of a column (PLAN 4.1) | `data` with `nullable`, `notNull` (`domain/domain.lang:9-11`) |
| `CState` | The state of the not-null CHECK of a column (PLAN 4.1) | `data` with `absent`, `notValid`, `valid` (`domain/domain.lang:13-16`) |
| `State n m` | The schema state (PLAN 4.1) | An iterated Sigma of the 9 fields of 5.1 (`domain/domain.lang:23-54`) |
| `Cols n` | A vector of columns: `k` columns at the codes 0 .. k - 1 (Q-S4-9 (a)) | `(k : ℕ) × (ℕ → Fin n)` (`domain/domain.lang:58-59`) |
| `App n` | An application version: the columns that it reads, the columns that it writes on update, the columns that it supplies on insert (PLAN 4.1) | `Cols n × Cols n × Cols n` (`domain/domain.lang:62-63`) |
| `Step n m s s'` | One step that changes the state `s` into `s'` (PLAN 4.1, J1 of 4.2) | `data` with two `State n m` indices and 12 constructors, one for each step word of section 6 (`domain/domain.lang:192-204`) |
| `Chain n m s0 sk` | A protocol: a chain of steps from `s0` to `sk` (PLAN 4.1) | `data` with `nil : Chain n m s s` and `snoc : Chain n m s s' → Step n m s' s'' → Chain n m s s''` (`domain/domain.lang:207-209`) |
| `Stage k` | A stage of a protocol with `k` steps (PLAN 4.1) | `Fin (k + 1)` (`domain/domain.lang:221-222`) |
| `Code` | A refusal code of PLAN 4.4 (S4) | `data` with `dropRequired`, `missingColumn`, `staleRead`, `unfilledRead`, `nullInsert`, `stepIndex`, `lockScan`, `renameInplace`, in the 4.4 order (`domain/domain.lang:225-233`) |
| `Fact n m` | A fact that only the run can show (PLAN 4.5, Q-S4-4 (a), Q-S5-3 (a)) | `data` with `backfillDone t c`, `retired (a : ℕ)` (`a` is the code of an app) and `fleetAt (s : ℕ)` (`s` is the stage before a barrier) (`domain/domain.lang:237-240`) |
| `Verdict n m` | The verdict of `compat` for one live version at one stage (PLAN 4.2) | `data` with `compatible`, `refused (code : Code)`, `runtime (fact : Fact n m)` (`domain/domain.lang:243-246`) |
| `LockMode` | The lock that a step takes on its table (PLAN 5.1) | `data` with `accessExclusive`, `shareRowExclusive`, `shareUpdateExclusive`, `rowExclusive`, `noLock` (`domain/domain.lang:249-254`) |
| `ScanClass` | The scan class of a step (PLAN 5.1) | `data` with `metadata`, `scan`, `batched`, `noScan` (`domain/domain.lang:257-261`) |
| `Live n` | The live versions at a stage: `k` apps at the codes 0 .. k - 1 (Q-S4-9 (a)) | `(k : ℕ) × (ℕ → App n)` (`domain/domain.lang:277-278`) |
| `Guard n m A` | The runtime obligations: a value of `A` that holds only if the fact of each `need` node holds (PLAN 2.2, 4.5, Q-S5-4 (a)) | `data` with `done (a : A)` and `need (f : Fact n m) (rest : Guard n m A)`, the free monad of the one operation `need` (`domain/domain.lang:421-423`) |
| `KeyRange` | The key range of one backfill batch: the low key, then the high key (PLAN 3.2, Q-S6-12 (a)) | `ℕ × ℕ` (`domain/domain.lang:502-503`) |

### 5.1 The State fields

Each field has an accessor with the same name (`owner n m s` and so on,
`domain/domain.lang:29-54`) and a `put` function (`domain/domain.lang:107-126`).

| Field | Type | Meaning | Steps that change it |
|---|---|---|---|
| `owner` | `Fin n → Fin m` | The table of each column | none |
| `key` | `Fin m → Fin n` | The key column of each table | none |
| `present` | `Fin n → Bool` | The column is in the schema | `addColumn`, `dropColumn` |
| `ty` | `Fin n → ColType` | The type of the column | `addColumn` |
| `null` | `Fin n → Null` | The nullability of the column | `addColumn`, `setNotNull` |
| `check` | `Fin n → CState` | The not-null CHECK of the column | `addColumn`, `dropColumn`, `addNotNullCheck`, `validate`, `dropCheck` |
| `sync` | `ℕ → ℕ → Bool` | A sync trigger from the first column to the second. It takes the column codes (`witness c`), so that a fold over `ℕ` can test each column (Q-S3-20 (a)) | `installSync`, `dropSync` |
| `filled` | `Fin n → Bool` | A backfill filled the column | `addColumn`, `backfill`, `dropColumn` |
| `link` | `ℕ → ℕ → Bool` | The two columns are the same field (S4, Q-S4-3 (a)). It takes the column codes, as `sync` does. J2.2 uses it. No step clears a link | `installSync`, `backfill` (both directions) |

### 5.2 The `table`, `app` and `protocol` forms

S3 adds three declaration forms (Q-S3-3 (a)). The words `table`, `app` and
`protocol` are keywords. The tokens `.` (in `t.c`) and `←` (in `backfill`)
are new; they have no ASCII alias.

```
decl   := ...
        | 'table' NAME '(' column (',' column)* ')'
        | 'app' NAME '(' (item (',' item)*)? ')'
        | 'protocol' NAME ('lockTimeout' NAT)? 'where' step+
column := NAME ':' NAME 'key'?
item   := ('reads' | 'writes' | 'inserts') NAME '.' NAME
step   := WORD NAME NAME NAME          (addColumn t c ty, installSync t c c',
                                        dropSync t c c', renameColumn t c c')
        | 'backfill' NAME NAME '←' NAME 'batch' NAT
        | WORD NAME                     (deploy a, retire a)
        | WORD NAME NAME                (dropColumn, addNotNullCheck, validate,
                                        setNotNull, dropCheck,
                                        createIndexConcurrently: t c)
```

- `table`: each table marks exactly one column `key` (Q-S3-24 (a)). The
  column type is a name (`text` or `bigint`).
- `app`: the items are `word t.c`. A word can repeat and can be absent. `()`
  is an app with no columns. Each `Cols` keeps the item order (Q-S3-26 (a)).
  In a program with no column (`n = 0`), an `app` gets `TYPE_REFL`
  (Q-S4-12 (a)); before S4 it checked.
- `protocol`: one step on each line, under the layout rule (2.4). The step
  words are the 12 words of section 6 and `renameColumn`, which the checker
  refuses (`REFUSE_RENAME_INPLACE`).
- `lockTimeout NAT` (S6, Q-S6-6 (a)): the PostgreSQL `lock_timeout` of the
  protocol, in milliseconds, for the SQL writer. The value is from 1 to
  2147483647; another value gives `PARSE_EXPECT` (Q-S6-15 (a)). With no
  clause, the value is 2000. `lockTimeout` is a word, not a keyword.
  `langc fmt` keeps the clause.

The elaboration gives these definitions (Q-S3-23 (a)). An unknown name in a
form gives `NAME_UNKNOWN` with the name as written (Q-S3-25 (a)).

| Form | Definitions |
|---|---|
| `table users (...)` | `users : Fin m`, and `users_name : Fin n` for each column `name` |
| `app v1 (...)` | `v1 : App n` |
| `protocol rename where` with `k` steps | the stages `rename_0` .. `rename_k : State n m`, and `rename : Chain n m rename_0 rename_k` |

`rename_i` is the state after step `i`. For `deploy` and `retire`,
`rename_i` is `rename_{i-1}` (Q-S3-27 (a)). Stage 0 (Q-S3-22 (a)): a
`table` column is present, has its type, is filled and has CHECK absent; the
`key` column is notNull and the other columns are nullable. A column that
only an `addColumn` step names is absent, has the type of the `addColumn`,
is nullable, has CHECK absent and is not filled. There is no sync.

## 6. Core operations

> Write here (DOMAIN SLOT): only the operations of `design/DESIGN.md`. Give
> each operation its type and its meaning in the design.

The operations are the 12 step words of a `protocol`. Each step word is a
constructor of `Step n m` (`domain/domain.lang:192-204`). A step with a
`pre` function takes a witness `Path Bool (pre ...) true`, and its target is
the `post` function of its source (J1 of PLAN 4.2, Q-S3-2 (a), Q-S3-21 (a)).
The `pre` and `post` functions are at `domain/domain.lang:131-188`. "`c` in
`t`" means that `c` is present and `owner c = t` (`inTbl`).

| Operation | Type | Meaning: pre (must be true) | Meaning: target (post) |
|---|---|---|---|
| `addColumn t c ty` | `Step n m s (postAddColumn n m s t c ty)` | `owner c = t`; `c` is not present | `c` present, type `ty`, nullable, CHECK absent, not filled |
| `installSync t c c'` | `Step n m s (postInstallSync n m s t c c')` | `c` in `t`; `c'` in `t`; `c ≠ c'`; no sync `c c'` | sync `c c'` and sync `c' c` |
| `dropSync t c c'` | `Step n m s (postDropSync n m s t c c')` | sync `c c'` | no sync `c c'` and no sync `c' c` |
| `backfill t c ← c' batch k` | `Step n m s (postBackfill n m s t c c' k)` | `c` in `t`; `c'` in `t`; `c ≠ c'`; `1 ≤ k` | `c` filled |
| `deploy a` | `Step n m s s` | none (Q-S3-4 (a)) | the same state |
| `retire a` | `Step n m s s` | none (Q-S3-4 (a)) | the same state |
| `dropColumn t c` | `Step n m s (postDropColumn n m s t c)` | `c` in `t`; `c ≠ key t`; no sync from `c` or to `c` | `c` not present, CHECK absent, not filled |
| `addNotNullCheck t c` | `Step n m s (postAddNotNullCheck n m s t c)` | `c` in `t`; CHECK absent | CHECK notValid |
| `validate t c` | `Step n m s (postValidate n m s t c)` | CHECK notValid | CHECK valid |
| `setNotNull t c` | `Step n m s (postSetNotNull n m s t c)` | `c` in `t`; nullable | notNull |
| `dropCheck t c` | `Step n m s (postDropCheck n m s t c)` | CHECK not absent | CHECK absent |
| `createIndexConcurrently t c` | `Step n m s s` | `c` in `t` | the same state (no index field, Q-S6-10 (a)) |

`deploy` and `retire` change the live set, not the schema (PLAN 4.1). Each
app of the program that the protocol does not `deploy` is live at stage 0;
`deploy a` adds `a` and `retire a` removes it (Q-S4-2 (a)). `setNotNull` with
no `valid` CHECK is J5 (`REFUSE_LOCK_SCAN`), not J1.

The chain operations (`domain/domain.lang:213-218`):

| Operation | Type | Meaning (design section) |
|---|---|---|
| `compose n m s s1 s2 a b` | `Chain n m s s1 → Chain n m s1 s2 → Chain n m s s2` | `a`, then `b` (PLAN `_⨾_`, Q-S3-12 (a)) |
| `length n m s s1 a` | `Chain n m s s1 → ℕ` | The number of steps of `a` |

The checker operations (S4, `domain/domain.lang:266-416`). In each type,
`a : Chain n m s0 s`, `h : Step n m s s1`, `v : App n`, `live : Live n`,
`lives : ℕ → Live n` (the live set at each stage) and `cols : Cols n` (the
columns of the program).

| Operation | Type | Meaning (design section) |
|---|---|---|
| `lockOf n m s s1 h` | `LockMode` | The lock of the step (PLAN 5.1, Q-S4-6 (a)). `dropSync` is `accessExclusive` (F-U0-2 (a)) |
| `scanOf n m s s1 h` | `ScanClass` | The scan class of the step (PLAN 5.1). `setNotNull` is `scan` when the CHECK of the column is not `valid` at `s` |
| `isBarrier n m s s1 h` | `Bool` | The step is `deploy` or `retire` (PLAN 4.3) |
| `segments n m s0 s a i` | `ℕ` | J3: the segment of stage `i`, the number of barrier steps in steps 1 .. i |
| `phase n m s0 s a j` | `ℕ` | The backfill phase of the column with the code `j` at the end of `a`: 0 no backfill, 1 backfilled, 2 at the first barrier after the backfill, 3 after that barrier (Q-S4-4 (a), Q-S4-10 (a)) |
| `fresh n m s live c` | `Bool` | J2.2: each column that a live app writes is not linked to `c`, or is `c`, or the app also writes `c`, or a sync joins it and `c` (Q-S4-3 (a)) |
| `compat n m s0 s a live cols v` | `Verdict n m` | J2: the verdict of `v` at the end of `a` (Q-S4-10 (a), Q-S4-11 (a)). The first clause that fails gives its code (Q-S4-1 (a)): J2.1 each read, write and insert is present; J2.2 each read is fresh; J2.3 each read that the protocol adds is filled; J2.4 each column that needs a value on an insert gets one. A read that the protocol adds, in phase 1 or 2, gives `runtime (backfillDone t c)` |
| `isRefused n m v`, `isAccessExclusive l`, `isScan c` | `Bool` | Tests of one constructor |
| `usesCol n v c` | `Bool` | `v` reads, writes or inserts `c` |
| `stageOk n m s0 s a live cols` | `Bool` | L1 at one stage: `compat` refuses no live app |
| `accepts n m s0 s a lives cols` | `Bool` | L1: `stageOk` at each stage of `a` (PLAN 6.1). These are the `compat` terms that the C driver evaluates (J4) |
| `dropHits n m s s1 h v`, `noDrops n m s s1 a v` | `Bool` | The step drops a column that `v` uses; no step of `a` drops a column that `v` uses |
| `dropSafe n m s0 s a lives` | `Bool` | L4: no app that is live at a stage uses a column that a step at or before the stage drops |
| `lockSafe n m s0 s a` | `Bool` | L6: no step of `a` is `scan` under `accessExclusive`. These are the `lockOf` and `scanOf` terms that the C driver evaluates (J5) |

`accepts` and `dropSafe` take the live sets as an argument (Q-S4-16 (a)): the
C driver makes them from the `deploy` and `retire` steps. A program proves a
decision on its protocol by `refl` (PLAN 6.1, Q-S4-7 (a)).

L2 and L3 (PLAN 6.1, Q-S6-12 (a)) are not domain operations.
`test/eval/backfill-laws.lang` states them on the rename protocol as
decisions over a table model of 10 rows, and proves each decision by
`refl`. The batch size is the batch of the `backfill` step of the protocol.
L2 is `twiceOnce` (the backfill run two times gives the table of one run)
and `resumed` (the run from the ledger point, after the first batch, gives
the table of the run with no stop). `ledgerBisim` proves that the plan from
the ledger point is bisimilar to the tail of the full plan. L3 is
`writeKeep` (at the `installSync` stage, each write keeps its row synced),
`rowsAfter` (after the backfill, each row is synced) and `readsMove` (the
transport along `ua` of the row swap moves the read of `v1` to the read of
`v3`; `movesRead` is the same for each row). `Stream`, `StreamBisim` and
the table model are program definitions in the test program. `StreamBisim`
is the pointwise form, because the CC rule refuses an indexed cofamily. The
broken models `addTwice`, `skipResume`, `writeNoSync` and `lateFill` give
`false`.

J2.3 includes columns dropped and added again under the same name:
`needsFill` tracks additions through the chain, and `filled` is cleared by
`addColumn`. The `phase` decision follows the most recent backfill, so a
later backfill starts a new `backfillDone` runtime obligation.

The runtime obligations (S5, `domain/domain.lang:418-498`). In each type,
`g : Guard n m A`, `f : Fact n m`, `e : Fact n m`, `apps : Live n` (each app
of the program in app order: the code of an app is its index) and `liveAt :
ℕ → ℕ → Bool` (`liveAt i j` is true when the app with the code `j` is live
at stage `i`).

| Operation | Type | Meaning (design section) |
|---|---|---|
| `guardBind n m A B g k` | `Guard n m B` | Gives the value of `g` to `k : A → Guard n m B`. The `need` nodes of `g` stay in front |
| `guardSize n m A g` | `ℕ` | The number of `need` nodes of `g` |
| `sameFact n m f e` | `Bool` | `f` and `e` are the same fact |
| `guardHas n m A g f` | `Bool` | `g` has a `need f` node |
| `stageGuard n m s0 s a live apps liveAt cols` | `Guard n m Unit` | The obligations of stage `i`, the end of `a` (`i` is `length a`), in the order of Q-S5-4 (a): `fleetAt (i - 1)` when step `i` is a barrier; then every runtime read of each live app, in app and read order; then any remaining phase-2 backfill, in column order; then the necessary retirements, in app order. Each fact is in the guard one time (Q-S5-8 (a)). `live` is the live set at `i`, as for `compat` |

The helpers `needNew`, `runtimeNew`, `eachApp`, `barrierGuard`,
`runtimeReads`, `barrierFills`, `addLive`, `historyLive` and `retiredGuards`
build `stageGuard`. The matrix keeps the first runtime fact of an app,
while its stage guard includes every distinct runtime read. Every backfill
in phase 2 is guarded at its first barrier, even if no live app reads it
yet: phase 3 assumes that obligation has been discharged. A retired app
needs a registry guard if its accesses fail with the historical writers
present, or if adding it back makes any current live app fail. Historical
apps are those live at any stage through `i`; future apps are excluded.
This covers write-only retired apps and interactions between retired apps.

The tests are in `test/eval/guard.lang` (17 evals): the
monad laws of `guardBind` by `refl`, the guard at each stage of the
protocol `widen`, and the CM twin `Pending` with the closed fact type
`Fact 3 1`. Four C regressions in `test/cubical.c` cover multiple
backfilled reads, a retired writer protecting a live reader, an unrelated
first barrier and interactions between retired apps. `Pending` takes the
type argument `A` because a program `data`
with no type argument is `REFUSE_DATA` (rule R2).

### 6.1 Refusal codes of the domain

`langc check` refuses a protocol with these eight codes. The texts are in
`docs/DIAGNOSTICS.md`. At each step, in chain order, the checker runs J1,
then for `dropColumn` `REFUSE_DROP_REQUIRED` against the live set, then J5,
then `compat` for each version that is live at the stage, in app
declaration order (Q-S4-1 (a)). The first verdict that is not `compatible`
or `runtime` refuses. At stage 0 the checker runs `compat` for each live
version before the first step.

| Code | Meaning | Fixture |
|---|---|---|
| `REFUSE_STEP_INDEX` | A protocol step is not legal at its source stage, because a conjunct of the `pre` function of the step is false. The message gives the step number, the step, the source stage, the first false conjunct and the arguments of the `pre` function. `expected` is the false conjunct and `found` is `false`. The span is the step word. | `test/json/REFUSE_STEP_INDEX.lang` |
| `REFUSE_RENAME_INPLACE` | A protocol has a `renameColumn` step. A rename in place breaks each live old version at once (PLAN 3.3). Use the five-stage protocol of examples/rename. `found` is `renameColumn`. The span is the step word. The checker refuses the step before the names resolve (Q-S3-29 (a)). | `test/json/REFUSE_RENAME_INPLACE.lang` |
| `REFUSE_DROP_REQUIRED` | A `dropColumn t c` step drops a column that a live version reads, writes or inserts (J4). The hint is "retire V before stage N". | `test/json/REFUSE_DROP_REQUIRED.lang` |
| `REFUSE_MISSING_COLUMN` | A live version reads, writes or inserts a column that is not present at a stage (J2.1). | `test/json/REFUSE_MISSING_COLUMN.lang` |
| `REFUSE_STALE_READ` | A live version reads a column that another live version updates through a different column, and no sync trigger joins the two columns (J2.2). | `test/json/REFUSE_STALE_READ.lang` |
| `REFUSE_UNFILLED_READ` | A live version reads a column that the protocol adds, and no earlier backfill fills it (J2.3). | `test/json/REFUSE_UNFILLED_READ.lang` |
| `REFUSE_NULL_INSERT` | A live version inserts into a table, and a not-null or CHECK column of that table gets no value from the insert or from a sync trigger (J2.4). | `test/json/REFUSE_NULL_INSERT.lang` |
| `REFUSE_LOCK_SCAN` | A step scans its table under an ACCESS EXCLUSIVE lock (J5), for example `setNotNull` with no valid CHECK before it. | `test/json/REFUSE_LOCK_SCAN.lang` |

Tests: each step word with a `pre` function has one check refusal with a
false `pre` (`test/check/step-*.lang`, 9 files), and `test/check/rename-inplace.lang`
refuses a `renameColumn` step. `test/eval/steps.lang` renames `users.name`
to `users.display_name` in 11 steps and reads one fact of the stage after
each step. Each S4 code has one check refusal; a recreated-column regression
brings `test/check/j4-*.lang` to 7 files. `test/check/rename-drop-early.lang` is the demo variant with no
`retire v1`: `REFUSE_STALE_READ` at stage 5 (`dropSync`, Q-S4-13 (a)).
`examples/rename.lang` is the demo (PLAN 4.6).

### 6.2 The compatibility report (`langc report`)

`langc report PROG PROTOCOL` checks the program PROG and writes one JSON
document for the protocol PROTOCOL on stdout (`docs/PLAN.md` 5.2). The
keys are in this order:

- `evolve`: the format version, 1.
- `protocol`: the name PROTOCOL.
- `stages`: one entry for each stage, with `index`, `steps` (`[]` at stage
  0), `lock`, `scan` (`null` at stage 0) and `guards` (each with `fact` and
  `guard`: the facts of `stageGuard`).
- `apps`: the apps of the program, in declaration order.
- `segments`: each run of stages with the same live set, with `stages` and
  `live`.
- `matrix`: one row for each app at each stage, in app order and then
  stage order, with `app`, `stage`, `live` and `verdict`. A `refused` row
  also has `code` and `fix`. A `runtime` row also has `fact` and `guard`.
- `progress`: the `check --json` progress object.

The error paths:

- PROG does not pass `langc check`: `report` writes the `check --json`
  lines and the exit is 1. The exit is 2 when PROG cannot be read.
- PROG has no protocol PROTOCOL: `USAGE`, and the exit is 2.
- PROG is not in canonical form: `report` writes the `W_NONCANON` line as
  text on stderr. The document on stdout does not change and the exit is 0
  (Q-S5-11 (a)).

Tests: `test/gate.sh` runs `report` on each `test/report/NAME.json` and
compares the document byte for byte. The program is
`test/report/NAME.lang`, or `examples/NAME.lang` when there is no such
file (Q-S5-7 (a)).

### 6.3 The SQL protocol (`langc sql`)

`langc sql PROG PROTOCOL OUTDIR` checks the program PROG and writes the
PostgreSQL files of the protocol PROTOCOL in the directory OUTDIR
(`docs/PLAN.md` 5.1). OUTDIR must exist (Q-S6-26 (a)). The command writes
nothing on stdout.

The files (Q-S6-1 (a), Q-S6-2 (a)):

- `00-setup.sql` is stage 0. Each stage I from 1 to K has the file
  `NN-WORD.sql`: NN is I with 2 digits (more digits when K is more than
  99), and WORD is the step word of the stage.
- Line 1 of each file is `-- evolve 1; source fnv1a64 HEX; stage I of K:
  STEP`. `1` is the format version, HEX is the FNV-1a 64 hash of the
  source text of PROG, and STEP is the step of the stage. In
  `00-setup.sql`, the line ends with `stage 0 of K: setup`.

The run (Q-S6-3 (a)): `psql -v tenant=NAME -f FILE` for each file, in
stage order. The tables of the tenant NAME are in the schema NAME. Run
`00-setup.sql` one time for each tenant: it adds the tenant row and the
stage 0 progress row. Every file first requires an existing schema whose
name exactly equals NAME as text. An absent or overlong tenant name stops
with `EVOLVE_TENANT NAME` before any migration or ledger write. Quoting
alone does not prevent PostgreSQL from truncating an identifier.

`00-setup.sql` makes the schema `evolve` with the tables `progress`,
`tenants`, `backfill` and `app_versions` (`app`, `live`, `changed_at`,
`seen_at`), and the functions `evolve.stage_done` and `evolve.ddl`.
`evolve.ddl` runs one DDL statement. When the statement stops on a lock
timeout (SQLSTATE `55P03`), the function waits 1 s and tries again, 5
tries in all (Q-S6-5 (a), Q-S6-17 (a)).

Each file starts with `\set ON_ERROR_STOP on`, sets `evolve.tenant`,
checks the exact schema name and sets `search_path TO pg_catalog,
:"tenant", pg_temp`. Generated function and procedure definitions and
backfill calls explicitly name the tenant schema. A table step also checks that its unqualified table
name resolves to the table in the exact tenant schema. An absent table or
a different resolution stops with `EVOLVE_TABLE TABLE`, before guards or
migration writes. This prevents a temporary or catalog relation from
receiving the tenant's migration. Each
stage file then sets the lock
timeout and reads the ledger: `SELECT NOT evolve.stage_done(P, I) AS run
\gset evolve_` and `\if :evolve_run`. When the ledger has the stage, the
file skips the step (Q-S6-18 (a)). In the `\if` block, the file runs the
guards of the stage, then the step, then it adds the progress row.

- A transaction stage: `BEGIN; SET LOCAL lock_timeout = 'Nms';` at the
  start and `COMMIT;` at the end (Q-S6-6 (a)).
- A bare stage (`backfill`, `createIndexConcurrently`): `SET lock_timeout
  = 'Nms';`, with no `BEGIN` and no `COMMIT`. The backfill procedure
  commits each batch, and `CREATE INDEX CONCURRENTLY` cannot run in a
  transaction block (Q-S6-10 (a)).
- N is the `lockTimeout` of the protocol (5.2), or 2000 when the protocol
  has no `lockTimeout` clause.

The steps with generated SQL objects (P is the protocol name and NN the
stage):

- `installSync T C S`: one `BEFORE INSERT OR UPDATE` trigger with the
  function `evolve_P_NN_sync` keeps C and S equal. When an UPDATE changes
  the two columns, C wins (Q-S6-8 (a)). `dropSync` can name the pair in
  either order and drops the objects of the most recent matching install.
- `backfill T C ← S batch B`: the procedure `evolve_P_NN_fill` copies S
  to C in key order, B rows in each batch, and commits after each batch.
  The table `evolve.backfill` keeps the next key, so a new run starts
  after the last batch that committed (Q-S6-7 (a)).
- `addNotNullCheck`: the constraint `evolve_P_NN_nn`, `CHECK (C IS NOT
  NULL) NOT VALID`. The writer uses this PostgreSQL 15 path only, not the
  PostgreSQL 18 form `NOT NULL ... NOT VALID` (Q-S6-9 (a), `docs/PLAN.md`
  O4).
- `createIndexConcurrently`: the index `evolve_P_NN_idx`. When an INVALID
  index with this name exists (a failed run), the stage drops it with
  `DROP INDEX CONCURRENTLY` and builds it again. Before cleanup, any
  existing object with this name must be a nonunique, single-column
  btree index of the intended table and column, with the default column
  options and no expression, predicate or included column. A mismatch
  stops with `EVOLVE_INDEX NAME`. After creation the same check also
  requires a valid, ready, live index before recording stage completion.
- `deploy A` and `retire A` write the row of A in `evolve.app_versions`:
  `deploy` sets `live`, `retire` clears it. When `live` changes,
  `changed_at` changes too (Q-S6-4 (a)).

The writer quotes each name (Q-S6-11 (a)). A generated SQL object name or
a table or column name used by the protocol of more than 63 bytes gives
`SQL_NAME_LONG` before any file is written (Q-S6-16 (a)). App and protocol
names stored as registry text are not SQL identifiers. The backfill procedure has the
variables `evolve_last_key`, `evolve_next_text` and `evolve_finished`
(Q-S6-23 (a)). SQL expressions qualify the key columns and the local
cursor with the block label `evolve_fill`, so user columns can have these
names too.

The guards are the facts of `stageGuard` (6.2, `docs/PLAN.md` 4.5). Each
guard is a `DO $evolve$ ... $evolve$` block. When the condition is false,
the block does `RAISE EXCEPTION 'EVOLVE_GUARD %', 'FACT' USING DETAIL =
'GUARD'` (SQLSTATE `P0001`, Q-S6-21 (a), Q-S6-24 (a)). FACT is the fact
(for example `fleetAt 3`), and GUARD is its `guard` text of 6.2.

- `fleetAt N`: each row of `evolve.tenants` has a progress row with stage
  N or a later stage.
- `retired A`: the `app_versions` row of A is not live and has no
  `seen_at` after `changed_at`.
- `backfillDone T C`: the backfill stage is done, and no row of T has C
  null and the source column not null.

The app contract (Q-S6-4 (a)): each instance of an app writes `seen_at` of
the row of its app in `evolve.app_versions`. The writer does not make this
code. Without it, the `retired` guard cannot see an instance that still
runs.

The exit codes:

- 0: the writer wrote all the files.
- 1: PROG does not pass `langc check` (the `check` diagnostics as text on
  stderr), or a name is too long (`SQL_NAME_LONG`; the writer writes no
  file).
- 2: `USAGE` (for example, PROG has no protocol PROTOCOL), `IO` (for
  example, OUTDIR does not exist or the writer cannot write a file), `OOM`
  or `INTERNAL`.

Tests: `test/gate.sh` runs `sql` for each directory `test/sql/NAME/`, with
NAME as the protocol. The program is `test/sql/NAME.lang`, or
`test/report/NAME.lang`, or `examples/NAME.lang` (the first file that
exists, Q-S6-13 (a), Q-S6-25 (a)). The run must write the same file names,
and each file must be equal to its golden file byte for byte. The staged
review executed the files in PostgreSQL WASM (`docs/STATUS.md`, S6 review).
A native server run remains open (Q-S6-14 (a), `docs/PLAN.md` O19).

### 6.4 The Wasm oracle (`langc wasm`)

`langc wasm PROG PROTOCOL -o OUT` checks the program PROG and writes the
Wasm 1.0 (MVP) module of the protocol PROTOCOL in the file OUT
(`docs/PLAN.md` 5.3). The module holds the compatibility table of the
protocol: the verdict of each matrix row of 6.2 and the guard fact count of
each stage. Section 7 gives the encoding. The writer reads the result of
the check. It does not decide a verdict, and the module is not the type
checker (Q3). The command writes nothing on stdout.

The error paths are those of `sql` (6.3):

- PROG does not pass `langc check`: `wasm` writes the `check` diagnostics
  as text on stderr and the exit is 1. The exit is 2 when PROG cannot be
  read.
- PROG has no protocol PROTOCOL, or the arguments are not `PROG PROTOCOL
  -o OUT`: `USAGE`, and the exit is 2.
- A stage of the protocol has more than 32 guard facts: `WASM_LIMIT`, and
  the exit is 2. The guard mask is one i32 (section 7).
- PROG is not in canonical form: `wasm` writes the `W_NONCANON` line as
  text on stderr. The module does not change and the exit is 0.

The writer makes the table on the arena before it opens OUT. Thus after
`WASM_LIMIT`, `OOM` or `INTERNAL` it does not write OUT.

The exit codes:

- 0: the writer wrote the module.
- 1: PROG does not pass `langc check`.
- 2: `USAGE`, `IO` (for example, the writer cannot write OUT),
  `WASM_LIMIT`, `OOM` or `INTERNAL`.

`WASM_LIMIT` is a bound that a checked program does not get to. With the
1 GiB arena, `langc check` gives `OOM` at 18 backfilled columns (a stage
of 19 facts) and at 12 retired apps. 17 backfilled columns (a stage of 18
facts) is the largest program found (`docs/STATUS.md`, S7).

Tests:

- `test/gate.sh` runs `wasm` on the program of each golden report
  `test/report/NAME.json` (the program is found as in 6.2), with NAME as
  the protocol (Q-S7-7 (a)). The module must be equal to
  `test/wasm/NAME.wasm` byte for byte, and `wasm-validate` must accept it.
  Then `test/run-wasm.mjs` calls the module: `stages`, `apps`, `verdict A
  I` for each matrix row, and for each stage I, `guard I 0` (1 when the
  stage has a fact, else 0) and `guard I 4294967295` (0). Each result must
  agree with NAME.json. The calls `verdict APPS 0`, `verdict 0 STAGES` and
  `guard STAGES 0` must trap. The gate prints `oracle: 6 golden modules,
  175 calls, 0 mismatches`.
- The differential (Q-S7-6 (a)): `test/oracle/NAME.lang` is the program
  NAME with the guest entries `verdict` and `guard` (section 7). `langc
  wasm` on it gives `test/wasm/NAME.wasm` too. For each matrix cell and
  for each mask below 2^min(k, 6) of a stage with k facts, `langc eval`
  of the same call must give the result of the module. The gate prints
  `oracle differential: 143 cases, 0 mismatches`.
- `build/oracle-writer` (`test/oracle-writer.c`) checks `WASM_LIMIT` (33
  facts) and the largest mask (32 facts) on a hand-made Report. It also
  checks the cell byte of each code 0 to 9 on a hand-made Report. The codes
  1, 6, 7 and 8 refuse the full protocol, so no checked program has a cell
  with them, and no golden module can have them (USER ruling 2026-10-30).

## 7. Target and instance encoding

An instance is a top-level definition whose type is a data type (F14).

### 7.1 The Wasm oracle module

`langc wasm` (6.4) writes a Wasm 1.0 (MVP) module with no imports
(`src/oracle.h`, `src/oracle.c`; the byte helpers are in `src/wasmbin.c`).
The module holds no term and no proof of the program: only the cell bytes
and the fact counts. The sections, in this order:

- type: two types, `() -> i32` and `(i32, i32) -> i32`.
- function: the 4 functions of the exports.
- memory: one memory with no maximum. Its pages are the table length
  divided by 65536, rounded up. The module does not export the memory.
- export: the 4 functions, in the order below (Q-S7-2 (a)).
- code: the 4 bodies.
- data: one active segment of memory 0 at offset 0, the table.

The table (Q-S7-5 (a)): A is the app count and S the stage count of the
protocol. The bytes 0 to A * S - 1 are the cells, in the Report order: the
cell of app a at stage i is the byte a * S + i. Then the byte A * S + i
is the guard fact count of stage i. The table length is A * S + S, and it
is 1 or more (each protocol has stage 0).

The cell byte (Q-S7-3 (a)):

| Byte | Verdict |
|---|---|
| 0 | `compatible` |
| 1 | `refused`, `REFUSE_DROP_REQUIRED` (`dropRequired`) |
| 2 | `refused`, `REFUSE_MISSING_COLUMN` (`missingColumn`) |
| 3 | `refused`, `REFUSE_STALE_READ` (`staleRead`) |
| 4 | `refused`, `REFUSE_UNFILLED_READ` (`unfilledRead`) |
| 5 | `refused`, `REFUSE_NULL_INSERT` (`nullInsert`) |
| 6 | `refused`, `REFUSE_STEP_INDEX` (`stepIndex`) |
| 7 | `refused`, `REFUSE_LOCK_SCAN` (`lockScan`) |
| 8 | `refused`, `REFUSE_RENAME_INPLACE` (`renameInplace`) |
| 9 | `runtime` |

The bytes 1 to 8 are the `Code` constructors in the domain order. A cell
that is not live keeps the code of its matrix row (Q-S5-9 (a)).

The exports. Each argument and each result is an i32:

- `stages()`: S.
- `apps()`: A.
- `verdict(app, stage)`: the cell byte of the app at the stage.
- `guard(stage, facts)`: the first failed fact (Q-S7-4 (a)). Bit K of
  FACTS (bit 0 first) is 1 when fact K of the stage guards holds. N is the
  fact count of the stage. The result is K + 1 for the lowest K below N
  with bit K 0, else 0. The bits at K >= N have no effect. Thus `guard(I,
  0)` is 1 when stage I has a fact, else 0.
- An app or a stage out of range traps (`unreachable`).

The mask has 32 bits, so a stage has a maximum of 32 facts
(`ORACLE_FACTS_MAX`). A stage with more facts gives `WASM_LIMIT` (6.4).

The guest side (S7 U1). Two folds of `domain/domain.lang` give the same
values as guest terms:

- `verdictCode n m v` is the cell byte of the verdict `v : Verdict n m`:
  0 for compatible, the `Code` in the domain order (1 to 8) for refused,
  and 9 for runtime.
- `guardFirst n m g x` is the result of `guard` for the guard `g : Guard
  n m Unit` and the mask x. It walks the facts of g in the need order
  (`guardWalk`).

The differential of 6.4 runs these folds with `langc eval` on
`test/oracle/NAME.lang` and compares each result with the same call of
the module `test/wasm/NAME.wasm`.

## 8. Host and target

> Write here: the host, its pinned commit (`PIN`), the facts from
> `probe/CAPABILITY.md` that set the design, and the check that the compiler
> runs on its output (for example the axiom report).

## 9. Open items

> Write here: one item for each open question. Number the items O1, O2 and
> so on. Mark each decision as "RULED YYYY-MM-DD (USER): <the ruling>". Mark
> each fact that a probe found as "Probe-forced".

- O1. <question>.

## 10. Milestones

> Write here: the content of each milestone, and a dated status line under
> the current milestone.

| Milestone | Content |
|---|---|
| M0 | Host probe (`probe/CAPABILITY.md`); the core types construct and check; refusal list; examples and tests |
| M1 | Core operations |
| M2 | Queries and reads, or the contract |
| M3 | Hardening: speed, limits, a checked certificate for the output |
