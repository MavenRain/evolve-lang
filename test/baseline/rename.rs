// Baseline sample (S9): the rename of examples/rename.lang in Rust, with the
// standard library only. The evolve-lang checker is in `langc`; this sample
// carries its own check of the rules that the rename uses. The gate counts
// this file (docs/TOKEN-BASELINE.md) and does not build or run it.

// The rename demo of PLAN 4.6 (D5): users.name becomes users.display_name.
// v1 reads and writes name. v3 reads and writes display_name. An app that
// inserts into users also inserts the key users.id (J2.4).
// D5 stage 0: the start, users (id, name). v1 is live. Checker stage 0.
// D5 stage 1: addColumn. Checker stage 1.
// D5 stage 2: installSync. Checker stage 2.
// D5 stage 3: backfill. Checker stage 3.
// D5 stage 4: deploy v3 (barrier). v1 and v3 are live. Checker stage 4.
// D5 stage 5: retire v1 (barrier), dropSync, dropColumn. v3 is live.
// Checker stages 5, 6 and 7.

use std::collections::{BTreeMap, BTreeSet};
use std::iter::once;

type Col = &'static str;

const TABLE: [(Col, &str); 2] = [("users.id", "bigint key"), ("users.name", "text")];

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug)]
enum AppName {
    V1,
    V3,
}

#[derive(Clone, Copy)]
struct App {
    reads: &'static [Col],
    writes: &'static [Col],
    inserts: &'static [Col],
}

impl App {
    fn uses(self) -> impl Iterator<Item = &'static Col> {
        self.reads.iter().chain(self.writes).chain(self.inserts)
    }
}

