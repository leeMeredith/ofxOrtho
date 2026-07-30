#pragma once

#include "ofMain.h"
#include "ofxOrtho.h"

class ofApp : public ofBaseApp {
public:
    void setup();
    void draw();
    void keyPressed(int key);

private:
    /* The generated text is drawn with a real font, not ofDrawBitmapString.
     * That call is an ASCII bitmap: it draws NOTHING for a multibyte glyph, so
     * a language whose quotes are guillemets or whose clause mark is an em dash
     * loses its punctuation on screen while still reserving width for it. The
     * fourth argument to load() asks for the full character set — without it,
     * ofTrueTypeFont is ASCII-only too and the problem persists.
     *
     * The HUD stays on ofDrawBitmapString deliberately: it is ASCII in a fixed
     * grid, which is what a bitmap font is good at. */
    ofTrueTypeFont font;

    ofxOrtho ortho;
    uint32_t seed = 12345;

    /* Cached page of tokens. Regenerated only on demand, never per-frame:
     * generation advances the PRNG, so calling it in draw() would make the
     * text churn every frame. */
    std::vector<ofxOrtho::Token> page;

    /* Which surface is drawn.
     *   TOKENS    - neutral path, count-exact, NEVER punctuated,
     *               and origin-bearing so tokens can be coloured by source
     *   PARAGRAPH - readable path, carries this language's own clause mark,
     *               quote pair and terminal marks
     *   COMPARE   - four languages at once. Spec 3.0 gives every seed its own
     *               phoneme inventory and syllable shape, and that is not
     *               visible in a single sample: one language looks like a
     *               language, four look like four. */
    enum Mode { MODE_TOKENS = 0, MODE_PARAGRAPH = 1, MODE_COMPARE = 2 };
    Mode mode = MODE_TOKENS;

    /* Four independent instances. Adjacent seeds share nothing — the PRNG sees
     * to that — so 1000..1003 is as varied a sample as any other. */
    static const int NPANEL = 4;
    ofxOrtho panel[NPANEL];
    uint32_t panelSeed[NPANEL] = { 1000, 1001, 1002, 1003 };
    std::vector<ofxOrtho::Token> panelPage[NPANEL];

    /* Paragraph size is a per-call argument, not a dial: how much text you
     * want is a property of the request, not of the language. */
    int  numSentences = 14;

    /* Bare is a toggle, not a one-way door - restores the previous preset. */
    bool   bare   = false;
    double preset = 0.5;

    /* Shaping: how much text, not what kind. Per-call arguments in the API,
     * held here only because an interactive example needs somewhere to put
     * them between keypresses. */
    int maxWords   = 22;
    int maxLetters = 9;

    /* Which of the seven dials UP/DOWN currently moves. Selecting a dial and
     * nudging it is the only way to hear one mechanism at a time: a preset
     * moves all seven at once, which tells you how the language feels but not
     * which dial did what. */
    int selDial = 0;

    /* Which dials the user has set by hand. The wrapper tracks this internally
     * and preset will not overwrite a marked dial, but it exposes no way to
     * ask — so the example mirrors it for display. Without the marks visible,
     * LEFT/RIGHT looks like it moves all seven at random. */
    bool dialExplicit[7] = { false, false, false, false, false, false, false };

    void regenerate();
    void regeneratePanels();
    void drawTokens();
    void drawProse();
    void drawCompare();
    void drawDials();
    std::string describe(const ofxOrtho &o) const;
    const char *dialName(int i) const;
    double dialValue(int i) const;
    void  setDial(int i, double v);
    ofColor colorForSource(uint8_t source) const;
};
