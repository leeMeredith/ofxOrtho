#pragma once

#include "ofMain.h"
#include "ofxOrtho.h"

class ofApp : public ofBaseApp {
public:
    void setup();
    void draw();
    void keyPressed(int key);

private:
    ofxOrtho ortho;
    uint32_t seed = 12345;

    /* Cached page of tokens. Regenerated only on demand, never per-frame:
     * generation advances the PRNG, so calling it in draw() would make the
     * text churn every frame. */
    std::vector<ofxOrtho::Token> page;

    /* Which surface is drawn.
     *   TOKENS    - neutral path, count-exact, NEVER punctuated,
     *               and origin-bearing so tokens can be coloured by source
     *   PARAGRAPH - readable path, carries commas / quotes / terminal marks,
     *               but plain strings: there is no paragraphWithSource()
     *
     * Two containers rather than one, because synthesising a source value for
     * the paragraph case would mean the example inventing origin data it does
     * not have. */
    enum Mode { MODE_TOKENS = 0, MODE_PARAGRAPH = 1 };
    Mode mode = MODE_TOKENS;

    /* Paragraph size is a per-call argument, not a dial: how much text you
     * want is a property of the request, not of the language. */
    int  numSentences = 14;

    /* Bare is a toggle, not a one-way door - restores the previous preset. */
    bool   bare   = false;
    double preset = 0.5;

    void regenerate();
    void drawTokens();
    void drawProse();
    ofColor colorForSource(uint8_t source) const;
};
