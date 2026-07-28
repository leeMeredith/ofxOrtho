# HOSTS.md — the host surface contract

`SPEC.md` in the reference repo defines the **language**. This file defines
everything a host has to decide that the language does not: message names,
default shaping values, buffer limits, and lifecycle semantics.

It exists because two hosts can both be perfectly conformant and still
disagree. Conformance says "same seed, same dials, same tokens." It says
nothing about what `page 3` means, how big a paragraph is by default, or
whether changing the seed re-mints immediately. Those get invented per host
unless they are written down once — so they are written down here.

When a host and this file disagree, this file wins and the host is a bug.

Status: `ortho-max` conforms. `ofxOrtho` conforms except where noted.

---

## 1. Generation surface

Every host exposes exactly four generation operations. Names differ to suit
the host idiom; behavior does not.

| operation | ortho-max | ofxOrtho | kernel call |
|---|---|---|---|
| N tokens, punctuation-free | `tokens N` | `tokens(n, maxLetters)` | `ortho_tokens` |
| one sentence | — | `sentence(numWords, maxLetters)` | `ortho_sentence` |
| one paragraph | `bang` | `paragraph(numSentences, ...)` | `ortho_paragraph` |
| N paragraphs | `page N` | — | `ortho_paragraph` × N |
| new section | `section` | `newSection()` | `ortho_new_section` |

**`page N` means N paragraphs.** Not one paragraph of N sentences — that
reading is already reachable by setting the sentences-per-paragraph value, so
it would make `page` a duplicate of an existing control. `page` is composed in
the glue; the kernel has no page function.

**`page` does not mint a new section.** The cast of names, topics, and phrases
persists across all N paragraphs. Only the explicit section operation changes
subjects.

Gaps in the table above are host coverage gaps, not semantic differences.
`ortho-max` has no single-sentence message; `ofxOrtho` has no page method. Both
are worth closing eventually.

---

## 2. Shaping values

These are **not dials**. They are not part of the language definition, not in
the frozen seven-name vocabulary, and they do not affect conformance. They
control how much text comes out and how long the words are.

Hosts must keep them visibly separate from the dials — separate group in the
inspector, separate section in the docs — so the distinction stays legible.

| value | default | rationale |
|---|---|---|
| `maxLetters` | 8 | matches the reference oracle's default |
| `maxWords` (per sentence) | 12 | matches ofxOrtho's declared default |
| `numSentences` (per paragraph) | 4 | Max's choice; ofxOrtho requires it explicitly |

**These are ceilings on a random draw, not targets.** Word and sentence
lengths vary below the value rather than sitting at it — deliberate, since
uniform lengths read as a list rather than as prose.

`ortho_paragraph` draws a per-sentence ceiling below `maxWords`, then passes
`maxLetters` through unchanged; `ortho_sentence` draws each word's length below
it. Sentence length varies, word length is reduced once. Paragraph words average
about 4.8 letters at `maxLetters` 8.

Spec 1.x drew below *both*, which reduced word length twice and put paragraph
words at two or three letters — below the digraph band, so paragraphs carried
nothing seed-specific and seeds read alike. Corrected in spec 2.0 (§9).

The readable paths are still not covered by the golden vectors, which exercise
`ortho_tokens` only. That gap is why the 1.x behaviour survived unnoticed in
three hosts. Hosts must not compensate for shaping behaviour locally: a host
that scaled a value on its own would diverge from every other host. Changes here
are spec changes.

`ofxOrtho` takes these as call arguments and remembers nothing between calls.
`ortho-max` holds them as saved attributes, because a Max object is
configured once in the patch rather than at each call site. That difference is
idiomatic and intended; the default *values* must still agree.

---

## 3. Buffer capacity

The kernel never allocates. Hosts own the buffer, and therefore own the cap.

`ortho-max` uses a fixed 1024-token buffer embedded in the object struct, so
an instance is a single allocation with no teardown.

**A host must never truncate silently.** If a request would exceed capacity,
clamp it and tell the user through the host's own reporting channel — the Max
console, an oF log line. The kernel reports nothing; that is the glue's job,
and it is the same rule as any other engine-flags-it, glue-announces-it split.

---

## 4. Lifecycle

**Seed changes re-mint immediately.** Setting the seed calls `ortho_init` at
once, rebuilding the substrate. It does not defer to the next generation call.
The seed *is* the language; a host holding a stale substrate is lying about
which language it speaks.

**Dial changes never re-mint.** `ortho_set_dials` rebuilds nothing and consumes
no PRNG draws, so a host may push dials as often as it likes. `ortho-max`
pushes before every generation call, which makes attribute application order
irrelevant.

