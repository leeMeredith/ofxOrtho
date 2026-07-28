# ortho-kernel

The shared C kernel for **ortho**, an invented-language generator — pseudo-words
that hold the shape and internal consistency of a language without belonging to
any existing one.

This repo holds the language engine as plain C99. It is consumed as a git
submodule by the native hosts:

- **[`ortho`](https://github.com/leeMeredith/ortho)** — reference implementation
  (JavaScript), specification, and golden vectors. **The authority.**
- **`ortho-kernel`** (this repo) — the shared, host-neutral C engine.
- **`ortho-max`** — Max/MSP external, wraps this kernel.
- **`ortho-of`** — openFrameworks addon (`ofxOrtho`), wraps this kernel.

The language is defined by [`SPEC.md`](https://github.com/leeMeredith/ortho/blob/main/SPEC.md)
in the reference repo. When this implementation and the spec disagree, the spec
wins and this is a bug.

## Design contract

- **Host-neutral.** No Max types, no C++ types, no host headers. These files
  compile unchanged into a Max external, an openFrameworks addon, or a plain C
  program.
- **Caller-owned memory.** The kernel never allocates and never frees. You hand
  it a buffer; it fills what fits and returns how many tokens it wrote.
- **Value-returning, not callback-driven.** The kernel produces language; it
  does not drive your control flow.
- **`ortho_token` is the canonical unit** — not `char **`. A token carries its
  text *and* its origin, and the struct can gain fields later without breaking
  hosts that already compile against it.

## Usage

```c
#include "ortho.h"

ortho_t o;
ortho_dials d;
ortho_token buf[256];

ortho_dials_preset(&d, 0.4);      /* or set the seven dials individually */
ortho_init(&o, 12345, &d);        /* seed 12345 == one specific language */

int n = ortho_tokens(&o, 100, 8, buf, 256);
for (int i = 0; i < n; i++) {
    printf("%s (source %u)\n", buf[i].text, buf[i].source);
}
```

`ortho_init` mints the language substrate once. Every token after that is drawn
from that same invented tongue, so output reads as internally consistent. The
same seed always produces the same language, on every host.

### The seven dials

Each is a double in `[0,1]`, default 0. Frozen vocabulary — the same names
appear as JS options, Max attributes, and oF setters.

| dial | effect |
|---|---|
| `phrases` | multi-word phrase recurrence, phrase-first and atomic |
| `function_words` | grammar-glue recurrence, document scope |
| `topics` | the section's subject recurring, section scope |
| `names` | the section's identities recurring, section scope |
| `commas` | narrative pacing, function-word-anchored |
| `quotation` | direct-speech span, speaker-anchored to a cast name |
| `scare_quotes` | a single term held at arm's length |

With all seven at 0 the engine reproduces the bare golden vectors exactly and
makes zero recurrence/punctuation PRNG draws.

`ortho_dials_preset()` fills all seven from one value — a convenience helper,
not part of the language definition.

### Token source

Every token reports why it appeared:

```c
ORTHO_SRC_FRESH     /* 0 — freshly generated */
ORTHO_SRC_FUNCTION  /* 1 — document-scope function word */
ORTHO_SRC_TOPIC     /* 2 — section-scope topic (the section's subject) */
ORTHO_SRC_NAME      /* 3 — section-scope name (the section's identities) */
ORTHO_SRC_PHRASE    /* 4 — member of a recurring phrase */
```

Hosts can branch on this — for example spawning a new corridor when a name
first appears.

## Building

```
make            # build the oracle
make test       # conformance against the reference vectors
make test REF=~/Documents/ortho
```

Or with CMake, which is how the Max external consumes it:

```cmake
add_subdirectory(ortho-kernel)
target_link_libraries(my_target PRIVATE ortho_kernel)
```

## Conformance

Coherence across hosts is *proven*, not hoped for. `tests/oracle.c` mirrors
`test/oracle.js` in the reference repo and prints the same three-column format
(`index`, `word`, `source`). `tests/conformance.sh` diffs its output against the
published golden vectors:

```
$ make test REF=../ortho
PASS  seed_0_bare
PASS  seed_1_bare
PASS  seed_42_bare
PASS  seed_12345_bare
PASS  seed_4294967295_bare
PASS  seed_42_preset50
PASS  seed_12345_preset50

CONFORMANT — 7/7 vectors, spec 2.0, vectors v3
```

The bare vectors prove the all-dials-zero baseline; the preset vectors exercise
all five source classes, phrase queueing, and section minting. A clean diff
means a seed names the same language in C as it does in JavaScript.

## Development discipline

One invariant governs this repo: **every subsystem reaches green before the
next subsystem exists.** PRNG passes against the JS stream, commit. Substrate
passes, commit. `word()` passes, commit. Tokens pass the vectors, commit.
Recurrence, commit. Punctuation, commit.

That history is documentation — you can `git bisect` the architecture. It also
means a failure is always localized to the subsystem just added, rather than
hiding somewhere in a thousand lines that all went in at once.

## Porting notes

The port is faithful to draw order, not to tidiness. Every PRNG call happens in
the same sequence as the JavaScript, because output identity depends on it.
Where the reference has a quirk, this reproduces the quirk rather than
correcting it — the language *is* the quirks. Those spots are marked `QUIRK` in
the source.

## License

MIT
