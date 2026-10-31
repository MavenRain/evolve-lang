---
name: evolve-lang
description: Write, check and repair evolve-lang programs (a cubical language for PostgreSQL schema changes as checked transitions while old application versions still run) with the langc checker. Use it when the task is in a yes row of the evolve-lang README table.
---

# evolve-lang

## Routing rule

Use evolve-lang when the task is in a "yes" row of the README table: a
schema change while old application versions still run, a multi-tenant
rollout with tenants at mixed stages, a resumable backfill, NOT NULL by
CHECK NOT VALID and VALIDATE, an index build that does not block writes.
For ad hoc SQL, data analysis or one-off local schemas, use plain SQL.
Do not use evolve-lang for MySQL, SQLite, SQL Server or other SQL
dialects: the design refuses them.

## The loop

1. Write FILE.lang: the `table` declarations, one `app` declaration for
   each application version with the columns that it reads, writes and
   inserts, and one `protocol` with one step on each line. Each step word
   is a constructor of `Step` (`langc list-steps` gives the 12 steps and
   their forms).
2. Run `build/langc check --json FILE.lang`. Each stdout line before the
   last is one diagnostic (docs/DIAGNOSTICS.md). The last line is the
   progress object. Exit 0: the protocol is safe for each live version at
   each stage. Exit 1: the checker refuses the file. Exit 2: usage or IO.
3. A diagnostic with `"retry":true`: apply its `fix` at its `span` when
   `fix` is not null. Else change the file at the span. A `REFUSE_` code
   names the rule that the protocol breaks: move, add or remove a step
   (for example, deploy the new version or retire the old version before
   the step). Then do step 2 again.
4. A diagnostic with `"retry":false`: a change to the program text cannot
   remove it. Correct the command or the environment.
5. On W_NONCANON, write the output of `build/langc fmt FILE.lang` to a
   new file, and replace FILE.lang with it.
6. When the check gives exit 0, run `build/langc report FILE.lang
   PROTOCOL` for the segments, the stages and the live versions, and
   `build/langc sql FILE.lang PROTOCOL OUTDIR` for the SQL files.

`langc explain-code CODE` gives the row of one code from
docs/DIAGNOSTICS.md as one JSON line.

## Verbs

- `langc check [--json] PROG`: check the program. Text: `ok`, or one
  line on stderr.
- `langc fmt PROG`: print the canonical text of PROG.
- `langc report PROG PROTOCOL`: the JSON report of the protocol (apps,
  segments, stages, live versions, locks).
- `langc sql PROG PROTOCOL OUTDIR` and `langc wasm PROG PROTOCOL -o OUT`:
  the SQL files and the WebAssembly module of the protocol.
- `langc explain-code CODE`: the row of CODE as one JSON line.
- `langc list-steps`: the 12 steps, one JSON line each.
- `langc eval PROG NAME [ARGS...]`, `langc ir PROG`, `langc abi PROG` and
  `langc build PROG -o OUT [--runtime]`: the kit back end.
- `langc mcp`: a JSON-RPC 2.0 server on stdin and stdout, one message on
  each line. Its 5 tools are `check` (`path`), `fmt` (`path`), `report`
  (`path`, `protocol`), `explain-code` (`code`) and `list-steps` (no
  arguments). A tool result has the stdout of the verb as its first text
  item, and the stderr as a second item when it is not empty. `isError`
  is true when the exit code is not 0.

## Examples

The examples go from easy to hard. Run each command from the repository
root. The gate runs each program and compares the output with the block
below it.

### 1. Add a nullable column

The new column is nullable, and v1 does not use it. The check passes.

```evolve-lang examples/agent/01-add-column.lang
-- A golden report (S5 U3): a protocol with one step. There are two stages
-- and one segment. v1 does not use the new column, so v1 is compatible at
-- the two stages.
table users (id : bigint key, name : text)

app v1 (reads users.id, reads users.name, inserts users.id, inserts users.name)

protocol single where
  addColumn users note text
```

`build/langc check --json examples/agent/01-add-column.lang` gives:

```json
{"parsed":"3 of 3","typed":"3 of 3","affine":"0 of 0","laws":"0 of 0"}
```

The exit code is 0.

### 2. Rename a column

users.name becomes users.display_name in seven steps. v1 and v3 are live together at stage 4. The laws at the end prove the protocol by `refl`.

```evolve-lang examples/agent/02-rename.lang
-- The rename demo of PLAN 4.6 (D5): users.name becomes users.display_name.
-- v1 reads and writes name. v3 reads and writes display_name. An app that
-- inserts into users also inserts the key users.id (J2.4).
-- D5 stage 0: the start, users (id, name). v1 is live. Checker stage 0.
-- D5 stage 1: addColumn. Checker stage 1.
-- D5 stage 2: installSync. Checker stage 2.
-- D5 stage 3: backfill. Checker stage 3.
-- D5 stage 4: deploy v3 (barrier). v1 and v3 are live. Checker stage 4.
-- D5 stage 5: retire v1 (barrier), dropSync, dropColumn. v3 is live.
-- Checker stages 5, 6 and 7.
table users (id : bigint key, name : text)

app v1 (reads users.name, reads users.id, writes users.name, inserts users.id, inserts users.name)

app v3 (reads users.display_name, reads users.id, writes users.display_name, inserts users.id, inserts users.display_name)

protocol rename where
  addColumn users display_name text
  installSync users display_name name
  backfill users display_name ← name batch 1000
  deploy v3
  retire v1
  dropSync users display_name name
  dropColumn users name

-- The laws L1, L4 and L6 on `rename` (Q-S4-7 (a)), proved by `refl`. The
-- live sets are the live sets of the C driver: v1 at stages 0 to 3, v1 and
-- v3 at stage 4, v3 at stages 5 to 7. `cols` is all the program columns.
lives : ℕ → Live 3
lives i = flagIf (natLe i 3) (pack 1 (λ (j : ℕ) → v1)) (flagIf (natEq i 4) (pack 2 (λ (j : ℕ) → flagIf (natEq j 0) v1 v3)) (pack 1 (λ (j : ℕ) → v3)))

cols : Cols 3
cols = pack 3 (λ (j : ℕ) → flagIf (natEq j 0) users_id (flagIf (natEq j 1) users_name users_display_name))

stageSound : Bool
stageSound = accepts 3 1 rename_0 rename_7 rename lives cols

stageSoundProof : Path Bool stageSound true
stageSoundProof = refl

dropSound : Bool
dropSound = dropSafe 3 1 rename_0 rename_7 rename lives

dropSoundProof : Path Bool dropSound true
dropSoundProof = refl

lockSound : Bool
lockSound = lockSafe 3 1 rename_0 rename_7 rename

lockSoundProof : Path Bool lockSound true
lockSoundProof = refl
```

