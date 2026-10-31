# Baseline sample (S9): the rename of examples/rename.lang in Python, with the
# standard library only. The evolve-lang checker is in `langc`; this sample
# carries its own check of the rules that the rename uses. The gate counts
# this file (docs/TOKEN-BASELINE.md) and does not run it.

# The rename demo of PLAN 4.6 (D5): users.name becomes users.display_name.
# v1 reads and writes name. v3 reads and writes display_name. An app that
# inserts into users also inserts the key users.id (J2.4).
# D5 stage 0: the start, users (id, name). v1 is live. Checker stage 0.
# D5 stage 1: addColumn. Checker stage 1.
# D5 stage 2: installSync. Checker stage 2.
# D5 stage 3: backfill. Checker stage 3.
# D5 stage 4: deploy v3 (barrier). v1 and v3 are live. Checker stage 4.
# D5 stage 5: retire v1 (barrier), dropSync, dropColumn. v3 is live.
# Checker stages 5, 6 and 7.

from dataclasses import dataclass, field, replace
from itertools import accumulate

TABLE = {"users.id": "bigint key", "users.name": "text"}


@dataclass(frozen=True)
class App:
    reads: tuple[str, ...]
    writes: tuple[str, ...]
    inserts: tuple[str, ...]

    def uses(self) -> tuple[str, ...]:
        return self.reads + self.writes + self.inserts


APPS = {
    "v1": App(("users.name", "users.id"), ("users.name",), ("users.id", "users.name")),
    "v3": App(("users.display_name", "users.id"), ("users.display_name",), ("users.id", "users.display_name")),
}

RENAME = [
    ("addColumn", "users.display_name", "text"),
    ("installSync", "users.display_name", "users.name"),
    ("backfill", "users.display_name", "users.name", 1000),
    ("deploy", "v3"),
    ("retire", "v1"),
    ("dropSync", "users.display_name", "users.name"),
    ("dropColumn", "users.name"),
]

# The lock mode and the scan class of each step (PLAN 5.1).
LOCKS = {
    "addColumn": ("accessExclusive", "metadata"),
    "installSync": ("shareRowExclusive", "metadata"),
    "backfill": ("rowExclusive", "batched"),
    "deploy": ("noLock", "noScan"),
    "retire": ("noLock", "noScan"),
    "dropSync": ("accessExclusive", "metadata"),
    "dropColumn": ("accessExclusive", "metadata"),
}

BARRIERS = ("deploy", "retire")

# The backfill phase of a column: 0 no backfill of it, 1 backfilled, 2 at the
# first barrier after the backfill, 3 after that barrier (Q-S4-4 (a)).
NEXT_PHASE = {(1, True): 2, (2, True): 3, (2, False): 3}


@dataclass(frozen=True)
class State:
    present: frozenset[str]
    added: frozenset[str] = frozenset()  # a read of an added column needs a backfill
    links: frozenset[frozenset[str]] = frozenset()  # the two columns of one field
    syncs: frozenset[frozenset[str]] = frozenset()
    phase: dict[str, int] = field(default_factory=dict)


def step(stage, s):
    """The stage after the step s. Only a barrier changes the live set, and a
    barrier does not change the schema."""
    state, live = stage
    kind, *args = s
    st = replace(state, phase={c: NEXT_PHASE.get((p, kind in BARRIERS), p) for c, p in state.phase.items()})
    match kind, args:
        case "addColumn", [c, _]:
            return replace(st, present=st.present | {c}, added=st.added | {c}), live
        case "installSync", [c, d]:
            return replace(st, links=st.links | {frozenset({c, d})}, syncs=st.syncs | {frozenset({c, d})}), live
        case "backfill", [c, d, _]:
            return replace(st, links=st.links | {frozenset({c, d})}, phase=st.phase | {c: 1}), live
        case "deploy", [a]:
            return st, live | {a}
        case "retire", [a]:
            return st, live - {a}
        case "dropSync", [c, d]:
            return replace(st, syncs=st.syncs - {frozenset({c, d})}), live
        case "dropColumn", [c]:
            return replace(st, present=st.present - {c}), live


def stages(steps):
    """Stage i is the schema state and the live set after the first i steps."""
    start = State(frozenset(TABLE)), frozenset(APPS) - {s[1] for s in steps if s[0] == "deploy"}
    return list(accumulate(steps, step, initial=start))


