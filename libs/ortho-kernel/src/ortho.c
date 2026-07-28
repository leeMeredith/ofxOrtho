/*
 * ortho.c — the ortho language kernel.
 *
 * A faithful port of the JavaScript reference (src/engine.js in the `ortho`
 * repo). Draw order is normative: every PRNG call happens in the same sequence
 * as the JS, because output identity across hosts depends on it. Where the JS
 * has a quirk, this port reproduces the quirk rather than correcting it — the
 * language IS the quirks. Such places are marked QUIRK.
 *
 * No allocation, no host types, no globals.
 */

#include "ortho.h"

#include <string.h>
#include <ctype.h>

/* --------------------------------------------------------------------------
 * Fixed character canon (SPEC.md §3)
 * ------------------------------------------------------------------------*/

static const char ALPHABET[]    = "abcdefghijklmnopqrstuvwxyz"; /* 26 */
static const char CONSONANTS[]  = "bcdfghjklmnpqrstvwxz";        /* 20 */
static const char VOWELS[]      = "aeiouy";                      /*  6 */
static const char PUNCTUATION[] = ".?!";                         /*  3 */

#define N_ALPHABET   26
#define N_CONSONANTS 20
#define N_VOWELS      6
#define N_PUNCT       3

/* --------------------------------------------------------------------------
 * Small string helpers (bounded, never overflow ORTHO_MAX_TOKEN)
 * ------------------------------------------------------------------------*/

