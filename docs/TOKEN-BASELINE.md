# Token baseline (S9)

This file compares the tokens of one task in evolve-lang, Python and Rust
(PLAN 7, bullet 6). The tool `build/tokcount` writes the table. The gate
makes the table again and compares it with this file.

## The task

The task is the rename of PLAN 4.6 (D5). The column `users.name` becomes
`users.display_name` in 7 steps: `addColumn`, `installSync`, `backfill`,
`deploy v3`, `retire v1`, `dropSync` and `dropColumn`. The app versions
`v1` and `v3` read and write the table during the rollout. Each file states
the steps and the two apps. The check gives the verdict of each app at each
stage.

## The files

- `examples/rename.lang`: the evolve-lang program of PLAN 4.6. `langc
  check` is its checker.
- `test/baseline/rename-ascii.lang`: the same program with the ASCII
  aliases of SPEC 2.3 (`->` for `→`, `\` for `λ`, `Nat` for `ℕ`). `langc
  fmt` of this file is `examples/rename.lang`, byte for byte.
- `test/baseline/rename.py`: the same task in Python. It has the data of
  the rename, a model of the check and its tests (`assert`).
- `test/baseline/rename.rs`: the same task in Rust. It has the same data,
  the same model and its tests (`#[test]`).

The checker of evolve-lang is in `langc`, so the Python and Rust samples
carry their own check. The gate counts the two samples. It does not run
them, and it does not build the Rust sample.

## The method

Each count is a fixed offline proxy, not the tokenizer of a model (PLAN
Q12 (a)). The tool counts all of each file, comments too, with two rules:

- rule A (column `tokens A`): a run of ASCII letters, digits and `_` is 1
  token. Rule A is the rule of leaf-lang 2fa3ea9 `test/tokcount.c`, so
  column A compares with leaf-lang.
- rule B (column `tokens B`): a run of length L is ceil(L / 4) tokens,
  that is (L + 3) / 4. Rule B is the word rule of quorum-lang 0c729b6
  `test/tokproxy.c`, so column B compares with quorum-lang.

In both rules, each other ASCII byte that is not white space is 1 token,
each byte from 0x80 is 1 token, and white space and control bytes are 0.
The column `code points` counts the bytes below 0x80 or from 0xc0.

## The table

From the repo root, this command writes the lines between the two markers
below:

```
build/tokcount examples/rename.lang test/baseline/rename-ascii.lang test/baseline/rename.py test/baseline/rename.rs
```

The gate runs the check command:

```
build/tokcount --check docs/TOKEN-BASELINE.md examples/rename.lang test/baseline/rename-ascii.lang test/baseline/rename.py test/baseline/rename.rs
```

The check makes the same text and compares it with the lines between the
markers, byte for byte. It also checks the half-up rule on fixed pairs,
runs `langc check` on the two `.lang` files and compares `langc fmt` of
the ASCII file with `examples/rename.lang`.

The document must be text without embedded NUL bytes. An invalid document,
a read error or a write error exits 2. `make check` also runs the C CLI
regressions in `test/tokcount-io.c`, including an unwritable stdout in
both modes and a document with extra bytes after a NUL.

<!-- tokcount begin -->
| file | bytes | code points | tokens A | tokens B |
| :--- | ---: | ---: | ---: | ---: |
| examples/rename.lang | 2044 | 2018 | 537 | 718 |
| test/baseline/rename-ascii.lang | 2035 | 2033 | 518 | 699 |
| test/baseline/rename.py | 7782 | 7782 | 2343 | 2929 |
| test/baseline/rename.rs | 11028 | 11028 | 3644 | 4376 |

ratio of the tokens A (test/baseline/rename-ascii.lang over examples/rename.lang): 0.96
ratio of the tokens B (test/baseline/rename-ascii.lang over examples/rename.lang): 0.97
ratio of the tokens A (test/baseline/rename.py over examples/rename.lang): 4.36
ratio of the tokens B (test/baseline/rename.py over examples/rename.lang): 4.08
ratio of the tokens A (test/baseline/rename.rs over examples/rename.lang): 6.79
ratio of the tokens B (test/baseline/rename.rs over examples/rename.lang): 6.09
<!-- tokcount end -->

The ratio lines below the table give the result. For each file after
`examples/rename.lang`, one line divides its tokens A by the tokens A of
`examples/rename.lang`, and the next line does the same for tokens B (half
up, 2 decimals). Each file is a full alternative, so the table has no
total row.
