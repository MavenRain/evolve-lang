#!/bin/sh
# The kit gate. `make check` runs it from the kit directory after the build.
set -eu
fail=0
count=0
for prog in examples/*.lang; do
  out=$(build/langc check "$prog" 2>&1) || true
  count=$((count + 1))
  if [ "$out" != "ok" ]; then
    echo "FAIL $prog: $out"
    fail=$((fail + 1))
  fi
done
echo "examples: $count checked, $fail failed"

tmp=${TMPDIR:-/tmp}/langc-gate.$$
mkdir -p "$tmp"
awk 'BEGIN { printf "deep : ℕ\ndeep = " } { } END {
  for (i = 0; i < 1100; i++) printf "("; printf "1"; for (i = 0; i < 1100; i++) printf ")"; print "" }' </dev/null >"$tmp/deep.lang"

# refusals DIR LABEL: each line of DIR/expect.txt is `FILE CODE [TEXT]`.
# `langc check` must exit 1 and print `langc: CODE: ...` on stderr. If the
# line has TEXT, the first stderr line must also contain TEXT as a fixed
# string. TEXT pins the branch that refuses when two branches give one code.
refusals() {
  checked=0
  while read -r file code text; do
    path="$1/$file"
    [ "$file" = "deep.lang" ] && path="$tmp/deep.lang"
    want="$code"
    [ -n "$text" ] && want="$code with \"$text\""
    status=0
    build/langc check "$path" 2>"$tmp/err" >/dev/null || status=$?
    first=$(head -n 1 "$tmp/err")
    checked=$((checked + 1))
    case "$status:$first" in
      "1:langc: $code: "*"$text"*) ;;
      *) echo "FAIL $file: want exit 1 and $want, got exit $status: $first"; fail=$((fail + 1)) ;;
    esac
  done <"$1/expect.txt"
  echo "$2 refusals: $checked checked"
}
refusals test/parse parse
refusals test/check check

# A refl refusal shows both normal forms.
build/langc check test/check/bad-refl.lang 2>"$tmp/err" >/dev/null || true
if ! rg -q -F 'the left side normalizes to 1201, the right side normalizes to 1200' "$tmp/err"; then
  echo "FAIL bad-refl.lang: the message does not show both normal forms"
  fail=$((fail + 1))
fi

# Each line of test/eval/expect.txt is `FILE NAME [ARGS] => OUTPUT`. FILE is a
# name in examples/, or a path with a `/` and no `.lang` (test/eval/path).
# `langc eval` must exit 0 and print OUTPUT. A trap also prints
# `langc: EVAL_OVERFLOW: NAME: ...` on stderr.
evals=0
while IFS= read -r line; do
  want=${line#* => }
  set -- ${line%% => *}
  file=$1
  name=$2
  shift 2
  status=0
  case "$file" in
    */*) src="$file.lang" ;;
    *) src="examples/$file.lang" ;;
  esac
  got=$(build/langc eval "$src" "$name" "$@" 2>"$tmp/err") || status=$?
  first=$(head -n 1 "$tmp/err")
  want_err=""
  [ "$want" = "trap" ] && want_err="langc: EVAL_OVERFLOW: $name: "
  evals=$((evals + 1))
  case "$status|$got|$first" in
    "0|$want|$want_err"*) ;;
    *) echo "FAIL eval $file $name: want $want, got exit $status: $got $first"; fail=$((fail + 1)) ;;
  esac
done <test/eval/expect.txt
echo "evals: $evals checked"

# eval_refused STATUS CODE NAME [ARGS]: `langc eval examples/entries.lang NAME
# ARGS` must exit STATUS and print `langc: CODE: NAME: ...` on stderr.
eval_refused() {
  want_status=$1
  code=$2
  shift 2
  status=0
  build/langc eval examples/entries.lang "$@" 2>"$tmp/err" >/dev/null || status=$?
  case "$status:$(head -n 1 "$tmp/err")" in
    "$want_status:langc: $code: $1: "*) ;;
    *) echo "FAIL eval $*: want exit $want_status and $code, got exit $status"; fail=$((fail + 1)) ;;
  esac
}
eval_refused 2 EVAL_ARGS addPrice 1
eval_refused 2 EVAL_ARGS addPrice x 1
eval_refused 2 EVAL_ARGS addPrice 18446744073709551616 1
eval_refused 2 EVAL_ARGS pick 2 4 9
eval_refused 1 EVAL_ENTRY nope

