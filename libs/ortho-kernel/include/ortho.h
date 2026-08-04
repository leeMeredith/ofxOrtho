/*
 * ortho.h — the ortho language kernel.
 *
 * Generates invented language: pseudo-words drawn from a persistent substrate
 * minted once from a seed. Same seed -> same language, on every host.
 *
 * DESIGN CONTRACT (see SPEC.md §12):
 *   - Host-neutral. No Max types, no C++ types, no host headers. This file
 *     compiles unchanged into a Max external, an openFrameworks addon, or a
 *     plain C program.
 *   - Caller-owned memory. The kernel never allocates and never frees. You
 *     hand it a buffer; it fills what fits and tells you how many it wrote.
 *   - Value-returning, not callback-driven. The kernel produces language; it
 *     does not drive your control flow.
 *   - `ortho_token` is the canonical unit. Not `char **`. Tokens carry their
 *     text AND their origin, and the struct can gain fields later without
 *     breaking any host that already compiles against it.
 *
 * Conformance: output must match the golden vectors published by the `ortho`
 * reference repo (test/vectors/v2). Same seed + same dials -> identical text
 * AND identical source classification.
 */

#ifndef ORTHO_H
#define ORTHO_H

#include <stdint.h>
#include <stddef.h>
#include "prng.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Limits
 * ------------------------------------------------------------------------*/

/* Longest token, including terminator. Measured worst case is well under 32
 * (long word + trigraph splice + 2-char contraction); 48 leaves margin. The
 * reference suite asserts every token fits. Tokens are truncated, never
 * overflowed. */
#define ORTHO_MAX_TOKEN 48

/* Substrate table sizes. These are part of the language definition — changing
 * one changes every language on every host. See SPEC.md §4. */
/* Upper bound on vowel digraphs. Spec 3.0 builds every ordered pair of
 * DISTINCT letters from the language's own vowel subset, so the count varies:
 * 6 vowels give 30, 4 give 12. Use vowel_digraph_count, not this. */
#define ORTHO_N_VOWEL_DIGRAPHS      30
#define ORTHO_N_CONSONANTS          20  /* size of the fixed consonant canon */
#define ORTHO_N_VOWELS               6  /* size of the fixed vowel canon     */
#define ORTHO_N_CONSONANT_DIGRAPHS  30
#define ORTHO_N_CONSONANT_TRIGRAPHS 10
#define ORTHO_N_CONTRACTIONS        20
#define ORTHO_N_NAMES               15
#define ORTHO_N_FUNCTION_WORDS      20

/* Section cast sizes (tier 2 recurrence). See SPEC.md §7.1. */
#define ORTHO_N_SECTION_NAMES   3
#define ORTHO_N_SECTION_TOPICS  5
#define ORTHO_N_SECTION_PHRASES 3
#define ORTHO_MAX_PHRASE_WORDS  4

/* --------------------------------------------------------------------------
 * Token
 * ------------------------------------------------------------------------*/

/* Why a token appeared. Values are NORMATIVE and match the JS `SRC` enum and
 * the third column of the v2 golden vectors. Hosts may branch on this — e.g.
 * spawning a new corridor when a NAME first appears. */
typedef enum {
    ORTHO_SRC_FRESH    = 0,  /* freshly generated, not recurring */
    ORTHO_SRC_FUNCTION = 1,  /* document-scope function word (grammar glue) */
    ORTHO_SRC_TOPIC    = 2,  /* section-scope topic word: section's subject */
    ORTHO_SRC_NAME     = 3,  /* section-scope name: section's identities   */
    ORTHO_SRC_PHRASE   = 4   /* member of a recurring multi-word phrase */
} ortho_source;

/* The canonical unit of ortho output. Fields may be ADDED in future versions;
 * existing fields never change meaning. */
typedef struct {
    char    text[ORTHO_MAX_TOKEN]; /* NUL-terminated */
    uint8_t source;                /* ortho_source */
    uint8_t flags;                 /* reserved, currently always 0 */
} ortho_token;

/* --------------------------------------------------------------------------
 * Dials
 * ------------------------------------------------------------------------*/

/* The seven dials, each clamped to [0,1], all defaulting to 0. Names are
 * frozen vocabulary shared across JS opts, Max attributes, and oF setters.
 * With all seven at 0 the engine reproduces the bare golden vectors exactly
 * and makes zero recurrence/punctuation PRNG draws. See SPEC.md §6. */
typedef struct {
    /* recurrence family — affects all generation including tokens */
    double phrases;        /* multi-word phrase recurrence, atomic */
    double function_words; /* grammar-glue recurrence, document scope */
    double topics;         /* the section's subject, section scope */
    double names;          /* the section's identities, section scope */
    /* punctuation family — readable path ONLY, never touches tokens */
    double commas;         /* narrative pacing, function-word-anchored */
    double quotation;      /* direct-speech span, speaker-anchored */
    double scare_quotes;   /* single term held at arm's length */
} ortho_dials;

/* Zero every dial (the bare default). */
void ortho_dials_clear(ortho_dials *d);

/* Fill the seven dials from a single preset value in [0,1], using the tuned
 * proportions from SPEC.md §6. preset <= 0 clears all dials. This is a
 * convenience helper, NOT part of the language definition — it consumes no
 * PRNG draws. */
void ortho_dials_preset(ortho_dials *d, double preset);

/* --------------------------------------------------------------------------
 * Engine
 * ------------------------------------------------------------------------*/

/* One instance == one invented language. The struct is exposed so hosts can
 * embed it by value (no allocation): Max puts it in t_ortho, oF puts it in
 * ofxOrtho. Treat the fields as private; use the functions. */