static void str_copy(char *dst, const char *src)
{
    size_t i = 0;
    while (src[i] != '\0' && i < (size_t)(ORTHO_MAX_TOKEN - 1)) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void str_append_char(char *dst, char c)
{
    size_t len = strlen(dst);
    if (len < (size_t)(ORTHO_MAX_TOKEN - 1)) {
        dst[len] = c;
        dst[len + 1] = '\0';
    }
}

static void str_append(char *dst, const char *src)
{
    size_t len = strlen(dst);
    size_t i = 0;
    while (src[i] != '\0' && len + i < (size_t)(ORTHO_MAX_TOKEN - 1)) {
        dst[len + i] = src[i];
        i++;
    }
    dst[len + i] = '\0';
}

/* JS: insertAt(s, add, i) -> s[0..i] + add + s[i+1..] */
static void str_insert_at(char *s, const char *add, int i)
{
    char tmp[ORTHO_MAX_TOKEN];
    int len = (int)strlen(s);
    int cut = i + 1;
    if (cut > len) cut = len;
    if (cut < 0) cut = 0;

    tmp[0] = '\0';
    {
        char head[ORTHO_MAX_TOKEN];
        int j;
        for (j = 0; j < cut && j < ORTHO_MAX_TOKEN - 1; j++) head[j] = s[j];
        head[(cut < ORTHO_MAX_TOKEN - 1) ? cut : ORTHO_MAX_TOKEN - 1] = '\0';
        str_copy(tmp, head);
    }
    str_append(tmp, add);
    str_append(tmp, s + cut);
    str_copy(s, tmp);
}

/* JS: spliceRange(s, add, from, to) -> s[0..from] + add + s[to..] */
static void str_splice_range(char *s, const char *add, int from, int to)
{
    char tmp[ORTHO_MAX_TOKEN];
    int len = (int)strlen(s);
    int j;

    if (from < 0) from = 0;
    if (from > len) from = len;
    if (to < from) to = from;
    if (to > len) to = len;

    tmp[0] = '\0';
    {
        char head[ORTHO_MAX_TOKEN];
        for (j = 0; j < from && j < ORTHO_MAX_TOKEN - 1; j++) head[j] = s[j];
        head[(from < ORTHO_MAX_TOKEN - 1) ? from : ORTHO_MAX_TOKEN - 1] = '\0';
        str_copy(tmp, head);
    }
    str_append(tmp, add);
    str_append(tmp, s + to);
    str_copy(s, tmp);
}

/* --------------------------------------------------------------------------
 * Dials
 * ------------------------------------------------------------------------*/

static double clamp01(double x)
{
    if (!(x > 0.0)) return 0.0;   /* also catches NaN */
    if (x > 1.0) return 1.0;
    return x;
}

void ortho_dials_clear(ortho_dials *d)
{
    d->phrases = 0.0;
    d->function_words = 0.0;
    d->topics = 0.0;
    d->names = 0.0;
    d->commas = 0.0;
    d->quotation = 0.0;
    d->scare_quotes = 0.0;
}

void ortho_dials_preset(ortho_dials *d, double preset)
{
    double p = clamp01(preset);
    if (p <= 0.0) {
        ortho_dials_clear(d);
        return;
    }
    /* proportions from SPEC.md §6 */
    d->phrases        = clamp01(p * 0.50);
    d->function_words = clamp01(p * 0.90);
    d->topics         = clamp01(p * 0.60);
    d->names          = clamp01(p * 0.50);
    d->commas         = clamp01(p * 0.80);
    d->quotation      = clamp01(p * 0.40);
    d->scare_quotes   = clamp01(p * 0.25);
}

void ortho_set_dials(ortho_t *o, const ortho_dials *dials)
{
    if (dials == NULL) {
        ortho_dials_clear(&o->dials);
        return;
    }
    o->dials.phrases        = clamp01(dials->phrases);
    o->dials.function_words = clamp01(dials->function_words);
    o->dials.topics         = clamp01(dials->topics);
    o->dials.names          = clamp01(dials->names);
    o->dials.commas         = clamp01(dials->commas);
    o->dials.quotation      = clamp01(dials->quotation);
    o->dials.scare_quotes   = clamp01(dials->scare_quotes);
}

/* --------------------------------------------------------------------------
 * Word generation (SPEC.md §5.1) — draw order is normative
 * ------------------------------------------------------------------------*/

/* SPEC 2.0 §5.1 step 8 — cluster guard.
 *
 * A filter over the finished word, not a decision: consumes no PRNG draws, so
 * it cannot shift the stream. Drops a character when either holds:
 *   - it would be the third consecutive occurrence of the same letter
 *   - it is a consonant identical to the one before it, with at least two
 *     consonants already in the run
 *
 * Trigraphs (three DISTINCT consonants) are deliberately untouched — they are
 * how a seed's own tables reach the page. Apostrophes are transparent: neither
 * vowel nor consonant, and they do not reset the run.
 */
static int is_vowel_ch(char c)
{
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y';
}

static void cluster_guard(char *w)
{
    char tmp[ORTHO_MAX_TOKEN];
    int  i, n = 0, cons = 0;
    int  len = (int)strlen(w);

    for (i = 0; i < len && n < ORTHO_MAX_TOKEN - 1; i++) {
        char ch = w[i];

        if (n >= 2 && ch == tmp[n - 1] && ch == tmp[n - 2]) continue;

        if (ch != '\'' && !is_vowel_ch(ch)) {
            if (cons >= 2 && n > 0 && ch == tmp[n - 1]) continue;
            cons++;
        } else if (ch != '\'') {
            cons = 0;
        }
        tmp[n++] = ch;
    }
    tmp[n] = '\0';
    strcpy(w, tmp);
}

int ortho_word(ortho_t *o, int num_letters, int allow_contractions, char *out)
{
    ortho_prng *r = &o->rng;
    int n = num_letters;
    int num_vowels, num_consonants;
    char vowels[ORTHO_MAX_TOKEN];
    char consonants[ORTHO_MAX_TOKEN];
    int v, c, m, mode;

    out[0] = '\0';
    if (n <= 0) n = 1;

    /* --- vowel/consonant split --- */
    if (n > 5) {
        num_vowels = (int)((ortho_prng_next(r) * (double)n) / 2.0);
        /* QUIRK: JS has `if (n >= n - 2)`, which is always true, so this
         * assignment always runs for n > 5. Reproduced exactly. */
        num_vowels = n / 2;
    } else {
        int pick = ortho_prng_below(r, 2);
        if (pick == 0 && n > 2) num_vowels = 2;
        else num_vowels = 1;
        if (n <= 3) num_vowels = 1;
        if (n == 4 || n == 5) num_vowels = 1;
    }
    num_consonants = n - num_vowels;
    if (num_consonants < 1) num_consonants = 1;

    /* --- pools --- */
    vowels[0] = '\0';
    for (v = 0; v < num_vowels; v++) {
        str_append_char(vowels, VOWELS[ortho_prng_below(r, N_VOWELS)]);
    }
    consonants[0] = '\0';
    for (c = 0; c < num_consonants; c++) {
        str_append_char(consonants, CONSONANTS[ortho_prng_below(r, N_CONSONANTS)]);
    }

    /* --- assembly --- */
    if (num_vowels <= 1 && num_consonants >= 5) {
        str_copy(out, consonants);
        str_insert_at(out, vowels, num_consonants / 2);
    } else {
        int vlen = (int)strlen(vowels);
        int clen = (int)strlen(consonants);
        mode = ortho_prng_below(r, 4);
        if (mode == 0) {
            for (m = 0; m < n; m++) {
                if (m < vlen) str_append_char(out, vowels[m]);
                if (m < clen) str_append_char(out, consonants[m]);
            }
        } else if (mode == 1) {
            for (m = 0; m < n; m++) {
                if (m < clen) str_append_char(out, consonants[m]);
                if (m < vlen) str_append_char(out, vowels[m]);
            }
        } else if (mode == 2) {
            for (m = n; m >= 0; m--) {
                if (m < vlen) str_append_char(out, vowels[m]);
                if (m < clen) str_append_char(out, consonants[m]);
            }
        } else {
            for (m = n; m >= 0; m--) {
                if (m < clen) str_append_char(out, consonants[m]);
                if (m < vlen) str_append_char(out, vowels[m]);
            }
        }

        /* --- digraph / trigraph injection --- */
        if (ortho_prng_below(r, 2) == 0) {
            int len = (int)strlen(out);
            if (len > 7) {
                const char *tri =
                    o->consonant_trigraphs[ortho_prng_below(r, ORTHO_N_CONSONANT_TRIGRAPHS)];
                if (mode == 0 || mode == 1) {
                    str_splice_range(out, tri, len - 2, len);
                } else {
                    str_splice_range(out, tri, 0, 2);
                }
            }
            len = (int)strlen(out);
            if (len > 2 && len < 8) {
                const char *di =
                    o->consonant_digraphs[ortho_prng_below(r, ORTHO_N_CONSONANT_DIGRAPHS)];
                if (mode == 0 || mode == 1) {
                    str_splice_range(out, di, len - 1, len);
                } else {
                    str_splice_range(out, di, 0, 1);
                }
            }
        }
    }

    /* --- contraction --- */
    if (allow_contractions && ortho_prng_below(r, 4) == 0) {
        str_append(out, o->contractions[ortho_prng_below(r, ORTHO_N_CONTRACTIONS)]);
    }

    /* --- leading double-letter fix --- */
    if (strlen(out) > 3 && out[0] == out[1]) {
        char add[2];
        add[0] = out[2];
        add[1] = '\0';
        str_insert_at(out, add, 1);
    }

    cluster_guard(out);
    return (int)strlen(out);
}

/* --------------------------------------------------------------------------
 * Substrate (SPEC.md §4) — built once, in this exact order
 * ------------------------------------------------------------------------*/

static void build_substrate(ortho_t *o)
{
    ortho_prng *r = &o->rng;
    int i, j, k;

    /* 1. vowel digraphs — no PRNG draws */
    k = 0;
    for (i = 0; i < N_VOWELS; i++) {
        for (j = 0; j < N_VOWELS; j++) {
            o->vowel_digraphs[k][0] = VOWELS[i];
            o->vowel_digraphs[k][1] = VOWELS[j];
            o->vowel_digraphs[k][2] = '\0';
            k++;
        }
    }

    /* 2. consonant digraphs */
    for (k = 0; k < ORTHO_N_CONSONANT_DIGRAPHS; k++) {
        int a = ortho_prng_below(r, N_CONSONANTS);
        int b = ortho_prng_below(r, N_CONSONANTS);
        while (a == b) b = ortho_prng_below(r, N_CONSONANTS);
        o->consonant_digraphs[k][0] = CONSONANTS[a];
        o->consonant_digraphs[k][1] = CONSONANTS[b];
        o->consonant_digraphs[k][2] = '\0';
    }

    /* 3. consonant trigraphs, de-duplicated positionally */
    for (k = 0; k < ORTHO_N_CONSONANT_TRIGRAPHS; k++) {
        int idx[3];
        int a, b;
        idx[0] = ortho_prng_below(r, N_CONSONANTS);
        idx[1] = ortho_prng_below(r, N_CONSONANTS);
        idx[2] = ortho_prng_below(r, N_CONSONANTS);
        for (a = 0; a < 3; a++) {
            for (b = 0; b < 3; b++) {
                if (a != b) {
                    while (idx[a] == idx[b]) {
                        idx[b] = ortho_prng_below(r, N_CONSONANTS);
                    }
                }
            }
        }
        o->consonant_trigraphs[k][0] = CONSONANTS[idx[0]];
        o->consonant_trigraphs[k][1] = CONSONANTS[idx[1]];
        o->consonant_trigraphs[k][2] = CONSONANTS[idx[2]];
        o->consonant_trigraphs[k][3] = '\0';
    }

    /* 4. contractions — first 5 are two-letter */
    for (k = 0; k < ORTHO_N_CONTRACTIONS; k++) {
        if (k < 5) {
            int a = ortho_prng_below(r, N_ALPHABET);
            int b = ortho_prng_below(r, N_ALPHABET);
            o->contractions[k][0] = '\'';
            o->contractions[k][1] = ALPHABET[a];
            o->contractions[k][2] = ALPHABET[b];
            o->contractions[k][3] = '\0';
        } else {
            int a = ortho_prng_below(r, N_ALPHABET);
            o->contractions[k][0] = '\'';
            o->contractions[k][1] = ALPHABET[a];
            o->contractions[k][2] = '\0';
        }
    }

    /* 5. names (15) */
    for (k = 0; k < ORTHO_N_NAMES; k++) {
        int len = ortho_prng_below(r, 10);
        if (len < 3) len = 5;
        ortho_word(o, len, 0, o->names[k]);
        if (o->names[k][0] != '\0') {
            o->names[k][0] = (char)toupper((unsigned char)o->names[k][0]);
        }
    }

    /* 6. function words (20), growing size every n/4 */
    {
        int size = 2;
        int count = 0;
        int step = ORTHO_N_FUNCTION_WORDS / 4;
        for (k = 0; k < ORTHO_N_FUNCTION_WORDS; k++) {
            count++;
            if (count == step) { size++; count = 0; }
            ortho_word(o, size, 0, o->function_words[k]);
        }
    }
}

/* --------------------------------------------------------------------------
 * Sections (SPEC.md §7.1)
 * ------------------------------------------------------------------------*/

void ortho_new_section(ortho_t *o)
{
    ortho_prng *r = &o->rng;
    int i, j;

    /* a section boundary ends any in-flight phrase */
    o->phrase_queue_len = 0;
    o->phrase_queue_pos = 0;

    for (i = 0; i < ORTHO_N_SECTION_NAMES; i++) {
        str_copy(o->sec_names[i], o->names[ortho_prng_below(r, ORTHO_N_NAMES)]);
    }
    for (i = 0; i < ORTHO_N_SECTION_TOPICS; i++) {
        char w[ORTHO_MAX_TOKEN];
        w[0] = '\0';
        while (strlen(w) < 2) {
            ortho_word(o, 3 + ortho_prng_below(r, 5), 0, w);
        }
        str_copy(o->sec_topics[i], w);
    }
    for (i = 0; i < ORTHO_N_SECTION_PHRASES; i++) {
        int len = 2 + ortho_prng_below(r, 3);   /* 2..4 words */
        if (len > ORTHO_MAX_PHRASE_WORDS) len = ORTHO_MAX_PHRASE_WORDS;
        o->sec_phrase_len[i] = len;
        for (j = 0; j < len; j++) {
            double pick = ortho_prng_next(r);
            if (pick < 0.4) {
                str_copy(o->sec_phrases[i][j],
                         o->sec_topics[ortho_prng_below(r, ORTHO_N_SECTION_TOPICS)]);
            } else if (pick < 0.6) {
                str_copy(o->sec_phrases[i][j],
                         o->sec_names[ortho_prng_below(r, ORTHO_N_SECTION_NAMES)]);
            } else {
                char w[ORTHO_MAX_TOKEN];
                w[0] = '\0';
                while (strlen(w) < 2) {
                    ortho_word(o, 2 + ortho_prng_below(r, 4), 0, w);
                }
                str_copy(o->sec_phrases[i][j], w);
            }
        }
    }
    o->has_section = 1;
}

/* --------------------------------------------------------------------------
 * Recurrence resolver (SPEC.md §7.2) — fixed order, first hit wins
 *
 * Writes the recurring term into `out` and returns 1, or returns 0 meaning
 * "generate fresh". Sets o->last_source either way.
 * ------------------------------------------------------------------------*/

static int recurrent_or_none(ortho_t *o, char *out)
{
    ortho_prng *r = &o->rng;
    const ortho_dials *d = &o->dials;

    /* 1. drain — phrase-first, atomic */
    if (o->phrase_queue_pos < o->phrase_queue_len) {
        str_copy(out, o->phrase_queue[o->phrase_queue_pos]);
        o->phrase_queue_pos++;
        o->last_source = ORTHO_SRC_PHRASE;
        return 1;
    }

    /* 2. short-circuit: all recurrence off -> zero PRNG draws */
    if (d->phrases <= 0.0 && d->function_words <= 0.0 &&
        d->topics <= 0.0 && d->names <= 0.0) {
        o->last_source = ORTHO_SRC_FRESH;
        return 0;
    }

    if (!o->has_section) ortho_new_section(o);

    /* 3. independent rolls, fixed order, first hit wins */
    if (d->phrases > 0.0 && ortho_prng_next(r) < d->phrases) {
        int p = ortho_prng_below(r, ORTHO_N_SECTION_PHRASES);
        int len = o->sec_phrase_len[p];
        int i;
        /* queue the tail; return word 0 now */
        o->phrase_queue_len = 0;
        o->phrase_queue_pos = 0;
        for (i = 1; i < len; i++) {
            str_copy(o->phrase_queue[o->phrase_queue_len], o->sec_phrases[p][i]);
            o->phrase_queue_len++;
        }
        str_copy(out, o->sec_phrases[p][0]);
        o->last_source = ORTHO_SRC_PHRASE;
        return 1;
    }
    if (d->function_words > 0.0 && ortho_prng_next(r) < d->function_words) {
        str_copy(out, o->function_words[ortho_prng_below(r, ORTHO_N_FUNCTION_WORDS)]);
        o->last_source = ORTHO_SRC_FUNCTION;
        return 1;
    }
    if (d->topics > 0.0 && ortho_prng_next(r) < d->topics) {
        str_copy(out, o->sec_topics[ortho_prng_below(r, ORTHO_N_SECTION_TOPICS)]);
        o->last_source = ORTHO_SRC_TOPIC;
        return 1;
    }
    if (d->names > 0.0 && ortho_prng_next(r) < d->names) {
        str_copy(out, o->sec_names[ortho_prng_below(r, ORTHO_N_SECTION_NAMES)]);
        o->last_source = ORTHO_SRC_NAME;
        return 1;
    }

    o->last_source = ORTHO_SRC_FRESH;
    return 0;
}

/* --------------------------------------------------------------------------
 * Init
 * ------------------------------------------------------------------------*/

void ortho_init(ortho_t *o, uint32_t seed, const ortho_dials *dials)
{
    memset(o, 0, sizeof(*o));
    o->seed = seed;
    ortho_prng_init(&o->rng, seed);
    ortho_set_dials(o, dials);
    o->has_section = 0;
    o->phrase_queue_len = 0;
    o->phrase_queue_pos = 0;
    o->last_source = ORTHO_SRC_FRESH;
    build_substrate(o);
}

/* --------------------------------------------------------------------------
 * Generation
 * ------------------------------------------------------------------------*/

int ortho_tokens(ortho_t *o, int n, int max_letters,
                 ortho_token *buf, size_t capacity)
{
    int count = n;
    int i;
    int written = 0;

    if (count < 0) count = 0;
    if (max_letters <= 0) max_letters = 8;

    for (i = 0; i < count; i++) {
        char w[ORTHO_MAX_TOKEN];
        uint8_t src;
        if (!recurrent_or_none(o, w)) {
            int len = 1 + ortho_prng_below(&o->rng, max_letters);
            ortho_word(o, len, 1, w);
            src = ORTHO_SRC_FRESH;
        } else {
            src = o->last_source;
        }
        if ((size_t)written < capacity) {
            str_copy(buf[written].text, w);
            buf[written].source = src;
            buf[written].flags = 0;
            written++;
        }
    }
    return written;
}

/* Punctuation pass (SPEC.md §8). Mutates token TEXT only; never changes count.
 * Zero PRNG draws when all three punctuation dials are 0. */
static void punctuate(ortho_t *o, ortho_token *buf, int n)
{
    const ortho_dials *d = &o->dials;
    ortho_prng *r = &o->rng;
    char claimed[256];
    int i;

    if (d->commas <= 0.0 && d->quotation <= 0.0 && d->scare_quotes <= 0.0) return;
    if (n <= 0) return;
    if (n > 256) n = 256;

    for (i = 0; i < n; i++) claimed[i] = 0;

    /* direct quotation — speaker-anchored */
    if (d->quotation > 0.0 && n >= 4 && ortho_prng_next(r) < d->quotation) {
        int span = 2 + ortho_prng_below(r, 3);
        int room = n - span - 1;
        int start = 1 + ortho_prng_below(r, room > 0 ? room : 1);
        int end = start + span - 1;
        if (end > n - 1) end = n - 1;
        if (start < n && end < n && start <= end) {
            char tmp[ORTHO_MAX_TOKEN];
            tmp[0] = '"'; tmp[1] = '\0';
            str_append(tmp, buf[start].text);
            str_copy(buf[start].text, tmp);
            str_append(buf[end].text, "\"");
            claimed[start] = 1;
            claimed[end] = 1;
            if (o->has_section && start - 1 >= 0) {
                str_copy(buf[start - 1].text,
                         o->sec_names[ortho_prng_below(r, ORTHO_N_SECTION_NAMES)]);
                buf[start - 1].source = ORTHO_SRC_NAME;
                claimed[start - 1] = 1;
            }
        }
    }

    /* scare quotes — single term */
    if (d->scare_quotes > 0.0 && n >= 2 && ortho_prng_next(r) < d->scare_quotes) {
        int i2 = 1 + ortho_prng_below(r, n - 1);
        if (i2 < n && !claimed[i2]) {
            char tmp[ORTHO_MAX_TOKEN];
            tmp[0] = '"'; tmp[1] = '\0';
            str_append(tmp, buf[i2].text);
            str_append(tmp, "\"");
            str_copy(buf[i2].text, tmp);
            claimed[i2] = 1;
        }
    }

    /* commas — function-word-anchored, rhythmic fallback */
    if (d->commas > 0.0 && n >= 3) {
        int lo = (int)((double)n * 0.25);
        int since = 99;
        if (lo < 1) lo = 1;
        for (i = 1; i < n - 1; i++) {
            int next_is_function = 0;
            int j;
            since++;
            if (claimed[i]) continue;
            for (j = 0; j < ORTHO_N_FUNCTION_WORDS; j++) {
                if (strcmp(buf[i + 1].text, o->function_words[j]) == 0) {
                    next_is_function = 1;
                    break;
                }
            }
            if (next_is_function && since >= 2) {
                if (ortho_prng_next(r) < d->commas) {
                    str_append(buf[i].text, ",");
                    since = 0;
                }
            } else if (i >= lo && since >= 3) {
                if (ortho_prng_next(r) < d->commas * 0.35) {
                    str_append(buf[i].text, ",");
                    since = 0;
                }
            }
        }
    }
}

int ortho_sentence(ortho_t *o, int num_words, int max_letters,
                   ortho_token *buf, size_t capacity)
{
    int count = num_words;
    int i;
    int written = 0;
    char last[ORTHO_MAX_TOKEN];

    if (count <= 0) count = 1;
    if (max_letters <= 0) max_letters = 8;
    last[0] = '\0';

    for (i = 0; i < count; i++) {
        char w[ORTHO_MAX_TOKEN];
        uint8_t src;
        if (!recurrent_or_none(o, w)) {
            ortho_word(o, ortho_prng_below(&o->rng, max_letters), 1, w);
            src = ORTHO_SRC_FRESH;
        } else {
            src = o->last_source;
        }
        /* promote a run of tiny words to a function word */
        if (i > 0 && i < count - 1 && strlen(w) <= 1 && strlen(last) <= 1) {
            str_copy(w, o->function_words[
                ortho_prng_below(&o->rng, ORTHO_N_FUNCTION_WORDS)]);
            src = ORTHO_SRC_FUNCTION;
        }
        if (i == count - 1 && strlen(w) <= 1) {
            str_copy(w, o->function_words[
                ortho_prng_below(&o->rng, ORTHO_N_FUNCTION_WORDS)]);
            src = ORTHO_SRC_FUNCTION;
        }
        if ((size_t)written < capacity) {
            str_copy(buf[written].text, w);
            buf[written].source = src;
            buf[written].flags = 0;
            written++;
        }
        str_copy(last, w);
    }

    punctuate(o, buf, written);

    if (written > 0) {
        buf[0].text[0] = (char)toupper((unsigned char)buf[0].text[0]);
        str_append(buf[written - 1].text,
                   (const char[]){ PUNCTUATION[ortho_prng_below(&o->rng, N_PUNCT)], '\0' });
    }
    return written;
}

int ortho_paragraph(ortho_t *o, int num_sentences, int max_words,
                    int max_letters, ortho_token *buf, size_t capacity)
{
    int s = num_sentences;
    int i;
    int written = 0;

    if (s <= 0) s = 1;
    if (max_words <= 3) max_words = 7;
    if (max_letters <= 3) max_letters = 5;

    for (i = 0; i < s; i++) {
        int nw = ortho_prng_below(&o->rng, max_words);
        /* SPEC 2.0 §9: max_words is drawn below, max_letters is passed
         * through. ortho_sentence already draws each word's length below its
         * argument; reducing twice put paragraph words under the digraph band
         * in §5.1 step 5, so they carried nothing seed-specific. */
        written += ortho_sentence(o, nw, max_letters,
                                  buf + written,
                                  (capacity > (size_t)written)
                                      ? capacity - (size_t)written : 0);
    }
    return written;
}