status=0
build/langc 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL usage: want exit 2, got $status"; fail=$((fail + 1)); }
status=0
build/langc check "$tmp/missing.lang" 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL missing file: want exit 2, got $status"; fail=$((fail + 1)); }

# The IR: each example lowers, with one func line for each entry.
irs=0
for f in examples/*.lang; do
  irs=$((irs + 1))
  status=0
  build/langc ir "$f" > "$tmp/ir.txt" 2> "$tmp/ir.err" || status=$?
  funcs=$(rg -c '^func ' "$tmp/ir.txt" || echo 0)
  entries=$(build/langc abi "$f" | rg -c '.' || echo 0)
  [ "$status" -eq 0 ] && [ "$funcs" -eq "$entries" ] || { echo "FAIL ir $f: exit $status, $funcs funcs for $entries entries: $(head -n 1 "$tmp/ir.err")"; fail=$((fail + 1)); }
done
echo "ir: $irs lowered"
# ir_refused NAME CODE: `langc check test/ir/NAME.lang` accepts it, and
# `langc ir` must exit 1 and print `langc: CODE: ...` on stderr.
ir_refused() {
  status=0
  build/langc check "test/ir/$1.lang" > /dev/null || status=$?
  [ "$status" -eq 0 ] || { echo "FAIL $1.lang: check exit $status"; fail=$((fail + 1)); }
  status=0
  first=$(build/langc ir "test/ir/$1.lang" 2>&1 > /dev/null) || status=$?
  case "$status:$first" in
    "1:langc: $2: "*) ;;
    *) echo "FAIL $1.lang: want exit 1 and $2, got exit $status: $first"; fail=$((fail + 1)) ;;
  esac
}
ir_refused closure REFUSE_RUNTIME_CLOSURE
got=$(build/langc eval test/ir/lazy.lang lazy 3 2>&1) || true
[ "$got" = "3" ] || { echo "FAIL lazy.lang: want 3, got $got"; fail=$((fail + 1)); }
build/langc ir test/ir/lazy.lang > /dev/null || { echo "FAIL lazy.lang: ir"; fail=$((fail + 1)); }
for want in "stackSum 3:15" "stackTrap 0:7" "stackTrap 2:5" "stackDemand 0:trap" "stackDemand 2:trap"; do
  call=${want%%:*}
  got=$(build/langc eval test/ir/spine.lang ${call% *} ${call#* } 2>"$tmp/err") || true
  want_err=""
  [ "${want#*:}" = "trap" ] && want_err="langc: EVAL_OVERFLOW: ${call% *}: "
  case "$got|$(head -n 1 "$tmp/err")" in
    "${want#*:}|$want_err"*) ;;
    *) echo "FAIL spine.lang $call: want ${want#*:}, got $got"; fail=$((fail + 1)) ;;
  esac
done
build/langc ir test/ir/spine.lang > /dev/null || { echo "FAIL spine.lang: ir"; fail=$((fail + 1)); }
ir_refused tree REFUSE_RUNTIME_TREE
ir_refused path REFUSE_RUNTIME_PATH
ir_refused glue-value REFUSE_RUNTIME_GLUE
build/langc ir test/ir/glue.lang > /dev/null || { echo "FAIL glue.lang: ir"; fail=$((fail + 1)); }
build/langc ir test/ir/walk.lang > /dev/null || { echo "FAIL walk.lang: ir"; fail=$((fail + 1)); }

# The Wasm target: each module builds and is valid. Then the differential: on
# the grid of test/grid.awk, node and `langc eval` give the same result for
# each entry (trap == trap). The stderr of eval is not merged into stdout.
wasms=0
cases=0
mismatches=0
for f in examples/*.lang test/ir/spine.lang test/ir/lazy.lang test/ir/proof-trap.lang test/ir/walk.lang; do
  w="$tmp/$(basename "$f" .lang).wasm"
  wasms=$((wasms + 1))
  if ! build/langc build "$f" -o "$w" 2>"$tmp/w.err" || ! wasm-validate "$w" 2>>"$tmp/w.err" ||
    ! wasm-objdump -x "$w" >/dev/null 2>>"$tmp/w.err"; then
    echo "FAIL wasm $f: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  build/langc ir "$f" | awk -f test/grid.awk >"$tmp/cases.txt"
  : >"$tmp/want.txt"
  while read -r call; do
    status=0
    want=$(build/langc eval "$f" $call 2>/dev/null) || status=$?
    [ "$status" -eq 0 ] || want="eval-exit-$status"
    echo "$want" >>"$tmp/want.txt"
  done <"$tmp/cases.txt"
  if ! node test/run-wasm.mjs "$w" - <"$tmp/cases.txt" >"$tmp/got.txt" 2>"$tmp/w.err"; then
    echo "FAIL node $f: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  cases=$((cases + $(rg -c '.' "$tmp/cases.txt" || echo 0)))
  paste -d '|' "$tmp/cases.txt" "$tmp/want.txt" "$tmp/got.txt" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
  bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
  mismatches=$((mismatches + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL differential $f: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
done
echo "wasm: $wasms modules valid"
echo "differential: $cases cases, $mismatches mismatches"
# A Bool argument larger than 1 traps (langc eval refuses it with EVAL_ARGS).
got=$(node test/run-wasm.mjs "$tmp/entries.wasm" pick 2 4 9 2>&1) || true
[ "$got" = "trap" ] || { echo "FAIL wasm pick 2 4 9: want trap, got $got"; fail=$((fail + 1)); }
# WASM_LIMIT (Q-S7-4 (a)): a checked program does not get to a stage with 33
# guard facts (the check reaches the arena limit first), and a checked program
# has no cell with the codes 1, 6, 7 or 8 (each refuses the full protocol). So
# build/oracle-writer checks the bound and the cell byte of each code 0 to 9 on
# hand-made reports. It prints only FAIL lines.
build/oracle-writer 2>"$tmp/oracle.err" || { echo "FAIL oracle writer: $(head -n 1 "$tmp/oracle.err")"; fail=$((fail + 1)); }
# The oracle modules (Q-S7-7 (a)). For each golden report NAME (the program is
# found as for the golden reports below), `langc wasm` gives
# test/wasm/NAME.wasm byte for byte, and the module is valid. Its calls agree
# with test/report/NAME.json: `stages`, `apps`, `verdict A I` for each matrix
# cell in the Report order (0 compatible, 1 to 8 the refused code in the
# domain order, 9 runtime), and for each stage I, `guard I 0` (1 when the
# stage has a fact, else 0) and `guard I 4294967295` (0). The calls with an
# index out of range, `verdict APPS 0`, `verdict 0 STAGES` and
# `guard STAGES 0`, trap.
# The oracle differential (Q-S7-6 (a)). test/oracle/NAME.lang is the program
# NAME with the chain of each stage and the guest entries verdict and guard.
# Its module is test/wasm/NAME.wasm too. For each (app, stage) cell and for
# each mask below 2^min(k, 6) of a stage with k facts, `langc eval` on it
# gives the result of the same call of the module.
oracles=0
ocalls=0
omis=0
dcases=0
dmis=0
for want in $(printf '%s\n' test/report/*.json | LC_ALL=C sort); do
  name=$(basename "$want" .json)
  prog="test/report/$name.lang"
  [ -f "$prog" ] || prog="examples/$name.lang"
  oracles=$((oracles + 1))
  if ! build/langc wasm "$prog" "$name" -o "$tmp/o.wasm" 2>"$tmp/o.err" || ! wasm-validate "$tmp/o.wasm" 2>>"$tmp/o.err"; then
    echo "FAIL oracle $name: $(head -n 1 "$tmp/o.err" | cut -c1-120)"
    fail=$((fail + 1))
    continue
  fi
  cmp -s "test/wasm/$name.wasm" "$tmp/o.wasm" || { echo "FAIL oracle $name: the module is not test/wasm/$name.wasm"; fail=$((fail + 1)); }
  rg -o '"guards":\[[^]]*\]' "$want" | awk '{ print gsub(/"fact":/, "") }' >"$tmp/o.facts"
  rg -o '"verdict":"[a-z]+"(,"code":"[A-Z_]+")?' "$want" >"$tmp/o.cells" || true
  awk 'BEGIN {
      split("DROP_REQUIRED MISSING_COLUMN STALE_READ UNFILLED_READ NULL_INSERT STEP_INDEX LOCK_SCAN RENAME_INPLACE", r, " ")
      for (k = 1; k <= 8; k++) code["REFUSE_" r[k]] = k
    }
    FNR == NR { facts[s++] = $1; next }
    { cell[n++] = $0 }
    END {
      print "stages|" s
      print "apps|" (s > 0 ? n / s : 0)
      for (j = 0; j < n; j++) {
        w = "unknown"
        if (cell[j] ~ /"compatible"/) w = 0
        else if (cell[j] ~ /"runtime"/) w = 9
        else if (match(cell[j], /REFUSE_[A-Z_]+/) && (substr(cell[j], RSTART, RLENGTH) in code)) w = code[substr(cell[j], RSTART, RLENGTH)]
        print "verdict " int(j / s) " " (j % s) "|" w
      }
      for (i = 0; i < s; i++) print "guard " i " 0|" (facts[i] > 0 ? 1 : 0) "\nguard " i " 4294967295|0"
      print "verdict " (s > 0 ? n / s : 0) " 0|trap\nverdict 0 " s "|trap\nguard " s " 0|trap"
    }' "$tmp/o.facts" "$tmp/o.cells" >"$tmp/o.want"
  cut -d '|' -f 1 "$tmp/o.want" >"$tmp/o.calls"
  if ! node test/run-wasm.mjs "$tmp/o.wasm" - <"$tmp/o.calls" >"$tmp/o.got" 2>"$tmp/o.err"; then
    echo "FAIL node oracle $name: $(head -n 1 "$tmp/o.err")"
    fail=$((fail + 1))
    continue
  fi
  ocalls=$((ocalls + $(rg -c '.' "$tmp/o.calls" || echo 0)))
  paste -d '|' "$tmp/o.want" "$tmp/o.got" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
  bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
  omis=$((omis + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL oracle $name: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
  orac="test/oracle/$name.lang"
  if ! build/langc wasm "$orac" "$name" -o "$tmp/d.wasm" 2>"$tmp/o.err" || ! cmp -s "test/wasm/$name.wasm" "$tmp/d.wasm"; then
    echo "FAIL oracle differential $name: the module of $orac is not test/wasm/$name.wasm $(head -n 1 "$tmp/o.err")"
    fail=$((fail + 1))
    continue
  fi
  { awk -F '|' '/^verdict / && $2 != "trap" { print $1 }' "$tmp/o.want"
    awk '{ m = $1 < 6 ? $1 : 6; for (x = 0; x < 2 ^ m; x++) print "guard " (NR - 1) " " x }' "$tmp/o.facts"; } >"$tmp/d.cases"
  : >"$tmp/d.want"
  while read -r call; do
    status=0
    w=$(build/langc eval "$orac" $call 2>/dev/null) || status=$?
    [ "$status" -eq 0 ] || w="eval-exit-$status"
    echo "$w" >>"$tmp/d.want"
  done <"$tmp/d.cases"
  if ! node test/run-wasm.mjs "test/wasm/$name.wasm" - <"$tmp/d.cases" >"$tmp/d.got" 2>"$tmp/o.err"; then
    echo "FAIL node oracle differential $name: $(head -n 1 "$tmp/o.err")"
    fail=$((fail + 1))
    continue
  fi
  dcases=$((dcases + $(rg -c '.' "$tmp/d.cases" || echo 0)))
  paste -d '|' "$tmp/d.cases" "$tmp/d.want" "$tmp/d.got" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
  bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
  dmis=$((dmis + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL oracle differential $name: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
done
echo "oracle: $oracles golden modules, $ocalls calls, $omis mismatches"
echo "oracle differential: $dcases cases, $dmis mismatches"
# The canonical printer. Each committed .lang file that parses is canonical:
# `langc fmt F` equals F byte for byte, so fmt is idempotent on it. The
# fronttool AST dump of F equals the dump of `fmt F` (the round trip). Each
# test/fmt/X.lang formats to test/fmt/X.canon (X.canon is in the canonical
# list), the two dumps are equal, and `langc check` gives W_NONCANON on
# X.lang and no warning on X.canon.
same_dump() {
  build/fronttool dump "$1" >"$tmp/d1.txt" 2>&1 && build/fronttool dump "$2" >"$tmp/d2.txt" 2>&1 &&
    cmp -s "$tmp/d1.txt" "$tmp/d2.txt"
}
canon=0
for f in domain/domain.lang examples/*.lang test/check/*.lang test/eval/*.lang test/ir/*.lang test/json/*.lang test/oracle/*.lang test/report/*.lang test/sql/*.lang test/fmt/*.canon; do
  case "$f" in
    test/json/LEX_*.lang | test/json/PARSE_*.lang | test/json/W_NONCANON.lang) continue ;;
  esac
  canon=$((canon + 1))
  status=0
  build/langc fmt "$f" >"$tmp/fmt.out" 2>"$tmp/fmt.err" || status=$?
  if [ "$status" -ne 0 ] || ! cmp -s "$f" "$tmp/fmt.out"; then
    echo "FAIL fmt $f: not canonical, exit $status $(head -n 1 "$tmp/fmt.err")"
    fail=$((fail + 1))
  elif ! same_dump "$f" "$tmp/fmt.out"; then
    echo "FAIL round trip $f: the AST dump changes after fmt"
    fail=$((fail + 1))
  fi
done
pairs=0
for f in test/fmt/*.lang; do
  c="${f%.lang}.canon"
  pairs=$((pairs + 1))
  status=0
  build/langc fmt "$f" >"$tmp/fmt.out" 2>"$tmp/fmt.err" || status=$?
  if [ "$status" -ne 0 ] || ! cmp -s "$c" "$tmp/fmt.out"; then
    echo "FAIL fmt $f: the output is not $c, exit $status $(head -n 1 "$tmp/fmt.err")"
    fail=$((fail + 1))
  fi
  same_dump "$f" "$c" || { echo "FAIL round trip $f: the AST dump differs from $c"; fail=$((fail + 1)); }
  out=$(build/langc check "$f" 2>"$tmp/err") || true
  first=$(head -n 1 "$tmp/err")
  case "$out|$first" in
    "ok|langc: W_NONCANON: "*) ;;
    *) echo "FAIL check $f: want ok and W_NONCANON, got $out|$first"; fail=$((fail + 1)) ;;
  esac
  out=$(build/langc check "$c" 2>"$tmp/err") || true
  if [ "$out" != "ok" ] || [ -s "$tmp/err" ]; then
    echo "FAIL check $c: want ok and no warning, got $out|$(head -n 1 "$tmp/err")"
    fail=$((fail + 1))
  fi
done
echo "fmt: $canon canonical, $pairs non-canonical pairs"

# JSON shape: `langc check --json` on each test/json/*.lang gives the lines of
# test/json/expect.txt exactly (with the exit code and any stderr line).
jsons=0
for f in $(printf '%s\n' test/json/*.lang | LC_ALL=C sort); do
  jsons=$((jsons + 1))
  status=0
  build/langc check --json "$f" >"$tmp/j.out" 2>"$tmp/j.err" || status=$?
  echo "== $(basename "$f") exit $status"
  cat "$tmp/j.out"
  if [ -s "$tmp/j.err" ]; then echo "stderr: $(head -n 1 "$tmp/j.err")"; fi
done >"$tmp/json.txt"
if ! cmp -s test/json/expect.txt "$tmp/json.txt"; then
  echo "FAIL json: the output is not test/json/expect.txt: $(diff test/json/expect.txt "$tmp/json.txt" | rg '^[<>]' | head -n 2 | cut -c1-160 | tr '\n' ' ')"
  fail=$((fail + 1))
fi
# The progress line: typed "k of m" with k < m (a later declaration fails),
# and an all-pass file.
for want in 'progress-part:{"parsed":"3 of 3","typed":"2 of 3","affine":"0 of 0","laws":"0 of 0"}' \
  'progress-pass:{"parsed":"2 of 2","typed":"2 of 2","affine":"0 of 0","laws":"0 of 0"}'; do
  got=$(build/langc check --json "test/json/${want%%:*}.lang" 2>/dev/null | tail -n 1) || true
  [ "$got" = "${want#*:}" ] || { echo "FAIL progress ${want%%:*}: want ${want#*:}, got $got"; fail=$((fail + 1)); }
done

# docs/DIAGNOSTICS.md has one row for each code in src/. Each code has
# test/json/CODE.lang (its first JSON line has that code), or its row gives
# "no fixture:" and a reason; not the two.
rg -o -I -N -r '$1$2$3' '"((?:LEX|PARSE|REFUSE|TYPE|NAME|EVAL|DOMAIN|GLUE|UNGLUE|SQL|WASM|W)_[A-Z_]+)"|"(OOM|IO|IO_SIZE|USAGE|INTERNAL)"|langc: ((?:EVAL|REFUSE|TYPE)_[A-Z_]+):' src |
  LC_ALL=C sort -u >"$tmp/src-codes.txt"
rg -o -N -r '$1' '^\| `([A-Z_]+)` \|' docs/DIAGNOSTICS.md | LC_ALL=C sort -u >"$tmp/doc-codes.txt"
if ! cmp -s "$tmp/src-codes.txt" "$tmp/doc-codes.txt"; then
  echo "FAIL docs/DIAGNOSTICS.md: the code list is not the src/ list: $(diff "$tmp/src-codes.txt" "$tmp/doc-codes.txt" | rg '^[<>]' | tr '\n' ' ')"
  fail=$((fail + 1))
fi
codes=0
fixtures=0
reasons=0
while read -r code; do
  codes=$((codes + 1))
  row=$(rg "^\| \`$code\` \|" docs/DIAGNOSTICS.md) || row=""
  case "$row" in *"no fixture: "*) reason=1 ;; *) reason=0 ;; esac
  if [ -f "test/json/$code.lang" ] && [ "$reason" -eq 0 ]; then
    fixtures=$((fixtures + 1))
    first=$(build/langc check --json "test/json/$code.lang" 2>/dev/null | head -n 1) || true
    case "$first" in
      "{\"code\":\"$code\","*) ;;
      *) echo "FAIL test/json/$code.lang: the first JSON line is not code $code: $(echo "$first" | cut -c1-120)"; fail=$((fail + 1)) ;;
    esac
  elif [ ! -f "test/json/$code.lang" ] && [ "$reason" -eq 1 ]; then
    reasons=$((reasons + 1))
  else
    echo "FAIL $code: want test/json/$code.lang or a \"no fixture:\" reason in docs/DIAGNOSTICS.md, not the two"
    fail=$((fail + 1))
  fi
done <"$tmp/doc-codes.txt"
for f in $(printf '%s\n' test/json/*.lang | rg '^test/json/[A-Z_]+\.lang$'); do
  rg -q -x -F "$(basename "$f" .lang)" "$tmp/doc-codes.txt" || { echo "FAIL $f: the code is not in docs/DIAGNOSTICS.md"; fail=$((fail + 1)); }
done
echo "json: $jsons fixtures; diagnostics: $codes codes, $fixtures with a fixture, $reasons with no fixture"

# Golden reports (Q-S5-7 (a)). test/report/NAME.json is the `langc report`
# document of the protocol NAME in test/report/NAME.lang, or in
# examples/NAME.lang when there is no test/report/NAME.lang (rename.json reads
# examples/rename.lang). Each run exits 0 with no stderr, and the document
# equals NAME.json byte for byte.
reports=0
for want in $(printf '%s\n' test/report/*.json | LC_ALL=C sort); do
  reports=$((reports + 1))
  name=$(basename "$want" .json)
  prog="test/report/$name.lang"
  [ -f "$prog" ] || prog="examples/$name.lang"
  status=0
  build/langc report "$prog" "$name" >"$tmp/r.out" 2>"$tmp/r.err" || status=$?
  if [ "$status" -ne 0 ] || [ -s "$tmp/r.err" ]; then
    echo "FAIL report $name: exit $status $(head -n 1 "$tmp/r.err" | cut -c1-120)"
    fail=$((fail + 1))
  elif ! cmp -s "$want" "$tmp/r.out"; then
    echo "FAIL report $name: the document is not $want: $(cmp "$want" "$tmp/r.out" 2>&1 | cut -c1-120)"
    fail=$((fail + 1))
  fi
done
echo "report: $reports golden reports"

# Golden SQL protocols (Q-S6-13 (a), Q-S6-25 (a)). test/sql/NAME/ holds the
# `langc sql` files of the protocol NAME in test/sql/NAME.lang, or in
# test/report/NAME.lang, or in examples/NAME.lang (the first that exists).
# Each run exits 0 with no stdout and no stderr, writes the same file names
# as the golden directory, and each file equals its golden file byte for byte.
sqls=0
sqlfiles=0
for want in $(printf '%s\n' test/sql/*/ | LC_ALL=C sort); do
  sqls=$((sqls + 1))
  name=$(basename "$want")
  prog="test/sql/$name.lang"
  [ -f "$prog" ] || prog="test/report/$name.lang"
  [ -f "$prog" ] || prog="examples/$name.lang"
  ls "$want" | LC_ALL=C sort >"$tmp/s.want"
  rm -rf "$tmp/sql"
  mkdir "$tmp/sql"
  status=0
  build/langc sql "$prog" "$name" "$tmp/sql" >"$tmp/s.out" 2>"$tmp/s.err" || status=$?
  ls "$tmp/sql" | LC_ALL=C sort >"$tmp/s.got"
  for f in $(cat "$tmp/s.want"); do
    sqlfiles=$((sqlfiles + 1))
  done
  if [ "$status" -ne 0 ] || [ -s "$tmp/s.out" ] || [ -s "$tmp/s.err" ]; then
    echo "FAIL sql $name: exit $status $(head -n 1 "$tmp/s.err" | cut -c1-120)"
    fail=$((fail + 1))
  elif ! cmp -s "$tmp/s.want" "$tmp/s.got"; then
    echo "FAIL sql $name: the file names are not the names in $want: $(cmp "$tmp/s.want" "$tmp/s.got" 2>&1 | cut -c1-120)"
    fail=$((fail + 1))
  else
    for f in $(cat "$tmp/s.want"); do
      if ! cmp -s "$want$f" "$tmp/sql/$f"; then
        echo "FAIL sql $name: $f is not $want$f: $(cmp "$want$f" "$tmp/sql/$f" 2>&1 | cut -c1-120)"
        fail=$((fail + 1))
      fi
    done
  fi
