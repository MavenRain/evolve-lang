/* Check face restrictions, binder scope and normalized source round trips. */
#include "front/check.h"
#include "front/front.h"
#include <stdio.h>
#include <string.h>

static int load(Arena *arena, const char *source, Machine *machine, Diag *diag) {
  Front front;
  return front_load(arena, "cubical-regression", source, strlen(source), &front, diag)
      && check_program(arena, &front.decls, machine, diag);
}

static int check_case(const char *label, const char *source, const char *want) {
  Arena arena;
  Diag diag;
  Machine machine;
  arena_init(&arena, (size_t)1 << 27);
  diag_init(&diag);
  int checked = load(&arena, source, &machine, &diag);
  int ok = want == NULL ? checked : !checked && diag.code != NULL && strcmp(diag.code, want) == 0;
  if (!ok) {
    fprintf(stderr, "FAIL cubical %s: wanted %s, got %s\n", label,
            want == NULL ? "ok" : want, checked ? "ok" : diag.code == NULL ? "no diagnostic" : diag.code);
    if (diag.set) diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return ok;
}

static int round_trip(const char *label, const char *signature, const char *body) {
  Arena arena;
  Diag diag;
  Machine machine;
  char source[32768];
  char normal[16384];
  char again[65536];
  int ok = 0;
  arena_init(&arena, (size_t)1 << 27);
  diag_init(&diag);
  int len = snprintf(source, sizeof source, "value : %s\nvalue = %s\n", signature, body);
  if (len < 0 || (size_t)len >= sizeof source || !load(&arena, source, &machine, &diag)) goto done;
  const Value *v = def_value(&machine, machine.def_count - 1u);
  if (v == NULL || !value_print(&machine, NULL, 0, v, normal, sizeof normal)) goto done;
  len = snprintf(again, sizeof again, "%s\nagain : %s\nagain = %s\n", source, signature, normal);
  if (len < 0 || (size_t)len >= sizeof again) goto done;
  diag_init(&diag);
  if (!load(&arena, again, &machine, &diag)) goto done;
  ok = conv_values(&machine, 0, def_value(&machine, machine.def_count - 2u),
                              def_value(&machine, machine.def_count - 1u));
done:
  if (!ok) {
    fprintf(stderr, "FAIL cubical round trip %s\n", label);
    if (diag.set) diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return ok;
}

int main(void) {
  static const struct { const char *label; const char *source; const char *code; } checks[] = {
    {"indexed cell result scope",
     "data Indexed (A : Type 0) : Path (Type 0) A A → Type where\n"
     "  inject : (a : A) → Indexed A refl\n"
     "value : Indexed ℕ refl\nvalue = inject ℕ 0\n", NULL},
    {"reducible type index",
     "data HiddenIndex (A : Type 0) : ((λ (u : Unit) → Type 0) unit) → Type where\n"
     "  generate : (a : A) → HiddenIndex A ℕ\n", "REFUSE_CELL"},
    {"reducible higher kind index",
     "data HiddenKind (A : Type 0) : (Unit → ((λ (u : Unit) → Type 0) unit)) → Type where\n"
     "  generate : (a : A) → HiddenKind A (λ u → ℕ)\n", "REFUSE_CELL"},
    {"shadowed side face", "value : (j : I) → ℕ\nvalue j = hcomp (λ j → [ j ↦ 0 ]) 0\n", "TYPE_SYSTEM"},
    {"renamed side face", "value : (j : I) → ℕ\nvalue j = hcomp (λ k → [ k ↦ 0 ]) 0\n", "TYPE_SYSTEM"},
    {"side face application", "value : (f : I → I) → ℕ\nvalue f = hcomp (λ j → [ f j ↦ 0 ]) 0\n", "TYPE_SYSTEM"},
    {"outer side face", "value : (j : I) → ℕ\nvalue j = hcomp (λ k → [ j ↦ 0 ]) 0\n", NULL},
    {"constant side face", "value : ℕ\nvalue = hcomp (λ j → [ j ∧ i0 ↦ 0 ]) 0\n", NULL},
    {"shadowed interval annotation", "value : (I : Type 0) → ℕ\nvalue I = hcomp (λ (j : I) → [ i1 ↦ 0 ]) 0\n", "TYPE_MISMATCH"},
    {"wrong side annotation", "value : ℕ\nvalue = hcomp (λ (j : ℕ) → [ i1 ↦ 0 ]) 0\n", "TYPE_MISMATCH"},
    {"interval annotation alias", "value : ℕ\nvalue = hcomp (λ (j : (λ (u : Unit) → I) unit) → [ i1 ↦ 0 ]) 0\n", NULL},
    {"refl under face", "value : (p : Path ℕ 0 1) → (i : I) → Partial i (Path ℕ (p i) 1)\nvalue p i = [ i ↦ refl ]\n", NULL},
    {"path endpoints under face", "value : (p : Path ℕ 0 1) → (i : I) → Partial i (Path ℕ (p i) 1)\nvalue p i = [ i ↦ (λ j → 1) ]\n", NULL},
    {"constant line under face", "value : (e : Path (Type 0) ℕ ℕ) → (i : I) → Partial (~ i) (PathP (λ (j : I) → e (i ∧ j)) 0 (transp (λ j → e (i ∧ j)) i0 0))\nvalue e i = [ ~ i ↦ refl ]\n", NULL},
    {"trans under face", "value : (p : Path ℕ 0 1) → (i : I) → (q : Path ℕ 0 (p i)) → (r : Path ℕ 1 2) → Partial i (Path ℕ 0 2)\nvalue p i q r = [ i ↦ trans q r ]\n", NULL},
    {"annotation under face", "value : (e : Path (Type 0) ℕ ℕ) → (i : I) → Partial i ((x : e i) → ℕ)\nvalue e i = [ i ↦ (λ (x : ℕ) → 0) ]\n", NULL},
    {"refl on wrong face", "value : (p : Path ℕ 0 1) → (i : I) → Partial (~ i) (Path ℕ (p i) 1)\nvalue p i = [ ~ i ↦ refl ]\n", "TYPE_REFL"},
    {"endpoints on wrong face", "value : (p : Path ℕ 0 1) → (i : I) → Partial (~ i) (Path ℕ (p i) 1)\nvalue p i = [ ~ i ↦ (λ j → 1) ]\n", "TYPE_ENDPOINT"},
    {"refl on both faces", "value : (p : Path ℕ 0 1) → (i : I) → Partial (i ∨ ~ i) (Path ℕ (p i) 1)\nvalue p i = [ i ∨ ~ i ↦ refl ]\n", "TYPE_REFL"},
    {"trans on wrong face", "value : (p : Path ℕ 0 1) → (i : I) → (q : Path ℕ 0 (p i)) → (r : Path ℕ 1 2) → Partial (~ i) (Path ℕ 0 2)\nvalue p i q r = [ ~ i ↦ trans q r ]\n", "TYPE_MISMATCH"},
    {"squash at i0", "data Trunc (A : Type 0) : Type where\n  inc : (a : A) → Trunc A\n  squash : (x : Trunc A) → (y : Trunc A) → Path (Trunc A) x y\nvalue : (A : Type 0) → (u : Trunc A) → (v : Trunc A) → Path (Trunc A) (squash A u v i0) u\nvalue A u v = refl\n", NULL},
    {"squash at i1", "data Trunc (A : Type 0) : Type where\n  inc : (a : A) → Trunc A\n  squash : (x : Trunc A) → (y : Trunc A) → Path (Trunc A) x y\nvalue : (A : Type 0) → (u : Trunc A) → (v : Trunc A) → Path (Trunc A) (squash A u v i1) v\nvalue A u v = refl\n", NULL}
  };
  static const struct { const char *label; const char *signature; const char *body; } rounds[] = {
    {"hcomp", "(p : Path ℕ 0 0) → (i : I) → ℕ", "λ p i → hcomp (λ j → [ i ↦ p j ]) 0"},
    {"Path type", "Type 0", "Path ℕ 0 0"},
    {"varying PathP type", "(e : Path (Type 0) ℕ ℕ) → Type 0", "λ e → PathP (λ (i : I) → e i) 0 0"},
    {"hcomp path", "(p : Path ℕ 0 0) → (i : I) → Path ℕ 0 0", "λ p i → hcomp (λ j → [ i ↦ p ]) p"},
    {"hcomp Sigma", "(n : ℕ) → (i : I) → ℕ × ℕ", "λ n i → hcomp (λ j → [ i ↦ pack 1 n ]) (pack 1 n)"},
    {"hcomp universe", "(i : I) → Type 0", "λ i → hcomp (λ j → [ i ↦ ℕ ]) ℕ"},
    {"neutral transp", "(e : Path (Type 0) ℕ ℕ) → ℕ → ℕ", "λ e n → transp (λ i → e i) i0 n"},
    {"nested Path type", "Type 0", "Path (Path ℕ 0 0) refl refl"}
  };
  int ok = 1;
  size_t i;
  for (i = 0; i < sizeof checks / sizeof checks[0]; i++)
    ok &= check_case(checks[i].label, checks[i].source, checks[i].code);
  for (i = 0; i < sizeof rounds / sizeof rounds[0]; i++)
    ok &= round_trip(rounds[i].label, rounds[i].signature, rounds[i].body);
  if (ok) printf("cubical regressions: %zu checks, %zu round trips passed\n",
                 sizeof checks / sizeof checks[0], sizeof rounds / sizeof rounds[0]);
  return ok ? 0 : 1;
}
