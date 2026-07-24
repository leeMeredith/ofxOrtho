/*
 * ofxOrtho.h — openFrameworks addon for the ortho invented-language kernel.
 *
 * DESIGN RULE: the wrapper performs TRANSLATION, not INTERPRETATION.
 *
 *   vector<string>  <->  ortho_token[]  <->  kernel        GOOD
 *   "if topics > .6 then behave differently"                BAD
 *
 * Every public method maps to exactly one kernel concept. Any language logic
 * living here instead of the kernel is a bug. If this file starts getting
 * smarter than ortho-kernel, stop and reconsider.
 *
 * Persistent state is ONLY the engine and the dials. Generation parameters
 * (n, maxLetters, ...) are arguments, never remembered between calls.
 *
 * Conformance: tokensWithSource() must diff clean against the v2 golden
 * vectors published by the `ortho` reference repo.
 */

#pragma once

/* The language engine lives in a git submodule at libs/ortho-kernel. GitHub's
 * "Download ZIP" does not include submodule contents, so a ZIP download gets
 * an empty directory and an otherwise baffling missing-header error. Catch it
 * here with something actionable instead. */
#if defined(__has_include)
#  if !__has_include("ortho.h")
#    error "ofxOrtho: ortho-kernel is missing. If you downloaded a ZIP, submodules are NOT included - clone instead: git clone --recursive https://github.com/leeMeredith/ofxOrtho.git   Already cloned? Run: git submodule update --init --recursive"
#  endif
#endif

#include "ortho.h"

#include <cstdint>
#include <string>
#include <vector>

class ofxOrtho {
public:
    /* One token plus its origin. Useful for visual work: colour by source,
     * animate names differently, spawn on NAME, etc. `source` holds an
     * ORTHO_SRC_* value (0 fresh, 1 function, 2 topic, 3 name, 4 phrase). */
    struct Token {
        std::string text;
        uint8_t     source = ORTHO_SRC_FRESH;
    };

    ofxOrtho();

    /* Seed names the language. Mints the substrate. Any dials set before this
     * call are carried in, so setTopics()-then-setup() is not punished. */
    void setup(uint32_t seed);

    /* True once setup() has run. Generation before setup returns empty. */
    bool isSetup() const { return started; }

    uint32_t getSeed() const { return seed; }

    /* ---- the seven dials — frozen vocabulary, mirrors the kernel ---------
     * Each takes a double in [0,1]. Calling a setter marks that dial as
     * EXPLICIT, so a later setPreset() will not overwrite it. Call order
     * therefore does not affect the resulting dial values. */
    void setPhrases(double v);
    void setFunctionWords(double v);
    void setTopics(double v);
    void setNames(double v);
    void setCommas(double v);
    void setQuotation(double v);
    void setScareQuotes(double v);

    /* One-knob onramp. Fills only the dials not explicitly set by hand.
     * Glue-level convenience; consumes no PRNG draws. */
    void setPreset(double v);

    /* Forget all explicit markings and zero every dial. */
    void clearDials();

    /* ---- generation ------------------------------------------------------
     * Count-exact, punctuation-free. The harness contract and vector path. */
    std::vector<std::string> tokens(int n, int maxLetters = 8);
    std::vector<Token>       tokensWithSource(int n, int maxLetters = 8);

    /* Readable path: capitalisation, terminal marks, punctuation per dials. */
    std::vector<std::string> sentence(int numWords, int maxLetters = 8);
    std::vector<std::string> paragraph(int numSentences, int maxWords = 12,
                                       int maxLetters = 8);

    /* Readable path, origin-bearing. The kernel already populates .source on
     * every token it writes, including on this path - these simply stop
     * discarding it. Punctuation is baked into .text by the kernel post-pass;
     * .source is untouched by that pass. Nothing is inferred here. */
    std::vector<Token> sentenceWithSource(int numWords, int maxLetters = 8);
    std::vector<Token> paragraphWithSource(int numSentences, int maxWords = 12,
                                           int maxLetters = 8);

    /* One word. */
    std::string word(int numLetters, bool allowContractions = true);

    /* Mint a fresh cast of names/topics/phrases. Same language, new subjects. */
    void newSection();

private:
    /* Indices into the explicit[] mark array, one per dial. */
    enum {
        DIAL_PHRASES = 0,
        DIAL_FUNCTION_WORDS,
        DIAL_TOPICS,
        DIAL_NAMES,
        DIAL_COMMAS,
        DIAL_QUOTATION,
        DIAL_SCARE_QUOTES,
        DIAL_COUNT
    };

    ortho_t     engine;                 /* embedded by value — no allocation */
    ortho_dials dials;                  /* host-side dial state */
    bool        explicitSet[DIAL_COUNT];
    double      preset  = 0.0;
    uint32_t    seed    = 0;
    bool        started = false;

    /* Recompute dials from preset + explicit overrides, then push to engine. */
    void recomputeDials();

    /* Set one dial by index, mark it explicit, recompute. */
    void setDial(int which, double v);
};