typedef struct {
    uint32_t   seed;
    ortho_prng rng;
    ortho_dials dials;

    /* substrate — built once at init, immutable thereafter */
    char vowel_digraphs[ORTHO_N_VOWEL_DIGRAPHS][3];
    char consonant_digraphs[ORTHO_N_CONSONANT_DIGRAPHS][3];
    char consonant_trigraphs[ORTHO_N_CONSONANT_TRIGRAPHS][4];
    char contractions[ORTHO_N_CONTRACTIONS][4];
    char names[ORTHO_N_NAMES][ORTHO_MAX_TOKEN];
    char function_words[ORTHO_N_FUNCTION_WORDS][ORTHO_MAX_TOKEN];

    /* current section cast (tier 2), re-minted at section boundaries */
    int  has_section;
    char sec_names[ORTHO_N_SECTION_NAMES][ORTHO_MAX_TOKEN];
    char sec_topics[ORTHO_N_SECTION_TOPICS][ORTHO_MAX_TOKEN];
    char sec_phrases[ORTHO_N_SECTION_PHRASES][ORTHO_MAX_PHRASE_WORDS][ORTHO_MAX_TOKEN];
    int  sec_phrase_len[ORTHO_N_SECTION_PHRASES];

    /* phrase drain queue — phrase-first, atomic, cleared at section boundary */
    char phrase_queue[ORTHO_MAX_PHRASE_WORDS][ORTHO_MAX_TOKEN];
    int  phrase_queue_len;
    int  phrase_queue_pos;

    /* source of the most recently resolved token (internal) */
    uint8_t last_source;

    /* CVV gate (SPEC §5.7, internal): suppress cluster tables while building
     * function words so the slot-2 boundary survives. Set and cleared around
     * a single run; never observable from outside. */
    int no_clusters;

    /* Slot-boundary offsets from the most recent run (SPEC §5.7, internal).
     * -1 means a cluster filled that slot as part of a multi-slot unit.
     * Per-instance so the kernel stays reentrant across instances. */
    int bounds[ORTHO_MAX_TOKEN + 1];

    /* ---- spec 3.0: this language's own character -----------------------
     * Every field below is drawn once at init and immutable thereafter. In
     * 2.x these did not exist: every seed used all 26 letters uniformly, one
     * word shape and one punctuation style, so languages differed in
     * vocabulary and in nothing else. See SPEC.md §4.2 for the draw order,
     * which is normative — a host drawing these in a different sequence
     * produces a different language and fails conformance. */

    char   root[8];              /* e.g. "CVCV" — the shape of every word   */
    int    root_len;

    char   cons_set[ORTHO_N_CONSONANTS + 1];  /* this language's consonants */
    int    cons_count;                        /* 6..20                      */
    char   vowel_set[ORTHO_N_VOWELS + 1];     /* this language's vowels     */
    int    vowel_count;                       /* 4..6                       */

    /* Cumulative weight tables, one entry per letter in the matching set.
     * Drawn against by wpick(): the first index whose cumulative value
     * exceeds a fresh next() draw. */
    double cons_w[ORTHO_N_CONSONANTS];
    double vowel_w[ORTHO_N_VOWELS];

    int    vowel_digraph_count;  /* how many of vowel_digraphs are in use   */

    char   clause_mark[4];       /* "," ";" ":" or an em dash (UTF-8, 3 by) */
    char   quote_open[4];        /* '"' or a UTF-8 guillemet                */
    char   quote_close[4];
    int    capitalize_quoted;    /* quoted speech opens with a capital      */
    char   terminals[4];         /* always ".", maybe "?" and/or "!"        */
    int    compounds;            /* this language joins roots with a hyphen */
} ortho_t;

/* Initialize an instance: seeds the PRNG and mints the language substrate.
 * `dials` may be NULL for the bare default (all zero). The engine is fully
 * usable immediately; no allocation occurs. */
void ortho_init(ortho_t *o, uint32_t seed, const ortho_dials *dials);

/* Change dials mid-life. Does NOT rebuild the substrate — the language stays
 * the same, only its character changes. Consumes no PRNG draws. */
void ortho_set_dials(ortho_t *o, const ortho_dials *dials);

/* Force a new section: mints a fresh cast of names, topics, and phrases, and
 * clears any in-flight phrase. Subjects change; the language does not. */
void ortho_new_section(ortho_t *o);

/* --------------------------------------------------------------------------
 * Generation
 *
 * All functions fill a caller-owned buffer and return the number of tokens
 * written, never more than `capacity`. The kernel allocates nothing.
 * ------------------------------------------------------------------------*/

/* Exactly `n` tokens (or `capacity`, whichever is smaller), structure-free.
 * Recurrence applies; punctuation NEVER does. This is the harness contract and
 * the golden-vector diff path. */
int ortho_tokens(ortho_t *o, int n, int max_letters,
                 ortho_token *buf, size_t capacity);

/* One sentence: first token capitalized, terminal mark on the last, with
 * punctuation woven in per the dials. */
int ortho_sentence(ortho_t *o, int num_words, int max_letters,
                   ortho_token *buf, size_t capacity);

/* One paragraph: several sentences, flattened into one token run. */
int ortho_paragraph(ortho_t *o, int num_sentences, int max_words,
                    int max_letters, ortho_token *buf, size_t capacity);

/* One word into a caller-owned buffer of at least ORTHO_MAX_TOKEN bytes.
 * Returns the string length written. */
int ortho_word(ortho_t *o, int num_letters, int allow_contractions,
               char *out);

#ifdef __cplusplus
}
#endif

#endif /* ORTHO_H */
