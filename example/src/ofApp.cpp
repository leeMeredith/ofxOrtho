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
    page = ortho.tokensWithSource(220, 9);
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
    const float left      = 40.0f;
    const float right     = ofGetWidth() - 40.0f;
    const float lineStep  = 26.0f;
    const float spaceStep = 10.0f;

    float x = left;
    float y = 110.0f;

    for (size_t i = 0; i < page.size(); ++i) {
        const std::string &w = page[i].text;

        /* Rough advance: bitmap glyphs are 8px wide. Enough for a demo; use
         * ofTrueTypeFont::stringWidth() for real typesetting. */
        float advance = w.size() * 8.0f;

        if (x + advance > right) {
            x  = left;
            y += lineStep;
        }
        if (y > ofGetHeight() - 30.0f) break;

        ofSetColor(colorForSource(page[i].source));
        ofDrawBitmapString(w, x, y);

        x += advance + spaceStep;
    }

    /* header */
    ofSetColor(255);
    ofDrawBitmapString("ofxOrtho  |  seed " + ofToString(seed), left, 40);
    ofSetColor(150);
    ofDrawBitmapString("SPACE new seed   N new section   R regenerate   "
                       "UP/DOWN preset   1 bare",
                       left, 60);

    /* legend */
    ofSetColor(colorForSource(ORTHO_SRC_FRESH));
    ofDrawBitmapString("fresh", left, 80);
    ofSetColor(colorForSource(ORTHO_SRC_FUNCTION));
    ofDrawBitmapString("function", left + 70, 80);
    ofSetColor(colorForSource(ORTHO_SRC_TOPIC));
    ofDrawBitmapString("topic", left + 170, 80);
    ofSetColor(colorForSource(ORTHO_SRC_NAME));
    ofDrawBitmapString("name", left + 240, 80);
    ofSetColor(colorForSource(ORTHO_SRC_PHRASE));
    ofDrawBitmapString("phrase", left + 310, 80);
}

void ofApp::keyPressed(int key)
{
    static double preset = 0.5;

    switch (key) {
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
            ortho.setPreset(preset);
            regenerate();
            break;

        case OF_KEY_DOWN:
            preset = std::max(0.0, preset - 0.1);
            ortho.setPreset(preset);
            regenerate();
            break;

        case '1':
            preset = 0.0;
            ortho.clearDials();
            regenerate();
            break;

        default:
            break;
    }
}
