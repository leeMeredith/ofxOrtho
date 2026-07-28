/*
 * ofxOrtho.cpp — translation layer over ortho-kernel.
 *
 * Every function here converts between C++ types and the kernel's
 * caller-owned buffers. No language decisions are made in this file.
 */

#include "ofxOrtho.h"

#include <cstring>

/* Reporting goes through oF's logger when this is built as an addon, and to
 * stderr when it is not. The conformance harness compiles this file with plain
 * c++ and no openFrameworks — building GLFW proves nothing about language
 * conformance — so ofLog.h cannot be a hard dependency. */
#if defined(__has_include)
#  if __has_include("ofLog.h")
#    include "ofLog.h"
#    define OFXORTHO_HAS_OFLOG 1
#  endif
#endif

#if defined(OFXORTHO_HAS_OFLOG)
#  define OFXORTHO_WARN(expr) ofLogWarning("ofxOrtho") expr
#else
#  include <iostream>
#  define OFXORTHO_WARN(expr) std::cerr << "[ofxOrtho] " expr << std::endl
#endif

namespace {

/* Clamp to the dial domain. The kernel clamps too; doing it here keeps the
 * host-side `dials` struct honest for anyone inspecting it. */
double clamp01(double v)
{
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

} // namespace

/* The kernel writes what fits and returns the count; it never reports. Telling
 * the user is the host's job — HOSTS.md section 3 requires it, so that a request
 * that quietly came back short is visible rather than mysterious. */
int ofxOrtho::capRequest(int n, const char *what) const
{
    if (n > static_cast<int>(kCapacity)) {
        OFXORTHO_WARN(<< what << ": requested " << n
                      << ", capacity is " << kCapacity
                      << " — returning " << kCapacity);
        return static_cast<int>(kCapacity);
    }
    return n;
}

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

    n = capRequest(n, "tokens");
    int wrote = ortho_tokens(&engine, n, maxLetters, scratch, kCapacity);

    out.reserve(static_cast<size_t>(wrote));
    for (int i = 0; i < wrote; ++i) {
        Token t;
        t.text   = scratch[i].text;
        t.source = scratch[i].source;
        out.push_back(t);
    }
    return out;
}

std::vector<std::string> ofxOrtho::tokens(int n, int maxLetters)
{
    std::vector<std::string> out;
    if (!started || n <= 0) return out;

    n = capRequest(n, "tokens");
    int wrote = ortho_tokens(&engine, n, maxLetters, scratch, kCapacity);

    out.reserve(static_cast<size_t>(wrote));
    for (int i = 0; i < wrote; ++i) out.push_back(scratch[i].text);
    return out;
}

std::vector<std::string> ofxOrtho::sentence(int numWords, int maxLetters)
{
    std::vector<std::string> out;
    if (!started || numWords <= 0) return out;

    numWords = capRequest(numWords, "sentence");
    int wrote = ortho_sentence(&engine, numWords, maxLetters,
                               scratch, kCapacity);

    out.reserve(static_cast<size_t>(wrote));
    for (int i = 0; i < wrote; ++i) out.push_back(scratch[i].text);
    return out;
}

std::vector<std::string> ofxOrtho::paragraph(int numSentences, int maxWords,
                                             int maxLetters)
{
    std::vector<std::string> out;
    if (!started || numSentences <= 0) return out;

    int wrote = ortho_paragraph(&engine, numSentences, maxWords, maxLetters,
                                scratch, kCapacity);
    if (wrote == static_cast<int>(kCapacity))
        OFXORTHO_WARN(<< "paragraph: filled the " << kCapacity
                      << "-token buffer; output may be truncated");

    out.reserve(static_cast<size_t>(wrote));
    for (int i = 0; i < wrote; ++i) out.push_back(scratch[i].text);
    return out;
}

std::vector<ofxOrtho::Token> ofxOrtho::sentenceWithSource(int numWords,
                                                          int maxLetters)
{
    std::vector<Token> out;
    if (!started || numWords <= 0) return out;

    numWords = capRequest(numWords, "sentence");
    int wrote = ortho_sentence(&engine, numWords, maxLetters,
                               scratch, kCapacity);

    out.reserve(static_cast<size_t>(wrote));
    for (int i = 0; i < wrote; ++i) {
        Token t;
        t.text   = scratch[i].text;
        t.source = scratch[i].source;
        out.push_back(t);
    }
    return out;
}

std::vector<ofxOrtho::Token> ofxOrtho::paragraphWithSource(int numSentences,
                                                           int maxWords,
                                                           int maxLetters)
{
    std::vector<Token> out;
    if (!started || numSentences <= 0) return out;

    int wrote = ortho_paragraph(&engine, numSentences, maxWords, maxLetters,
                                scratch, kCapacity);
    if (wrote == static_cast<int>(kCapacity))
        OFXORTHO_WARN(<< "paragraph: filled the " << kCapacity
                      << "-token buffer; output may be truncated");

    out.reserve(static_cast<size_t>(wrote));
    for (int i = 0; i < wrote; ++i) {
        Token t;
        t.text   = scratch[i].text;
        t.source = scratch[i].source;
        out.push_back(t);
    }
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
