/*
 * ofxOrtho example — invented language on screen.
 *
 *   SPACE   new random seed        (a different language)
 *   N       new section            (same language, new cast of names/topics)
 *   R       regenerate             (same language, same cast, new text)
 *   UP/DOWN preset up / down       (more or less recurrence + punctuation)
 *   1       reset to bare dials    (all seven at zero)
 *
 * Tokens are coloured by ORIGIN, which is the thing the source classification
 * is for: names read differently from fresh coinages.
 */

#include "ofApp.h"

void ofApp::setup()
{
    ofBackground(18);
    ofSetFrameRate(60);

    /* Dials may be set before setup() — they are carried in, not discarded. */
    ortho.setPreset(0.5);
    ortho.setup(seed);

    regenerate();
}

void ofApp::regenerate()
{
    /* Two separate calls into the kernel. Nothing is combined: the neutral
     * path stays neutral, the readable path stays readable. */
    page.clear();

    if (mode == MODE_TOKENS) {
        page = ortho.tokensWithSource(220, 9);
    } else {
        page = ortho.paragraphWithSource(numSentences, 22, 9);
    }
}

ofColor ofApp::colorForSource(uint8_t source) const
{
    switch (source) {
        case ORTHO_SRC_FUNCTION: return ofColor(110, 120, 140); // grammar glue
        case ORTHO_SRC_TOPIC:    return ofColor(120, 200, 190); // the phony WHAT
        case ORTHO_SRC_NAME:     return ofColor(245, 190,  90); // the phony WHO
        case ORTHO_SRC_PHRASE:   return ofColor(215, 120, 180); // recurring phrase
        case ORTHO_SRC_FRESH:
        default:                 return ofColor(225, 225, 225); // fresh coinage
    }
}

void ofApp::draw()
{
    if (mode == MODE_TOKENS) drawTokens();
    else                     drawProse();

    /* header */
    ofSetColor(255);
    ofDrawBitmapString("ofxOrtho  |  seed " + ofToString(seed) + "  |  " +
                       (mode == MODE_TOKENS ? "tokens (neutral, unpunctuated)"
                                            : "paragraph (readable)"),
                       40, 40);
    ofSetColor(150);
    ofDrawBitmapString("TAB switch surface   SPACE new seed   N new section   "
                       "R regenerate   UP/DOWN preset   1 bare",
                       40, 60);

    {
        ofSetColor(colorForSource(ORTHO_SRC_FRESH));
        ofDrawBitmapString("fresh", 40, 80);
        ofSetColor(colorForSource(ORTHO_SRC_FUNCTION));
        ofDrawBitmapString("function", 110, 80);
        ofSetColor(colorForSource(ORTHO_SRC_TOPIC));
        ofDrawBitmapString("topic", 210, 80);
        ofSetColor(colorForSource(ORTHO_SRC_NAME));
        ofDrawBitmapString("name", 280, 80);
        ofSetColor(colorForSource(ORTHO_SRC_PHRASE));
        ofDrawBitmapString("phrase", 350, 80);
    }
}

/* Neutral path: origin-bearing, so colour by source. Never punctuated. */
void ofApp::drawTokens()
{
    const float left  = 40.0f;
    const float right = ofGetWidth() - 40.0f;
    float x = left, y = 110.0f;

    for (size_t i = 0; i < page.size(); ++i) {
        const std::string &w = page[i].text;
        float advance = w.size() * 8.0f;

        if (x + advance > right) { x = left; y += 26.0f; }
        if (y > ofGetHeight() - 30.0f) break;

        ofSetColor(colorForSource(page[i].source));
        ofDrawBitmapString(w, x, y);
        x += advance + 10.0f;
    }
}

/* Readable path: punctuation is baked into the token characters by the
 * kernel post-pass, so nothing special is needed here - just draw them. */
void ofApp::drawProse()
{
    const float left  = 40.0f;
    const float right = ofGetWidth() - 40.0f;
    float x = left, y = 110.0f;

    for (size_t i = 0; i < page.size(); ++i) {
        const std::string &w = page[i].text;
        float advance = w.size() * 8.0f;

        if (x + advance > right) { x = left; y += 26.0f; }
        if (y > ofGetHeight() - 30.0f) break;

        ofSetColor(colorForSource(page[i].source));
        ofDrawBitmapString(w, x, y);
        x += advance + 10.0f;
    }
}

void ofApp::keyPressed(int key)
{
    switch (key) {
        case OF_KEY_TAB:
            /* Switch which kernel surface is drawn. */
            mode = (mode == MODE_TOKENS) ? MODE_PARAGRAPH : MODE_TOKENS;
            regenerate();
            break;

        case ' ':
            /* A new seed is a new language. Keep the number and you can
             * always return to this exact language later. */
            seed = (uint32_t)ofRandom(0, 4294967295.0);
            ortho.setup(seed);
            regenerate();
            break;

        case 'n':
        case 'N':
            /* Same language, fresh cast of names / topics / phrases. */
            ortho.newSection();
            regenerate();
            break;

        case 'r':
        case 'R':
            regenerate();
            break;

        case OF_KEY_UP:
            preset = std::min(1.0, preset + 0.1);
            bare = false;
            ortho.setPreset(preset);
            regenerate();
            break;

        case OF_KEY_DOWN:
            preset = std::max(0.0, preset - 0.1);
            bare = false;
            ortho.setPreset(preset);
            regenerate();
            break;

        case '1':
            /* Toggle, not a one-way door: restores the preset on second press. */
            bare = !bare;
            if (bare) ortho.clearDials();
            else      ortho.setPreset(preset);
            regenerate();
            break;

        case '[':
            if (numSentences > 2) numSentences -= 2;
            regenerate();
            break;

        case ']':
            if (numSentences < 60) numSentences += 2;
            regenerate();
            break;

        default:
            break;
    }
}