def fresh(state, live, c):
    """J2.2: each column d that a live app w writes is not linked to c, or is
    c, or w also writes c, or a sync joins d and c."""
    return all(
        frozenset({d, c}) not in state.links or d == c or c in APPS[w].writes or frozenset({d, c}) in state.syncs
        for w in live
        for d in APPS[w].writes
    )


def compat(state, live, a):
    """J2: the verdict for the app a at a stage. The first rule that fails
    gives its code."""
    reads = APPS[a].reads
    if not all(c in state.present for c in APPS[a].uses()):
        return "refused: REFUSE_MISSING_COLUMN"
    if not all(fresh(state, live, c) for c in reads):
        return "refused: REFUSE_STALE_READ"
    if any(c in state.added and state.phase.get(c, 0) == 0 for c in reads):
        return "refused: REFUSE_UNFILLED_READ"
    pending = [c for c in reads if c in state.added and state.phase.get(c, 0) < 3]
    return f"runtime: backfillDone {pending[0].replace('.', ' ')}" if pending else "compatible"


def refusals(i, stage, s):
    """The refusals at stage i: a drop of a column that a live app uses, then
    the verdict of each live app."""
    state, live = stage
    order = [a for a in APPS if a in live]
    if s[0] == "dropColumn":
        yield from ((i, a, "refused: REFUSE_DROP_REQUIRED") for a in order if s[1] in APPS[a].uses())
    yield from ((i, a, v) for a in order if (v := compat(state, live, a)).startswith("refused"))


def check(steps):
    """J4: the first refusal in chain order as (stage, app, verdict), or None
    when the checker accepts the protocol. Each stage of a segment has the
    live set of the segment, so this checks each live app at each stage of
    its segment (J3)."""
    return next((r for i, (stage, s) in enumerate(zip(stages(steps), [("start",), *steps])) for r in refusals(i, stage, s)), None)


STAGES = stages(RENAME)

# The verdicts of PLAN 4.6 for v1 and v3 at each D5 stage. D5 stage 5 is
# checker stages 5, 6 and 7: the verdicts are at checker stage 7.
assert all(
    compat(*STAGES[i], "v1") == v1 and compat(*STAGES[i], "v3") == v3
    for i, v1, v3 in [
        (0, "compatible", "refused: REFUSE_MISSING_COLUMN"),
        (1, "compatible", "refused: REFUSE_UNFILLED_READ"),
        (2, "compatible", "refused: REFUSE_UNFILLED_READ"),
        (3, "compatible", "runtime: backfillDone users display_name"),
        (4, "compatible", "runtime: backfillDone users display_name"),
        (7, "refused: REFUSE_MISSING_COLUMN", "compatible"),
    ]
)

# The laws L1, L4 and L6 on RENAME (Q-S4-7 (a)), as asserts. The live sets
# are the live sets of the C driver: v1 at stages 0 to 3, v1 and v3 at stage
# 4, v3 at stages 5 to 7.
assert [sorted(live) for _, live in STAGES] == [["v1"]] * 4 + [["v1", "v3"]] + [["v3"]] * 3

# L1 stage soundness: the checker accepts RENAME, and each live app at each
# stage is compatible or has a runtime verdict.
assert check(RENAME) is None
assert all(not compat(state, live, a).startswith("refused") for state, live in STAGES for a in live)

# L4 drop safety: no app that is live at or after a dropColumn stage uses the
# column.
assert all(
    s[1] not in APPS[a].uses() for i, s in enumerate(RENAME, 1) if s[0] == "dropColumn" for _, live in STAGES[i:] for a in live
)

# L6 lock map: no step scans the table under ACCESS EXCLUSIVE.
assert all(LOCKS[s[0]] != ("accessExclusive", "scan") for s in RENAME)

# The demo variant of PLAN 4.6 (Q-S4-13 (a)): RENAME with no `retire v1`. The
# checker refuses v1 at stage 5 (dropSync), before the drop at stage 6: with
# no sync, a write of display_name by v3 does not change the name that v1
# reads.
assert check([s for s in RENAME if s != ("retire", "v1")]) == (5, "v1", "refused: REFUSE_STALE_READ")