**Dials must not be written directly into `ortho_t`.** `ortho.h` says to treat
its fields as private, and `ortho_set_dials` clamps on the way in. A host keeps
its own `ortho_dials` and pushes it; writing into `engine.dials` bypasses the
clamp and takes authority over engine state away from the engine.

---

## 5. Preset and explicit marks

`ortho_dials_preset` is glue-level convenience, not part of the language. It
consumes no PRNG draws.

**Setting a dial by hand marks it explicit, and preset never overwrites a
marked dial.** This makes order irrelevant: `@preset 0.5 @names 0.9` and
`@names 0.9 @preset 0.5` land on identical values, and the same holds for
`setPreset()` before or after `setNames()`.

Hosts also expose a clear operation that drops every mark, zeroes all seven
dials, **and zeroes the preset** — `cleardials` in Max, `clearDials()` in oF.
All three, not the first two: a live preset would refill every unmarked dial on
the next change, silently undoing the clear.

**Known divergence.** `ortho-max` saves dials into the patcher file, so on
reload every dial fires its setter and marks itself explicit. Values survive
identically, but preset will not move anything until `cleardials` is sent.
`ofxOrtho` has no persistence and never hits this. Fixable by persisting the
marks alongside the values; not yet done.

---

## 6. Token sources

Every token carries an `ORTHO_SRC_*` value, and the header calls these
normative. **Hosts must expose them on every generation path**, not only the
token path — a host that discards origin is strictly less capable than its
siblings for no reason.

- `ortho-max` — parallel list on the right outlet, one integer per word
- `ofxOrtho` — `tokensWithSource`, `sentenceWithSource`, `paragraphWithSource`

Punctuation is applied by a post-pass that does not touch `.source`, so origin
is accurate on the readable paths too. Nothing is inferred host-side.

---

## 7. Punctuation is not stripped

The readable paths bake punctuation into the token text. A token can arrive as
`Lxzxe,` or `"Titrg"` — one unit, punctuation included. `tokens` is
punctuation-free by contract and is the conformance path.

**Hosts must not strip or escape.** The punctuation is part of the language,
and mangling it in one host breaks parity with every other for no gain.

Host-specific note, Max: comma and semicolon are message separators in the
patcher language. A symbol carrying one travels safely through `zl`, `coll`,
`route`, and anything else programmatic; it re-parses only if pasted into a
message box or written into a patcher file. This is documented, not fixed.

---

## 8. Frozen vocabulary

The seven dial names are fixed across every host and the JS reference:

`phrases`, `function_words`, `topics`, `names`, `commas`, `quotation`,
`scare_quotes`

Hosts adapt only the casing their idiom requires — `@function_words` in Max,
`setFunctionWords()` in C++. The underlying name never changes, and no host
adds an eighth dial. A new dial is a spec change, not a host change.

---

## 9. Delivery

**Settled: both hosts vendor.**

`ortho-max` holds an unmodified copy at `source/projects/ortho/ortho-kernel/`
with a `KERNEL_VERSION` file naming the upstream commit. `ofxOrtho` holds one
at `libs/ortho-kernel/` with a `VENDORED.md` doing the same. Both build from a
plain `git clone` or a GitHub ZIP download with no further steps.

The reason is acquisition, not preference. GitHub's ZIP downloads exclude
submodule contents entirely, and an oF addon is normally acquired by dropping a
folder into `addons/` — so the most common path is exactly the one a submodule
breaks. A Max package is normally acquired as a release archive, where the
kernel sources are not present at all and only the built external matters.
Neither audience should have to know what a submodule is.

The cost is two copies of a frozen library, and it is paid by discipline rather
than by tooling: edit upstream, re-vendor the whole directory, update the
provenance file, re-run conformance. Never edit a vendored copy in place.

Drift is detectable, which is what makes this safe. `diff -r` a vendored copy
against upstream proves it is unmodified; the conformance suite proves it still
speaks the language. Drift that changes output fails loudly, and drift that does
not is cosmetic.

---

## 10. Conformance

A host is conformant when it reproduces the published golden vectors
(`test/vectors/v2` in the reference repo) through its own full output path —
not merely when the kernel it links passes `make test`.

The kernel's own suite proves C matches JavaScript. It says nothing about
whether the host's conversion layer preserved that. Test the host end to end.

Verified for `ortho-max`:

| vector | path | result |
|---|---|---|
| `seed_0_bare` | `tokens 20`, fresh object | matches |
| `seed_12345_bare` | `tokens 20`, fresh object | matches |
| `seed_12345_preset50` | `tokens 20`, fresh object | matches, all five source classes |

"Fresh object" matters: every word drawn advances the PRNG, so a host tested
after any other generation call is comparing a different point in the stream,
not a different language.
