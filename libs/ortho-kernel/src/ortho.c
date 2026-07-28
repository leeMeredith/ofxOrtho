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
#include <math.h>
#include <ctype.h>

/* --------------------------------------------------------------------------
 * Fixed character canon (SPEC.md §3)
 * ------------------------------------------------------------------------*/

static const char CONSONANTS[]  = "bcdfghjklmnpqrstvwxz";        /* 20 */
static const char VOWELS[]      = "aeiouy";                      /*  6 */

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

/* --------------------------------------------------------------------------
 * Spec 3.0 draw helpers (SPEC.md §4.1)
 * ------------------------------------------------------------------------*/

/* Cumulative weight table. `mix` blends toward uniform: exponent alone is
 * insufficient, because over a 4-item set raw random weights already leave one
 * member holding roughly half the mass. */
static void make_weights(ortho_prng *r, double *cum, int n, double e, double mix)
{
    double w[32];
    double sum = 0.0, acc = 0.0;
    int i;
    for (i = 0; i < n; i++) {
        w[i] = (1.0 - mix) / (double)n + mix * pow(ortho_prng_next(r), e);
        sum += w[i];
    }
    for (i = 0; i < n; i++) { acc += w[i] / sum; cum[i] = acc; }
}

/* k distinct characters drawn from pool, in draw order (not pool order). */
static void make_subset(ortho_prng *r, const char *pool, int pooln, char *out, int k)
{
    char avail[32];
    int n = pooln, i, j, idx;
    for (i = 0; i < n; i++) avail[i] = pool[i];
    for (i = 0; i < k && n > 0; i++) {
        idx = ortho_prng_below(r, n);
        out[i] = avail[idx];
        for (j = idx; j < n - 1; j++) avail[j] = avail[j + 1];
        n--;
    }
    out[k] = '\0';
}

/* First index whose cumulative weight exceeds a fresh draw. */
static void ortho_run(ortho_t *o, int len, char *out);
static int wpick(ortho_prng *r, const double *cum, int n)
{
    double x = ortho_prng_next(r);
    int i;
    for (i = 0; i < n; i++) if (x < cum[i]) return i;
    return n - 1;
}

/* One root-shaped run (SPEC §5.1 step 4).
 *
 * Length rounds UP to a whole number of roots, minimum one, so a word is an
 * honest instance of the language's shape rather than a truncation of it.
 *
 * Adjacent same-class slots take a cluster from this language's own tables.
 * That is where digraphs and trigraphs belong: they ARE the permitted
 * clusters. Spec 2.x spliced them over a finished word at fixed length bands,
 * which appended clusters to templates that forbid them — a CVCV language
 * produced words like `rirgh`. Under this rule a CVCV language correctly has
 * no clusters at all, exactly as Hawaiian has none. */
static void ortho_run(ortho_t *o, int len, char *out)
{
    ortho_prng *r = &o->rng;
    char slots[ORTHO_MAX_TOKEN];
    int reps, i, k, si = 0;
    double q;

    q = (double)len / (double)o->root_len;
    reps = (int)(q + 0.5);
    if (reps < 1) reps = 1;

    slots[0] = '\0';
    for (i = 0; i < reps; i++) {
        int t;
        for (t = 0; t < o->root_len; t++) {
            if (si < ORTHO_MAX_TOKEN - 1) slots[si++] = o->root[t];
        }
    }
    slots[si] = '\0';

    out[0] = '\0';
    i = 0;
    while (i < si) {
        if (slots[i] == 'V') {
            int vk = 0;
            while (i + vk < si && slots[i + vk] == 'V') vk++;
            if (vk >= 2 && o->vowel_digraph_count > 0) {
                str_append(out, o->vowel_digraphs[
                    ortho_prng_below(r, o->vowel_digraph_count)]);
                i += 2;
                continue;
            }
            {
                char ch = o->vowel_set[wpick(r, o->vowel_w, o->vowel_count)];
                size_t L = strlen(out);
                /* Single redraw, never a loop: a small inventory would spin.
                 * A doubled letter at a slot boundary reads as a stutter. */
                if (L > 0 && ch == out[L - 1])
                    ch = o->vowel_set[wpick(r, o->vowel_w, o->vowel_count)];
                str_append_char(out, ch);
            }
            i++;
            continue;
        }
        k = 0;
        while (i + k < si && slots[i + k] == 'C') k++;
        if (k >= 3) {
            str_append(out, o->consonant_trigraphs[
                ortho_prng_below(r, ORTHO_N_CONSONANT_TRIGRAPHS)]);
            i += 3;
        } else if (k >= 2) {
            str_append(out, o->consonant_digraphs[
                ortho_prng_below(r, ORTHO_N_CONSONANT_DIGRAPHS)]);
            i += 2;
        } else {
            char ch = o->cons_set[wpick(r, o->cons_w, o->cons_count)];
            size_t L = strlen(out);
            if (L > 0 && ch == out[L - 1])
                ch = o->cons_set[wpick(r, o->cons_w, o->cons_count)];
            str_append_char(out, ch);
            i++;
        }
    }
}