`build/langc check --json examples/agent/02-rename.lang` gives:

```json
{"parsed":"12 of 12","typed":"12 of 12","affine":"0 of 0","laws":"0 of 0"}
```

The exit code is 0.

### 3. A rename with a too-early drop

The drop of users.name comes before the deploy of v3, so v1 is live at the drop. The checker refuses the drop at line 17.

```evolve-lang examples/agent/03-drop-early.lang
-- Example 3: the rename with a too-early drop. The protocol drops users.name
-- before the deploy of v3, so v1 is live at the drop, and v1 reads, writes
-- and inserts users.name. The checker refuses the drop with
-- REFUSE_DROP_REQUIRED. To repair it, deploy v3 and retire v1 before the
-- dropSync, as in example 2.
table users (id : bigint key, name : text)

app v1 (reads users.name, reads users.id, writes users.name, inserts users.id, inserts users.name)

app v3 (reads users.display_name, reads users.id, writes users.display_name, inserts users.id, inserts users.display_name)

protocol rename where
  addColumn users display_name text
  installSync users display_name name
  backfill users display_name ← name batch 1000
  dropSync users display_name name
  dropColumn users name
  deploy v3
```

`build/langc check --json examples/agent/03-drop-early.lang` gives:

```json
{"code":"REFUSE_DROP_REQUIRED","severity":"error","span":{"file":"examples/agent/03-drop-early.lang","line":17,"col":3,"end_line":17,"end_col":13},"expected":null,"found":"v1","fix":null,"retry":true,"message":"stage 5 (dropColumn users name): the live version v1 reads, writes or inserts the column that the step drops; retire v1 before stage 5"}
{"parsed":"4 of 4","typed":"3 of 4","affine":"0 of 0","laws":"0 of 0"}
```

The exit code is 1.

### 4. NOT NULL by CHECK NOT VALID and VALIDATE

v2 inserts the new column and v1 does not, so the protocol retires v1 before the CHECK. The check passes.

```evolve-lang examples/agent/04-not-null.lang
-- A golden report (S5 U3): a new not-null column. v2 inserts users.handle and
-- v1 does not. After the CHECK, v1 is REFUSE_NULL_INSERT, so v1 is retired
-- before the CHECK. validate takes the lock shareUpdateExclusive and the
-- scan class scan.
table users (id : bigint key, name : text)

app v1 (reads users.id, reads users.name, inserts users.id, inserts users.name)

app v2 (reads users.id, reads users.name, inserts users.id, inserts users.name, inserts users.handle)

protocol strict where
  addColumn users handle text
  deploy v2
  retire v1
  backfill users handle ← name batch 500
  addNotNullCheck users handle
  validate users handle
  setNotNull users handle
  dropCheck users handle
```

`build/langc check --json examples/agent/04-not-null.lang` gives:

```json
{"parsed":"4 of 4","typed":"4 of 4","affine":"0 of 0","laws":"0 of 0"}
```

The exit code is 0.

### 5. A rollout with two segments and a stale read

The deploy of v2 cuts the protocol into two segments. In segment 2, v1 and v2 are live at each stage. The checker refuses the dropSync at line 19.

```evolve-lang examples/agent/05-stale-read.lang
-- Example 5: a rollout with two segments and a stale read. The deploy of v2
-- is a barrier, so the protocol has two segments. In a segment, tenants can
-- be at mixed stages, and each live version can meet each stage of the
-- segment. In segment 2, v1 and v2 are live. After the dropSync, v1 writes
-- orders.status and v2 reads orders.state, and no trigger copies the value.
-- The checker refuses the dropSync with REFUSE_STALE_READ.
table orders (id : bigint key, status : text)

app v1 (reads orders.id, reads orders.status, writes orders.status, inserts orders.id, inserts orders.status)

app v2 (reads orders.id, reads orders.state, writes orders.state, inserts orders.id, inserts orders.state)

protocol rollout where
  addColumn orders state text
  installSync orders state status
  backfill orders state ← status batch 500
  deploy v2
  createIndexConcurrently orders state
  dropSync orders state status
```

`build/langc check --json examples/agent/05-stale-read.lang` gives:

```json
{"code":"REFUSE_STALE_READ","severity":"error","span":{"file":"examples/agent/05-stale-read.lang","line":19,"col":3,"end_line":19,"end_col":11},"expected":null,"found":"v1","fix":null,"retry":true,"message":"stage 6 (dropSync orders state status): the live version v1 reads a column that another live version updates through a different column, with no sync trigger; install a sync trigger before stage 6"}
{"parsed":"4 of 4","typed":"3 of 4","affine":"0 of 0","laws":"0 of 0"}
```

The exit code is 1.