done
echo "sql: $sqls golden protocols, $sqlfiles files"

# The MCP server (Q-S8-11 (a)). build/mcp-test sends test/mcp/requests.jsonl to
# `build/langc mcp` and compares the replies with test/mcp/responses.jsonl line
# by line. Then it sends a ping of MCP_LINE_MAX_BYTES bytes, a ping one byte
# longer (-32700) and one more ping. Then it sends a `check` of each of the 5
# programs of examples/agent/ and compares each reply with `build/langc check
# --json PATH`: the text is the stdout, and isError is exit != 0. Each run
# exits 0 with no stderr. It prints FAIL lines, then the count line.
build/mcp-test "$tmp" || fail=$((fail + 1))

# explain-code and list-steps (Q-S8-7 (a), Q-S8-8 (a)). build/verbs-test runs
# `build/langc explain-code CODE` for each row of docs/DIAGNOSTICS.md and
# compares the line with the row, then compares each row of src/codes.c with
# its doc row (both ways). It compares the step names of `build/langc
# list-steps`, in order, with the Step constructors of domain/domain.lang. It
# prints FAIL lines, then the two count lines.
build/verbs-test || fail=$((fail + 1))

# The skill (Q-S8-9 (a), Q-S8-10 (a)). build/skill-test compares the program
# block of each example of skills/evolve-lang/SKILL.md with its file, and the
# stdout and the exit code of `build/langc check --json PATH` with the json
# block and the exit sentence. It compares examples 1, 2 and 4 with their
# sources. It prints FAIL lines, then the count line.
build/skill-test || fail=$((fail + 1))

