# ofxOrtho

openFrameworks addon for **ortho**, an invented-language generator — pseudo-words
in the uncanny valley between legible text and noise.

One instance mints a language substrate once from a seed (its own consonants,
vowels, digraphs, trigraphs, contractions), then every word comes from that same
invented tongue. Output reads as one internally consistent fake language rather
than a fresh scramble. Names recur, phrases return, things get quoted — you keep
almost catching the subject. That "almost" is the point.

Same seed → same language, on every host and every run.

> **Do not use GitHub's "Download ZIP".** The language engine is a git
> submodule, and ZIP downloads do not include submodule contents — you would
> get an empty `libs/ortho-kernel/` and the addon will not build. Clone with
> `--recursive` instead; see Install below.

## Install

```
cd openFrameworks/addons
git clone --recursive https://github.com/leeMeredith/ofxOrtho.git
```

The `--recursive` matters — the language engine is a submodule. If you
already cloned without it:

```
cd ofxOrtho && git submodule update --init --recursive
```

Then generate a project with Project Generator and check **ofxOrtho**.

## Usage

```cpp
ofxOrtho ortho;

ortho.setup(12345);              // seed names the language
ortho.setPreset(0.5);            // one-knob onramp

auto words = ortho.tokens(100);  // vector<string>
```

Tokens carry their origin, which is useful for visual work — colour by source,
animate names differently, spawn on NAME:

```cpp
for (auto &t : ortho.tokensWithSource(100)) {
    switch (t.source) {
        case ORTHO_SRC_NAME:   /* the phony WHO */   break;
        case ORTHO_SRC_TOPIC:  /* the phony WHAT */  break;
        case ORTHO_SRC_PHRASE: /* recurring phrase */ break;
    }
}
```

### The seven dials

Each takes a double in `[0,1]`, default 0. Frozen vocabulary — the same names
appear as JS options, Max attributes, and oF setters.

| setter | effect |
|---|---|
| `setPhrases()` | multi-word phrase recurrence, atomic |
| `setFunctionWords()` | grammar-glue recurrence, document scope |
| `setTopics()` | the phony *what* recurring, section scope |
| `setNames()` | the phony *who* recurring, section scope |
| `setCommas()` | narrative pacing (readable path only) |
| `setQuotation()` | direct-speech span (readable path only) |
| `setScareQuotes()` | a single term held at arm's length |

Explicit setters always win over `setPreset()`, regardless of call order:

```cpp
ortho.setTopics(0.9);
ortho.setPreset(0.3);    // topics stays at 0.9
```

With all seven at zero the engine reproduces the bare golden vectors exactly.

### Sections

`newSection()` mints a fresh cast of names, topics, and phrases. Same language,
new subjects.

### Reproducible or random, both

A seed names a language, so you get both surprise and recall:

```cpp
uint32_t seed = ofRandom(0, UINT32_MAX);
ortho.setup(seed);       // a language you've never seen
                         // keep the number and you can always return to it
```

## Surfaces

| call | returns |
|---|---|
| `tokens(n, maxLetters)` | exactly `n` words, no punctuation |
| `tokensWithSource(n, maxLetters)` | same, each carrying its origin |
| `sentence(numWords, maxLetters)` | capitalised, terminal mark, punctuation per dials |
| `paragraph(numSentences, ...)` | several sentences flattened |
| `word(numLetters)` | one word |

`tokens()` is count-exact and never punctuated — it is the harness contract and
the vector-diff path. `sentence()`/`paragraph()` are the readable path.

## Design rule

**The wrapper performs translation, not interpretation.**

Its only job is converting between `ortho_token[]` and C++ types, and exposing
the dials. Every public method maps to exactly one kernel concept. If this addon
starts making language decisions, something has gone wrong — that logic belongs
in the kernel, where every host shares it.

## Conformance

Coherence across hosts is *proven*, not hoped for:

```
./tests/conformance.sh ../ortho
```

```
CONFORMANT — 7/7 vectors, spec 1.1, vectors v2
```

The oracle drives this wrapper (not the kernel directly) and diffs its output
against the reference repo's golden vectors — identical text *and* identical
source classification. It builds standalone with plain `cc`/`c++`; no
openFrameworks required, because building GLFW proves nothing about language
conformance.

Run it from the addon root, with the [`ortho`](https://github.com/leeMeredith/ortho)
reference repo as a sibling directory.

## The constellation

- [`ortho`](https://github.com/leeMeredith/ortho) — JavaScript reference, `SPEC.md`, golden vectors. **The authority.**
- [`ortho-kernel`](https://github.com/leeMeredith/ortho-kernel) — the shared host-neutral C engine
- `ofxOrtho` (this repo) — openFrameworks addon
- `ortho-max` — Max/MSP external

When the spec and an implementation disagree, the spec wins and the
implementation is a bug.

## Tested with

openFrameworks 0.12.1, macOS (Apple Silicon), Xcode clang. C++11 or later.

## License

MIT