int ortho_word(ortho_t *o, int num_letters, int allow_contractions, char *out)
{
    ortho_prng *r = &o->rng;
    int n = num_letters;

    out[0] = '\0';
    if (n <= 0) n = 1;

    /* --- particle (SPEC §5.1 step 2) ---------------------------------------
     * Only build_function_words asks for length 1. A one-letter FUNCTION word
     * is a particle — the equivalent of English `a` or `I`. A one-letter
     * CONTENT word is a truncation artifact, which is why run() below floors
     * everything else at one whole root. */
    if (n == 1) {
        int use_vowel = (ortho_prng_below(r, 3) == 0);
        out[0] = use_vowel
            ? o->vowel_set[wpick(r, o->vowel_w, o->vowel_count)]
            : o->cons_set[wpick(r, o->cons_w, o->cons_count)];
        out[1] = '\0';
        return 1;
    }

    /* --- compound decision (SPEC §5.1 step 3) ---------------------------- */
    if (o->compounds && n >= 6 && ortho_prng_below(r, 3) == 0) {
        char a[ORTHO_MAX_TOKEN], b[ORTHO_MAX_TOKEN];
        ortho_run(o, (n + 1) / 2, a);
        ortho_run(o, n / 2, b);
        str_copy(out, a);
        str_append_char(out, '-');
        str_append(out, b);
    } else {
        ortho_run(o, n, out);
    }

    if (allow_contractions && ortho_prng_below(r, 4) == 0) {
        str_append(out, o->contractions[ortho_prng_below(r, ORTHO_N_CONTRACTIONS)]);
    }

    cluster_guard(out);
    return (int)strlen(out);
}

/* --------------------------------------------------------------------------
 * Substrate (SPEC.md §4.2) — built once, in this exact order.
 *
 * DRAW ORDER IS NORMATIVE. A host that draws the same values in a different
 * sequence produces a different language and fails conformance.
 * ------------------------------------------------------------------------*/

/* Root templates and their consonant-weight bias (SPEC §4.2 step 1). Vowel-
 * leading and vowel-cluster shapes carry a consonant-favouring bias so the two
 * cannot compound into a language that is nearly all vowels. */
static const char *ROOT_SHAPES[10] = {
    "CVCV", "CVC", "CCVC", "CVCC", "CVCVC", "CCVCV", "VCVC", "CVCCV", "CVV", "CVVC"
};
static const double ROOT_BIAS[10] = {
    1.0, 1.0, 1.2, 1.2, 1.0, 1.1, 0.8, 1.1, 1.3, 1.2
};