fn app(a: AppName) -> App {
    match a {
        AppName::V1 => App { reads: &["users.name", "users.id"], writes: &["users.name"], inserts: &["users.id", "users.name"] },
        AppName::V3 => App {
            reads: &["users.display_name", "users.id"],
            writes: &["users.display_name"],
            inserts: &["users.id", "users.display_name"],
        },
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Step {
    AddColumn(Col, &'static str),
    InstallSync(Col, Col),
    Backfill(Col, Col, u32),
    Deploy(AppName),
    Retire(AppName),
    DropSync(Col, Col),
    DropColumn(Col),
}

const RENAME: [Step; 7] = [
    Step::AddColumn("users.display_name", "text"),
    Step::InstallSync("users.display_name", "users.name"),
    Step::Backfill("users.display_name", "users.name", 1000),
    Step::Deploy(AppName::V3),
    Step::Retire(AppName::V1),
    Step::DropSync("users.display_name", "users.name"),
    Step::DropColumn("users.name"),
];

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Lock {
    AccessExclusive,
    ShareRowExclusive,
    RowExclusive,
    NoLock,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Scan {
    Scan,
    Metadata,
    Batched,
    NoScan,
}

// The lock mode and the scan class of each step (PLAN 5.1).
fn lock(s: &Step) -> (Lock, Scan) {
    match s {
        Step::AddColumn(..) | Step::DropSync(..) | Step::DropColumn(..) => (Lock::AccessExclusive, Scan::Metadata),
        Step::InstallSync(..) => (Lock::ShareRowExclusive, Scan::Metadata),
        Step::Backfill(..) => (Lock::RowExclusive, Scan::Batched),
        Step::Deploy(_) | Step::Retire(_) => (Lock::NoLock, Scan::NoScan),
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Refuse {
    MissingColumn,
    StaleRead,
    UnfilledRead,
    DropRequired,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Verdict {
    Compatible,
    RuntimeBackfillDone(Col),
    Refused(Refuse),
}

#[derive(Clone, Default)]
struct State {
    present: BTreeSet<Col>,
    // A read of an added column needs a backfill.
    added: BTreeSet<Col>,
    // The two columns of one field.
    links: BTreeSet<(Col, Col)>,
    syncs: BTreeSet<(Col, Col)>,
    // The backfill phase of a column: no entry if no backfill of it, 1
    // backfilled, 2 at the first barrier after the backfill, 3 after that
    // barrier (Q-S4-4 (a)).
    phase: BTreeMap<Col, u8>,
}

type Stage = (State, BTreeSet<AppName>);

fn pair(c: Col, d: Col) -> (Col, Col) {
    (c.min(d), c.max(d))
}

fn with<T: Ord + Copy>(set: &BTreeSet<T>, x: T) -> BTreeSet<T> {
    set.iter().copied().chain(once(x)).collect()
}

fn without<T: Ord + Copy>(set: &BTreeSet<T>, x: T) -> BTreeSet<T> {
    set.iter().copied().filter(|&y| y != x).collect()
}

fn next_phase(p: u8, barrier: bool) -> u8 {
    match () {
        () if p == 2 => 3,
        () if p == 1 && barrier => 2,
        () => p,
    }
}

// The stage after the step s. Only a barrier changes the live set, and a
// barrier does not change the schema.
fn step((state, live): &Stage, s: &Step) -> Stage {
    let barrier = matches!(s, Step::Deploy(_) | Step::Retire(_));
    let st = State { phase: state.phase.iter().map(|(&c, &p)| (c, next_phase(p, barrier))).collect(), ..state.clone() };
    match *s {
        Step::AddColumn(c, _) => (State { present: with(&st.present, c), added: with(&st.added, c), ..st }, live.clone()),
        Step::InstallSync(c, d) => {
            (State { links: with(&st.links, pair(c, d)), syncs: with(&st.syncs, pair(c, d)), ..st }, live.clone())
        }
        Step::Backfill(c, d, _) => {
            let phase = st.phase.iter().map(|(&k, &p)| (k, p)).chain(once((c, 1))).collect();
            (State { links: with(&st.links, pair(c, d)), phase, ..st }, live.clone())
        }
        Step::Deploy(a) => (st, with(live, a)),
        Step::Retire(a) => (st, without(live, a)),
        Step::DropSync(c, d) => (State { syncs: without(&st.syncs, pair(c, d)), ..st }, live.clone()),
        Step::DropColumn(c) => (State { present: without(&st.present, c), ..st }, live.clone()),
    }
}

// Stage i is the schema state and the live set after the first i steps.
fn stages(steps: &[Step]) -> Vec<Stage> {
    let present = TABLE.iter().map(|&(c, _)| c).collect();
    let live = [AppName::V1, AppName::V3].into_iter().filter(|&a| !steps.contains(&Step::Deploy(a))).collect();
    steps.iter().fold(vec![(State { present, ..State::default() }, live)], |acc, s| {
        let next = acc.last().map(|stage| step(stage, s));
        acc.into_iter().chain(next).collect()
    })
}

// J2.2: each column d that a live app w writes is not linked to c, or is c,
// or w also writes c, or a sync joins d and c.
fn fresh(state: &State, live: &BTreeSet<AppName>, c: Col) -> bool {
    live.iter().all(|&w| {
        app(w).writes.iter().all(|&d| {
            !state.links.contains(&pair(d, c)) || d == c || app(w).writes.contains(&c) || state.syncs.contains(&pair(d, c))
        })
    })
}

// J2: the verdict for the app a at a stage. The first rule that fails gives
// its code.
fn compat(state: &State, live: &BTreeSet<AppName>, a: AppName) -> Verdict {
    let reads = app(a).reads.iter().copied();
    let pending = reads.clone().find(|c| state.added.contains(c) && state.phase.get(c).is_some_and(|&p| p < 3));
    match () {
        () if !app(a).uses().all(|c| state.present.contains(c)) => Verdict::Refused(Refuse::MissingColumn),
        () if !reads.clone().all(|c| fresh(state, live, c)) => Verdict::Refused(Refuse::StaleRead),
        () if reads.clone().any(|c| state.added.contains(c) && state.phase.get(c).is_none()) => {
            Verdict::Refused(Refuse::UnfilledRead)
        }
        () => pending.map_or(Verdict::Compatible, Verdict::RuntimeBackfillDone),
    }
}

// The first refusal at stage i: a drop of a column that a live app uses, then
// the verdict of each live app.
fn refusal(i: usize, (state, live): &Stage, s: Option<&Step>) -> Option<(usize, AppName, Verdict)> {
    let drop = live.iter().copied().find(|&a| app(a).uses().any(|&c| s == Some(&Step::DropColumn(c))));
    drop.map(|a| (i, a, Verdict::Refused(Refuse::DropRequired))).or_else(|| {
        live.iter().copied().map(|a| (i, a, compat(state, live, a))).find(|&(_, _, v)| matches!(v, Verdict::Refused(_)))
    })
}

// J4: the first refusal in chain order, or None when the checker accepts the
// protocol. Each stage of a segment has the live set of the segment, so this
// checks each live app at each stage of its segment (J3).
fn check(steps: &[Step]) -> Option<(usize, AppName, Verdict)> {
    stages(steps).iter().enumerate().find_map(|(i, stage)| refusal(i, stage, i.checked_sub(1).and_then(|j| steps.get(j))))
}

#[cfg(test)]
mod laws {
    use super::*;
    use AppName::{V1, V3};

    // The verdicts of PLAN 4.6 for v1 and v3 at each D5 stage. D5 stage 5 is
    // checker stages 5, 6 and 7: the verdicts are at checker stage 7.
    #[test]
    fn stage_verdicts() {
        let fill = Verdict::RuntimeBackfillDone("users.display_name");
        let table = [
            (0, Verdict::Compatible, Verdict::Refused(Refuse::MissingColumn)),
            (1, Verdict::Compatible, Verdict::Refused(Refuse::UnfilledRead)),
            (2, Verdict::Compatible, Verdict::Refused(Refuse::UnfilledRead)),
            (3, Verdict::Compatible, fill),
            (4, Verdict::Compatible, fill),
            (7, Verdict::Refused(Refuse::MissingColumn), Verdict::Compatible),
        ];
        let st = stages(&RENAME);
        assert!(table.iter().all(|&(i, v1, v3)| st
            .get(i)
            .is_some_and(|(s, l)| compat(s, l, V1) == v1 && compat(s, l, V3) == v3)));
    }

    // The laws L1, L4 and L6 on RENAME (Q-S4-7 (a)), as tests. The live sets
    // are the live sets of the C driver: v1 at stages 0 to 3, v1 and v3 at
    // stage 4, v3 at stages 5 to 7.
    #[test]
    fn live_sets() {
        let lives: Vec<Vec<AppName>> = stages(&RENAME).into_iter().map(|(_, l)| l.into_iter().collect()).collect();
        let want: Vec<Vec<AppName>> =
            [(4, vec![V1]), (1, vec![V1, V3]), (3, vec![V3])].into_iter().flat_map(|(n, l)| std::iter::repeat_n(l, n)).collect();
        assert_eq!(lives, want);
    }

    // L1 stage soundness: the checker accepts RENAME, and each live app at
    // each stage is compatible or has a runtime verdict.
    #[test]
    fn stage_sound() {
        assert_eq!(check(&RENAME), None);
        assert!(stages(&RENAME).iter().all(|(s, l)| l.iter().all(|&a| !matches!(compat(s, l, a), Verdict::Refused(_)))));
    }

    // L4 drop safety: no app that is live at or after a dropColumn stage uses
    // the column.
    #[test]
    fn drop_sound() {
        let st = stages(&RENAME);
        assert!(RENAME.iter().zip(1..).all(|(s, i)| st
            .iter()
            .skip(i)
            .all(|(_, l)| l.iter().all(|&a| app(a).uses().all(|&c| *s != Step::DropColumn(c))))));
    }

    // L6 lock map: no step scans the table under ACCESS EXCLUSIVE.
    #[test]
    fn lock_sound() {
        assert!(RENAME.iter().all(|s| lock(s) != (Lock::AccessExclusive, Scan::Scan)));
    }

    // The demo variant of PLAN 4.6 (Q-S4-13 (a)): RENAME with no `retire v1`.
    // The checker refuses v1 at stage 5 (dropSync), before the drop at stage
    // 6: with no sync, a write of display_name by v3 does not change the name
    // that v1 reads.
    #[test]
    fn drop_early() {
        let steps: Vec<Step> = RENAME.into_iter().filter(|&s| s != Step::Retire(V1)).collect();
        assert_eq!(check(&steps), Some((5, V1, Verdict::Refused(Refuse::StaleRead))));
    }
}
