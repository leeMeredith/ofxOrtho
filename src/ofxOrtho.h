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

/* The language engine is vendored at libs/ortho-kernel — a copy, not a
 * submodule, so that ZIP downloads and the Project Generator both work with no
 * extra steps. If the header is missing anyway, the addon folder is incomplete
 * or the include path is wrong; say so instead of emitting a wall of
 * unresolved types. */
#if defined(__has_include)
#  if !__has_include("ortho.h")
#    error "ofxOrtho: ortho.h not found. libs/ortho-kernel/include should sit inside this addon - check that the folder copied completely, and that addon_config.mk is being picked up. Fresh copy: https://github.com/leeMeredith/ofxOrtho"
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

    /* ---- current dial values --------------------------------------------
     * Read-back matters because setPreset() fills every dial you have not set
     * by hand: without these you cannot tell what the engine is actually
     * running, only what you last typed. */
    double getPhrases()       const { return engine.dials.phrases; }
    double getFunctionWords() const { return engine.dials.function_words; }
    double getTopics()        const { return engine.dials.topics; }
    double getNames()         const { return engine.dials.names; }
    double getCommas()        const { return engine.dials.commas; }
    double getQuotation()     const { return engine.dials.quotation; }
    double getScareQuotes()   const { return engine.dials.scare_quotes; }
    double getPreset()        const { return preset; }

    /* ---- what makes this language itself (spec 3.0) ---------------------
     * Read-only views onto the substrate. Useful for labelling output, and
     * for visual work that wants to respond to a language's character rather
     * than only to its words: a 6-consonant language and a 20-consonant one
     * want different treatment on screen. */

    /* Syllable shape every word is built from, e.g. "CVCV" or "CCVC". */
    std::string getRoot() const { return std::string(engine.root); }

    /* This language's phoneme inventory, in draw order. */
    std::string getConsonants() const { return std::string(engine.cons_set); }
    std::string getVowels()     const { return std::string(engine.vowel_set); }

    /* Mark this language uses where English would use a comma: one of
     * , ; : or an em dash. UTF-8, so not always one byte. */
    std::string getClauseMark() const { return std::string(engine.clause_mark); }

    /* Quote marks: " " or a guillemet pair. */
    std::string getQuoteOpen()  const { return std::string(engine.quote_open); }
    std::string getQuoteClose() const { return std::string(engine.quote_close); }

    /* Whether quoted speech opens with a capital in this language. */
    bool capitalizesQuoted() const { return engine.capitalize_quoted != 0; }

    /* Terminal marks in use — always ".", sometimes "?" and/or "!". */
    std::string getTerminals() const { return std::string(engine.terminals); }

    /* Whether this language joins roots into compounds with a hyphen. */
    bool hasCompounds() const { return engine.compounds != 0; }

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

    /* Generation scratch. A member rather than a local so nothing allocates
     * per call — the kernel allocates nothing, and it would be a poor trade
     * for the wrapper to undo that by constructing ~200 KB on every draw()
     * frame. Sized once, reused forever. */
    static const size_t kCapacity = 4096;
    ortho_token scratch[kCapacity];

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

    /* Warn if a request exceeds the scratch buffer. The kernel silently writes
     * what fits; saying so is the host's job, per HOSTS.md section 3. Returns
     * the clamped count. */
    int capRequest(int n, const char *what) const;
};