# The README table and AGENTS.md (Q-S8-12 (a)). build/readme-test checks that
# each yes row of the table has a slice tag and names steps, that each named
# step is a step of `build/langc list-steps`, that each of the 12 steps is
# named, and that each verb after `langc ` in AGENTS.md is in the usage text.
# It prints FAIL lines, then the count line.
build/readme-test || fail=$((fail + 1))

# The token baseline (Q-S9-1 (c), Q-S9-2 (a), Q-S9-3 (a)). `build/tokcount
# --check` compares its table of the 4 files with the region between the
# tokcount markers of docs/TOKEN-BASELINE.md, checks the half-up rule on
# fixed pairs, runs `build/langc check` on the 2 .lang files and compares
# `build/langc fmt` of the ASCII file with examples/rename.lang. It prints
# FAIL lines, then the count line.
build/tokcount --check docs/TOKEN-BASELINE.md examples/rename.lang \
  test/baseline/rename-ascii.lang test/baseline/rename.py \
  test/baseline/rename.rs || fail=$((fail + 1))

# Check the counter's CLI on valid input, unwritable stdout in both modes
# and a document with extra bytes hidden after a NUL.
build/tokcount-io || fail=$((fail + 1))

rm -rf "$tmp"

if rg -n '\x{2013}|\x{2014}' . >/dev/null; then
  echo "FAIL: an em-dash or en-dash is in the kit"
  fail=$((fail + 1))
fi
echo "gate: $fail failures"
[ "$fail" -eq 0 ]
