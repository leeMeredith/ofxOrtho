/*
 * ofxOrtho.cpp — translation layer over ortho-kernel.
 *
 * Every function here converts between C++ types and the kernel's
 * caller-owned buffers. No language decisions are made in this file.
 */

#include "ofxOrtho.h"

#include <cstring>

namespace {

/* Clamp to the dial domain. The kernel clamps too; doing it here keeps the
 * host-side `dials` struct honest for anyone inspecting it. */
double clamp01(double v)
{
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

/* Scratch buffer for generation calls. Sized generously; the kernel never
 * writes more than the capacity it is handed. */
const size_t kScratchCap = 4096;

} // namespace

ofxOrtho::ofxOrtho()
{
    std::memset(&engine, 0, sizeof(engine));
    ortho_dials_clear(&dials);
    for (int i = 0; i < DIAL_COUNT; ++i) explicitSet[i] = false;
}

void ofxOrtho::setup(uint32_t s)
{
    seed = s;
    /* Dials set before setup() are carried in, not discarded. */
    ortho_init(&engine, seed, &dials);
    started = true;
}

/* -------------------------------------------------------------------------
 * Dials
 *
 * The resulting value of each dial is:
 *     explicit value, if the user set it by hand
 *     else preset-derived value, if a preset is active
 *     else 0
 *
 * This is recomputed from scratch on every change, which is what makes call
 * order irrelevant.
 * ---------------------------------------------------------------------- */

void ofxOrtho::recomputeDials()
{
    ortho_dials base;
    ortho_dials_clear(&base);
    if (preset > 0.0) ortho_dials_preset(&base, preset);

    /* Explicit settings win over the preset-derived baseline. */
    double *dst[DIAL_COUNT] = {
        &base.phrases, &base.function_words, &base.topics, &base.names,
        &base.commas,  &base.quotation,      &base.scare_quotes
    };
    const double *cur[DIAL_COUNT] = {
        &dials.phrases, &dials.function_words, &dials.topics, &dials.names,
        &dials.commas,  &dials.quotation,      &dials.scare_quotes
    };

    for (int i = 0; i < DIAL_COUNT; ++i) {
        if (explicitSet[i]) *dst[i] = *cur[i];
    }

    dials = base;
    if (started) ortho_set_dials(&engine, &dials);
}

void ofxOrtho::setDial(int which, double v)
{
    double *dst[DIAL_COUNT] = {
        &dials.phrases, &dials.function_words, &dials.topics, &dials.names,
        &dials.commas,  &dials.quotation,      &dials.scare_quotes
    };
    *dst[which]        = clamp01(v);
    explicitSet[which] = true;
    recomputeDials();
}

void ofxOrtho::setPhrases(double v)       { setDial(DIAL_PHRASES, v); }
void ofxOrtho::setFunctionWords(double v) { setDial(DIAL_FUNCTION_WORDS, v); }
void ofxOrtho::setTopics(double v)        { setDial(DIAL_TOPICS, v); }
void ofxOrtho::setNames(double v)         { setDial(DIAL_NAMES, v); }
void ofxOrtho::setCommas(double v)        { setDial(DIAL_COMMAS, v); }
void ofxOrtho::setQuotation(double v)     { setDial(DIAL_QUOTATION, v); }
void ofxOrtho::setScareQuotes(double v)   { setDial(DIAL_SCARE_QUOTES, v); }

void ofxOrtho::setPreset(double v)
{
    preset = clamp01(v);
    recomputeDials();
}

void ofxOrtho::clearDials()
{
    preset = 0.0;
    for (int i = 0; i < DIAL_COUNT; ++i) explicitSet[i] = false;
    ortho_dials_clear(&dials);
    if (started) ortho_set_dials(&engine, &dials);
}

/* -------------------------------------------------------------------------
 * Generation
 * ---------------------------------------------------------------------- */

std::vector<ofxOrtho::Token> ofxOrtho::tokensWithSource(int n, int maxLetters)
{
    std::vector<Token> out;
    if (!started || n <= 0) return out;

    std::vector<ortho_token> buf(kScratchCap);
    int wrote = ortho_tokens(&engine, n, maxLetters, buf.data(), buf.size());

    out.reserve(static_cast<size_t>(wrote));
    for (int i = 0; i < wrote; ++i) {
        Token t;
        t.text   = buf[i].text;
        t.source = buf[i].source;
        out.push_back(t);
    }
    return out;
}

std::vector<std::string> ofxOrtho::tokens(int n, int maxLetters)
{
    std::vector<std::string> out;
    if (!started || n <= 0) return out;

    std::vector<ortho_token> buf(kScratchCap);
    int wrote = ortho_tokens(&engine, n, maxLetters, buf.data(), buf.size());

    out.reserve(static_cast<size_t>(wrote));
    for (int i = 0; i < wrote; ++i) out.push_back(buf[i].text);
    return out;
}

std::vector<std::string> ofxOrtho::sentence(int numWords, int maxLetters)
{
    std::vector<std::string> out;
    if (!started || numWords <= 0) return out;

    std::vector<ortho_token> buf(kScratchCap);
    int wrote = ortho_sentence(&engine, numWords, maxLetters,
                               buf.data(), buf.size());

    out.reserve(static_cast<size_t>(wrote));
    for (int i = 0; i < wrote; ++i) out.push_back(buf[i].text);
    return out;
}

std::vector<std::string> ofxOrtho::paragraph(int numSentences, int maxWords,
                                             int maxLetters)
{
    std::vector<std::string> out;
    if (!started || numSentences <= 0) return out;

    std::vector<ortho_token> buf(kScratchCap);
    int wrote = ortho_paragraph(&engine, numSentences, maxWords, maxLetters,
                                buf.data(), buf.size());

    out.reserve(static_cast<size_t>(wrote));
    for (int i = 0; i < wrote; ++i) out.push_back(buf[i].text);
    return out;
}

std::string ofxOrtho::word(int numLetters, bool allowContractions)
{
    if (!started) return std::string();

    char out[ORTHO_MAX_TOKEN];
    out[0] = '\0';
    ortho_word(&engine, numLetters, allowContractions ? 1 : 0, out);
    return std::string(out);
}

void ofxOrtho::newSection()
{
    if (!started) return;
    ortho_new_section(&engine);
}
