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
  arena_init(&arena, (size_t)1 << 28);
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
    {"live app named j",
     "table users (id : bigint key, name : text)\n"
     "app j (reads users.name)\n"
     "protocol p where\n  addColumn users copy text\n", NULL},
    {"live apps with primed index names",
     "table users (id : bigint key, name : text)\n"
     "app j (reads users.name)\napp j' (writes users.name)\napp j'' (inserts users.id)\n"
     "protocol p where\n  addColumn users copy text\n", NULL},
    {"Glue pair branch",
     "value : (A : Type 0) → (e : Equiv ((x : ℕ) × ℕ) A) → (j : I) → Glue A [ j ↦ pack ((x : ℕ) × ℕ) e ]\n"
     "value A e j = glue [ j ↦ pack 3 7 ] (witness e (pack 3 7))\n", NULL},
    {"Glue lambda branch",
     "value : (j : I) → Glue (ℕ → ℕ) [ j ↦ pack (ℕ → ℕ) (idEquiv (ℕ → ℕ)) ]\n"
     "value j = glue [ j ↦ λ x → x ] (λ x → x)\n", NULL},
    {"nested Glue branch",
     "value : (j : I) → Glue (Glue ℕ []) [ j ↦ pack (Glue ℕ []) (idEquiv (Glue ℕ [])) ]\n"
     "value j = glue [ j ↦ glue [] 7 ] (glue [] 7)\n", NULL},
    {"Glue pair branch on a join",
     "value : (j : I) → (k : I) → Glue ((x : ℕ) × ℕ) [ j ∨ k ↦ pack ((x : ℕ) × ℕ) (idEquiv ((x : ℕ) × ℕ)) ]\n"
     "value j k = glue [ j ∨ k ↦ pack 3 7 ] (pack 3 7)\n", NULL},
    {"Glue pair branch across partitions",
     "value : (j : I) → (k : I) → Glue ((x : ℕ) × ℕ) [ j ↦ pack ((x : ℕ) × ℕ) (idEquiv ((x : ℕ) × ℕ)) , k ↦ pack ((x : ℕ) × ℕ) (idEquiv ((x : ℕ) × ℕ)) ]\n"
     "value j k = glue [ j ∨ k ↦ pack 3 7 ] (pack 3 7)\n", NULL},
    {"Glue pair boundary mismatch",
     "value : (j : I) → Glue ((x : ℕ) × ℕ) [ j ↦ pack ((x : ℕ) × ℕ) (idEquiv ((x : ℕ) × ℕ)) ]\n"
     "value j = glue [ j ↦ pack 3 7 ] (pack 3 8)\n", "GLUE_BOUNDARY"},
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
    {"squash at i1", "data Trunc (A : Type 0) : Type where\n  inc : (a : A) → Trunc A\n  squash : (x : Trunc A) → (y : Trunc A) → Path (Trunc A) x y\nvalue : (A : Type 0) → (u : Trunc A) → (v : Trunc A) → Path (Trunc A) (squash A u v i1) v\nvalue A u v = refl\n", NULL},
    {"codata Stream", "codata Stream (A : Type 0) : Type where\n  corec : (head : A) → (tail : Stream A) → Stream A\n", NULL},
    {"codata labelled field", "codata Stream (A : Type 0) : Type where\n  corec : (head : A) → (pick : ℕ → Stream A) → Stream A\n", NULL},
    {"codata head law",
     "codata Stream (A : Type 0) : Type where\n  corec : (head : A) → (tail : Stream A) → Stream A\n"
     "headLaw : (A : Type 0) → (S : Type 0) → (o : S → A) → (t : S → S) → (seed : S) → Path A (head A (corec A S o t seed)) (o seed)\n"
     "headLaw A S o t seed = refl\n", NULL},
    {"codata tail law",
     "codata Stream (A : Type 0) : Type where\n  corec : (head : A) → (tail : Stream A) → Stream A\n"
     "tailLaw : (A : Type 0) → (S : Type 0) → (o : S → A) → (t : S → S) → (seed : S) → Path (Stream A) (tail A (corec A S o t seed)) (corec A S o t (t seed))\n"
     "tailLaw A S o t seed = refl\n", NULL},
    {"codata then def label",
     "codata Stream (A : Type 0) : Type where\n  corec : (head : A) → (tail : Stream A) → Stream A\n"
     "label : ℕ\nlabel = 0\n", NULL},
    {"codata field named Stream",
     "codata Stream (A : Type 0) : Type where\n  corec : (head : A) → (Stream : Stream A) → Stream A\n", "REFUSE_NAME"},
    {"codata field two times",
     "codata Stream (A : Type 0) : Type where\n  corec : (head : A) → (tail : Stream A) → (tail : Stream A) → Stream A\n", "REFUSE_NAME"},
    {"codata generated name taken",
     "Stream_pos : ℕ\nStream_pos = 0\n"
     "codata Stream (A : Type 0) : Type where\n  corec : (head : A) → (tail : Stream A) → Stream A\n", "REFUSE_NAME"},
    {"codata two recursive fields",
     "codata Branches (A : Type 0) : Type where\n  grow : (observe : A) → (left : Branches A) → (right : Branches A) → Branches A\n"
     "value : Path ℕ (observe ℕ (right ℕ (left ℕ (grow ℕ ℕ (λ (s : ℕ) → s) (λ (s : ℕ) → natAdd s 1) (λ (s : ℕ) → natMul s 2) 3)))) 8\n"
     "value = refl\n", NULL},
    {"codata mixed recursive fields",
     "codata Route (A : Type 0) : Type where\n  collect : (viaNat : ℕ → Route A) → (seen : A) → (nextRoute : Route A) → (viaBool : Bool → Route A) → Route A\n"
     "route : Route ℕ\nroute = collect ℕ ℕ (λ (s : ℕ) → s) (λ (s : ℕ) (e : ℕ) → natAdd s e) (λ (s : ℕ) → natMul s 2) (λ (s : ℕ) (e : Bool) → natMul s 3) 3\n"
     "value : Path ℕ (seen ℕ (viaBool ℕ (nextRoute ℕ (viaNat ℕ route 5)) true)) 48\n"
     "value = refl\n", NULL},
    {"Fin unfolds to a Sigma",
     "value : Path (Type 0) (Fin 2) ((k : ℕ) × Path Bool (natLe (natAdd k 1) 2) true)\nvalue = refl\n", NULL},
    {"Fin value below the bound", "value : Fin 2\nvalue = pack 1 refl\n", NULL},
    {"Fin value at the bound", "value : Fin 2\nvalue = pack 2 refl\n", "TYPE_REFL"},
    {"Fin with a neutral bound", "value : (n : ℕ) → (x : Fin n) → Path Bool (natLe (natAdd (witness x) 1) n) true\nvalue n x = payload x\n",
     NULL},
    {"ua identity transport",
     "uaNat : Path (Type 0) ℕ ℕ\nuaNat = ua ℕ ℕ (idEquiv ℕ)\n"
     "value : (n : ℕ) → Path ℕ (transp (λ i → uaNat i) i0 n) n\nvalue n = refl\n", NULL},
    {"ua swap transport",
     "swap : (p : (x : ℕ) × ℕ) → (x : ℕ) × ℕ\nswap p = pack (payload p) (witness p)\n"
     "swapEquiv : Equiv ((x : ℕ) × ℕ) ((x : ℕ) × ℕ)\n"
     "swapEquiv = pack swap (λ y → pack (pack (swap y) refl) (λ z i → pack (swap (payload z (~ i))) (λ j → payload z (~ i ∨ j))))\n"
     "uaSwap : Path (Type 0) ((x : ℕ) × ℕ) ((x : ℕ) × ℕ)\nuaSwap = ua ((x : ℕ) × ℕ) ((x : ℕ) × ℕ) swapEquiv\n"
     "value : (p : (x : ℕ) × ℕ) → Path ((x : ℕ) × ℕ) (transp (λ i → uaSwap i) i0 p) (swap p)\nvalue p = refl\n", NULL},
    {"ua swap is not the identity",
     "swap : (p : (x : ℕ) × ℕ) → (x : ℕ) × ℕ\nswap p = pack (payload p) (witness p)\n"
     "swapEquiv : Equiv ((x : ℕ) × ℕ) ((x : ℕ) × ℕ)\n"
     "swapEquiv = pack swap (λ y → pack (pack (swap y) refl) (λ z i → pack (swap (payload z (~ i))) (λ j → payload z (~ i ∨ j))))\n"
     "uaSwap : Path (Type 0) ((x : ℕ) × ℕ) ((x : ℕ) × ℕ)\nuaSwap = ua ((x : ℕ) × ℕ) ((x : ℕ) × ℕ) swapEquiv\n"
     "value : (p : (x : ℕ) × ℕ) → Path ((x : ℕ) × ℕ) (transp (λ i → uaSwap i) i0 p) p\nvalue p = refl\n", "TYPE_REFL"},
    {"hcomp at a Glue type",
     "gl : (j : I) → Glue ℕ [ j ↦ pack ℕ (idEquiv ℕ) ]\ngl j = glue [ j ↦ 7 ] 7\n"
     "value : (j : I) → Path (Glue ℕ [ j ↦ pack ℕ (idEquiv ℕ) ]) (hcomp (λ l → []) (gl j)) (gl j)\nvalue j = refl\n", NULL},
    {"Glue eta",
     "value : (j : I) → (g : Glue ℕ [ j ↦ pack ℕ (idEquiv ℕ) ]) → Path (Glue ℕ [ j ↦ pack ℕ (idEquiv ℕ) ]) (glue [ j ↦ g ] (unglue g)) g\n"
     "value j g = refl\n", NULL},
    {"unglue of an hcomp at a Glue type",
     "gl : (j : I) → (n : ℕ) → Glue ℕ [ j ↦ pack ℕ (idEquiv ℕ) ]\ngl j n = glue [ j ↦ n ] n\n"
     "value : (j : I) → (k : I) → (n : ℕ) → (p : Path ℕ n n) → Path ℕ (unglue (hcomp (λ l → [ k ↦ glue [ j ↦ p l ] (p l) ]) (gl j n))) (hcomp (λ i → [ j ↦ hcomp (λ l → [ ~ i ↦ n , k ↦ p (i ∧ l) ]) n , k ↦ p i ]) n)\n"
     "value j k n p = refl\n", NULL},
    {"ua fiber center not in normal form",
     "weirdId : Equiv ℕ ℕ\n"
     "weirdId = pack (λ x → x) (λ y → pack (pack (hcomp (λ l → []) y) (λ k → hcomp (λ l → [ k ↦ y ]) y)) (λ z i → pack (hcomp (λ l → [ i ↦ payload z (~ l) ]) y) (λ k → hcomp (λ l → [ i ↦ payload z (~ l ∨ k) , k ↦ y ]) y)))\n"
     "uaW : Path (Type 0) ℕ ℕ\nuaW = ua ℕ ℕ weirdId\n"
     "m1w : (j : I) → (n : ℕ) → ℕ\nm1w j n = hcomp (λ k → [ j ↦ transp (λ i → uaW (i ∧ ~ j)) i0 n ]) n\n", NULL},
    {"Walk left unit by the eliminator",
     "append : (a : ℕ) → (b : ℕ) → (c : ℕ) → (u : Walk a b) → (v : Walk b c) → Walk a c\n"
     "append a b c u v = fold (λ (x : ℕ) (y : ℕ) (w : Walk x y) → Walk a x → Walk a y) (λ (x : ℕ) (t : Walk a x) → t) (λ (x : ℕ) (y : ℕ) (z : ℕ) (w : Walk x y) (h : Hop y z) (ih : Walk a x → Walk a y) (t : Walk a x) → stride a y z (ih t) h) v u\n"
     "value : (a : ℕ) → (b : ℕ) → (w : Walk a b) → Path (Walk a b) (append a a b (stay a) w) w\n"
     "value a b w = fold (λ (x : ℕ) (y : ℕ) (u : Walk x y) → Path (Walk x y) (append x x y (stay x) u) u) (λ (x : ℕ) → refl) (λ (x : ℕ) (y : ℕ) (z : ℕ) (u : Walk x y) (h : Hop y z) (ih : Path (Walk x y) (append x x y (stay x) u) u) → λ i → stride x y z (ih i) h) w\n", NULL}
  };
  static const struct { const char *label; const char *signature; const char *body; } rounds[] = {
    {"Glue element", "(j : I) → Glue ℕ [ j ↦ pack ℕ (idEquiv ℕ) ]", "λ j → glue [ j ↦ 7 ] 7"},
    {"neutral unglue", "(j : I) → (g : Glue ℕ [ j ↦ pack ℕ (idEquiv ℕ) ]) → ℕ", "λ j g → unglue g"},
    {"hcomp", "(p : Path ℕ 0 0) → (i : I) → ℕ", "λ p i → hcomp (λ j → [ i ↦ p j ]) 0"},
    {"Path type", "Type 0", "Path ℕ 0 0"},
    {"varying PathP type", "(e : Path (Type 0) ℕ ℕ) → Type 0", "λ e → PathP (λ (i : I) → e i) 0 0"},
    {"hcomp path", "(p : Path ℕ 0 0) → (i : I) → Path ℕ 0 0", "λ p i → hcomp (λ j → [ i ↦ p ]) p"},
    {"hcomp Sigma", "(n : ℕ) → (i : I) → ℕ × ℕ", "λ n i → hcomp (λ j → [ i ↦ pack 1 n ]) (pack 1 n)"},
    {"hcomp universe", "(i : I) → Type 0", "λ i → hcomp (λ j → [ i ↦ ℕ ]) ℕ"},
    {"neutral transp", "(e : Path (Type 0) ℕ ℕ) → ℕ → ℕ", "λ e n → transp (λ i → e i) i0 n"},
    {"nested Path type", "Type 0", "Path (Path ℕ 0 0) refl refl"},
    {"named Sigma type", "Type 0", "(n : ℕ) × PathP (λ (_ : I) → ℕ) n n"},
    {"product type", "Type 0", "ℕ × Bool"},
    {"polymorphic Pi type", "Type 1", "(A : Type 0) → (A → A) → A → A"},
    {"function left of product", "Type 0", "(ℕ → ℕ) × ℕ"},
    {"function right of product", "Type 0", "ℕ × (ℕ → ℕ)"},
    {"product left of product", "Type 0", "(ℕ × ℕ) × ℕ"},
    {"named Sigma with Pi body", "Type 0", "(n : ℕ) × (ℕ → PathP (λ (_ : I) → ℕ) n n)"},
    {"nested named Sigma", "Type 0", "(n : ℕ) × (k : ℕ) × PathP (λ (_ : I) → ℕ) n k"},
    {"product right of arrow", "Type 0", "ℕ → ℕ × ℕ"},
    {"product left of arrow", "Type 0", "(ℕ × ℕ) → ℕ"},
    {"universe left of product", "Type 1", "(Type 0) × ℕ"},
    {"universe left of arrow", "Type 1", "(Type 0) → ℕ"},
    {"shadowed Sigma binder", "Type 0", "(n : ℕ) × (n : ℕ) × Path ℕ n n"},
    {"annotated lambda", "ℕ → ℕ", "λ (x : ℕ) → natAdd x 1"},
    {"indexed fold motive", "(a : ℕ) → (b : ℕ) → Walk a b → ℕ",
     "λ a b w → fold (λ (x : ℕ) (y : ℕ) (u : Walk x y) → ℕ) (λ (x : ℕ) → 0) (λ (x : ℕ) (y : ℕ) (z : ℕ) (u : Walk x y) (h : Hop y z) (ih : ℕ) → natAdd ih 1) w"},
    {"indexed fold constant motive", "(a : ℕ) → (b : ℕ) → Walk a b → ℕ",
     "λ a b w → fold (λ (x : ℕ) → 0) (λ (x : ℕ) (y : ℕ) (z : ℕ) (u : Walk x y) (h : Hop y z) (ih : ℕ) → natAdd ih 1) w"},
    {"indexed fold function motive", "(a : ℕ) → (b : ℕ) → Walk a b → ℕ → ℕ",
     "λ a b w → fold (λ (x : ℕ) (y : ℕ) (u : Walk x y) → ℕ → ℕ) (λ (x : ℕ) (v : ℕ) → v) (λ (x : ℕ) (y : ℕ) (z : ℕ) (u : Walk x y) (h : Hop y z) (ih : ℕ → ℕ) (v : ℕ) → natAdd (ih v) 1) w"},
    {"recursive fold motive", "Stack → ℕ",
     "λ w → fold (λ (s : Stack) → ℕ) 0 (λ (top : ℕ) (rest : Stack) (ih : ℕ) → natAdd top ih) w"},
    {"parameterized fold motive", "(n : ℕ) → Lot n → ℕ",
     "λ n w → fold (λ (s : Lot n) → ℕ) (λ (price : ℕ) → price) w"}
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