static void build_substrate(ortho_t *o)
{
    ortho_prng *r = &o->rng;
    int k, i, j, ri;
    double bias;
    char alpha[ORTHO_N_CONSONANTS + ORTHO_N_VOWELS + 1];
    int nalpha;

    /* 1. root template */
    ri = ortho_prng_below(r, 10);
    str_copy(o->root, ROOT_SHAPES[ri]);
    o->root_len = (int)strlen(o->root);
    bias = ROOT_BIAS[ri];

    /* 2. phoneme inventory. Never fewer than 4 vowels: with any lopsided
     * weight a 3-vowel language collapses onto one and reads as a stutter. */
    o->cons_count  = 6 + ortho_prng_below(r, 15);
    o->vowel_count = 4 + ortho_prng_below(r, 3);
    make_subset(r, CONSONANTS, ORTHO_N_CONSONANTS, o->cons_set,  o->cons_count);
    make_subset(r, VOWELS,     ORTHO_N_VOWELS,     o->vowel_set, o->vowel_count);

    /* 3. letter weights. These COMPENSATE for inventory size rather than
     * compounding with it: subset and weight are both narrowing devices, and
     * at full strength together they leave one letter doing all the work. */
    make_weights(r, o->vowel_w, o->vowel_count, 1.4,
                 0.20 + 0.55 * ((double)o->vowel_count / (double)ORTHO_N_VOWELS));
    make_weights(r, o->cons_w, o->cons_count, 1.4 * bias,
                 0.25 + 0.55 * ((double)o->cons_count / (double)ORTHO_N_CONSONANTS));

    /* 4. clause mark */
    switch (ortho_prng_below(r, 4)) {
        case 0:  str_copy(o->clause_mark, ",");   break;
        case 1:  str_copy(o->clause_mark, ";");   break;
        case 2:  str_copy(o->clause_mark, ":");   break;
        default: str_copy(o->clause_mark, "\xe2\x80\x94"); break;  /* em dash */
    }

    /* 5. quote pair */
    switch (ortho_prng_below(r, 3)) {
        case 0:
            str_copy(o->quote_open,  "\"");
            str_copy(o->quote_close, "\"");
            break;
        case 1:
            str_copy(o->quote_open,  "\xc2\xab");   /* << */
            str_copy(o->quote_close, "\xc2\xbb");   /* >> */
            break;
        default:
            str_copy(o->quote_open,  "\xe2\x80\xb9");  /* single << */
            str_copy(o->quote_close, "\xe2\x80\xba");  /* single >> */
            break;
    }

    /* 6. quoted capitalisation */
    o->capitalize_quoted = (ortho_prng_below(r, 2) == 0);

    /* 7. terminal marks — TWO draws always, regardless of outcome */
    str_copy(o->terminals, ".");
    if (ortho_prng_below(r, 2) == 0) str_append_char(o->terminals, '?');
    if (ortho_prng_below(r, 2) == 0) str_append_char(o->terminals, '!');

    /* 8. compounding */
    o->compounds = (ortho_prng_below(r, 4) == 0);

    /* 9. vowel digraphs — every ordered pair of DISTINCT letters from this
     * language's vowel subset. NO PRNG DRAWS. A doubled vowel is a long
     * vowel, not a cluster. */
    k = 0;
    for (i = 0; i < o->vowel_count; i++) {
        for (j = 0; j < o->vowel_count; j++) {
            if (i == j) continue;
            if (k >= ORTHO_N_VOWEL_DIGRAPHS) break;
            o->vowel_digraphs[k][0] = o->vowel_set[i];
            o->vowel_digraphs[k][1] = o->vowel_set[j];
            o->vowel_digraphs[k][2] = '\0';
            k++;
        }
    }
    o->vowel_digraph_count = k;

    /* 10. consonant digraphs — from this language's own consonants */
    for (k = 0; k < ORTHO_N_CONSONANT_DIGRAPHS; k++) {
        int a = ortho_prng_below(r, o->cons_count);
        int b = ortho_prng_below(r, o->cons_count);
        while (a == b) b = ortho_prng_below(r, o->cons_count);
        o->consonant_digraphs[k][0] = o->cons_set[a];
        o->consonant_digraphs[k][1] = o->cons_set[b];
        o->consonant_digraphs[k][2] = '\0';
    }

    /* 11. consonant trigraphs, de-duplicated positionally */
    for (k = 0; k < ORTHO_N_CONSONANT_TRIGRAPHS; k++) {
        int idx[3];
        int a, b;
        idx[0] = ortho_prng_below(r, o->cons_count);
        idx[1] = ortho_prng_below(r, o->cons_count);
        idx[2] = ortho_prng_below(r, o->cons_count);
        for (a = 0; a < 3; a++) {
            for (b = 0; b < 3; b++) {
                if (a != b) {
                    while (idx[a] == idx[b]) idx[b] = ortho_prng_below(r, o->cons_count);
                }
            }
        }
        o->consonant_trigraphs[k][0] = o->cons_set[idx[0]];
        o->consonant_trigraphs[k][1] = o->cons_set[idx[1]];
        o->consonant_trigraphs[k][2] = o->cons_set[idx[2]];
        o->consonant_trigraphs[k][3] = '\0';
    }

    /* 12. contractions — drawn from consonants THEN vowels, in that order */
    nalpha = 0;
    for (i = 0; i < o->cons_count; i++)  alpha[nalpha++] = o->cons_set[i];
    for (i = 0; i < o->vowel_count; i++) alpha[nalpha++] = o->vowel_set[i];
    alpha[nalpha] = '\0';

    for (k = 0; k < ORTHO_N_CONTRACTIONS; k++) {
        if (k < 5) {
            int a = ortho_prng_below(r, nalpha);
            int b = ortho_prng_below(r, nalpha);
            o->contractions[k][0] = '\'';
            o->contractions[k][1] = alpha[a];
            o->contractions[k][2] = alpha[b];
            o->contractions[k][3] = '\0';
        } else {
            int a = ortho_prng_below(r, nalpha);
            o->contractions[k][0] = '\'';
            o->contractions[k][1] = alpha[a];
            o->contractions[k][2] = '\0';
        }
    }

    /* 13. names (SPEC §5.3) */
    for (k = 0; k < ORTHO_N_NAMES; k++) {
        int len = ortho_prng_below(r, 10);
        if (len < 3) len = 5;
        ortho_word(o, len, 0, o->names[k]);
        if (o->names[k][0] != '\0') {
            o->names[k][0] = (char)toupper((unsigned char)o->names[k][0]);
        }
    }

    /* 14. function words (SPEC §5.4). The size counter starts at ONE, which
     * is what gives a language its particles. */
    {
        int size = 1, count = 0;
        int step = ORTHO_N_FUNCTION_WORDS / 4;
        for (k = 0; k < ORTHO_N_FUNCTION_WORDS; k++) {
            count++;
            if (count == step) { size++; count = 0; }
            ortho_word(o, size, 0, o->function_words[k]);
        }
    }
}

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
            /* SPEC 3.0 §8: the dial decides how often, the seed decides which
             * mark. A capitalised span reads as an utterance someone said; a
             * lowercase one reads as a term held up for inspection. */
            if (o->capitalize_quoted && buf[start].text[0] != '\0') {
                buf[start].text[0] =
                    (char)toupper((unsigned char)buf[start].text[0]);
            }
            str_copy(tmp, o->quote_open);
            str_append(tmp, buf[start].text);
            str_copy(buf[start].text, tmp);
            str_append(buf[end].text, o->quote_close);
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
            str_copy(tmp, o->quote_open);
            str_append(tmp, buf[i2].text);
            str_append(tmp, o->quote_close);
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
                    str_append(buf[i].text, o->clause_mark);
                    since = 0;
                }
            } else if (i >= lo && since >= 3) {
                if (ortho_prng_next(r) < d->commas * 0.35) {
                    str_append(buf[i].text, o->clause_mark);
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
                   (const char[]){ o->terminals[ortho_prng_below(&o->rng,
                       (int)strlen(o->terminals))], '\0' });
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
