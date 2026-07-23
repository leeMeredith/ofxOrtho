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

    void regenerate();
    ofColor colorForSource(uint8_t source) const;
};
