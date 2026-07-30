/*
 * ofxOrtho example — invented language on screen.
 *
 *   TAB        tokens / paragraph / compare
 *   SPACE      new random seed     (a different language)
 *   N          new section         (same language, new cast of names/topics)
 *   R          regenerate          (same language, same cast, new text)
 *
 *   1 .. 7     select a dial
 *   UP/DOWN    move the selected dial by 0.05
 *   0 / 9      selected dial to 0 / to 1
 *   LEFT/RIGHT preset down / up    (fills only dials you have not set by hand)
 *   B          bare toggle         (all seven at zero)
 *
 *   [ ]        sentences per paragraph
 *   - =        max words per sentence
 *   , .        max letters per word
 *
 * Tokens are coloured by ORIGIN, which is the thing the source classification
 * is for: names read differently from fresh coinages.
 */

#include "ofApp.h"


void ofApp::setup()
{
    /* verdana.ttf ships with openFrameworks' own examples. The final `true` is
     * the full character set: guillemets and em dashes live outside ASCII and
     * will not render without it. */
    font.load("verdana.ttf", 15, true, true);

    ofBackground(18);
    ofSetFrameRate(60);

    /* Dials may be set before setup() — they are carried in, not discarded. */
    ortho.setPreset(0.5);
    ortho.setup(seed);

    /* Punctuation up front, so the example shows all seven mechanisms without
     * anyone having to find the keys first. Quotation needs to be near 1.0 to
     * appear at all: a preset caps it at 0.4, and a span also needs a sentence
     * of 4+ words. */
    ortho.setQuotation(0.9);
    ortho.setScareQuotes(0.5);
    ortho.setCommas(0.6);
    ortho.setNames(0.5);
    dialExplicit[3] = dialExplicit[4] = dialExplicit[5] = dialExplicit[6] = true;

    regenerate();
    regeneratePanels();
}

/* One line summarising what kind of language this is: syllable shape, how many
 * consonants and vowels it uses, and the mark it puts where English puts a
 * comma. Three facts, and they are enough to tell two languages apart before
 * reading a word of either. */
std::string ofApp::describe(const ofxOrtho &o) const
{
    return o.getRoot()
         + "  " + ofToString(o.getConsonants().size()) + "c"
         + ofToString(o.getVowels().size()) + "v"
         + "  " + o.getClauseMark()
         + (o.hasCompounds() ? "  compounds" : "");
}

void ofApp::regeneratePanels()
{
    for (int i = 0; i < NPANEL; ++i) {
        /* High enough that quotation and scare quotes actually land — a
         * quoted span needs 4+ words and a roll against the dial, so a low
         * preset shows the clause mark and little else. */
        panel[i].setPreset(0.8);
        panel[i].setup(panelSeed[i]);
        panelPage[i] = panel[i].paragraphWithSource(6, 14, 9);
    }
}

/* Four languages, one frame. The point of the mode: a 6-consonant CVCV
 * language and a 20-consonant CCVC one do not look related, and no single
 * sample can show that. */
void ofApp::drawCompare()
{
    const float w = ofGetWidth() / 2.0f;
    const float h = (ofGetHeight() - 110.0f) / 2.0f;

    for (int i = 0; i < NPANEL; ++i) {
        const float ox = 40.0f + (i % 2) * w;
        const float oy = 110.0f + (i / 2) * h;

        ofSetColor(90);
        ofDrawBitmapString("seed " + ofToString(panelSeed[i]), ox, oy);
        ofSetColor(150, 200, 255);
        ofDrawBitmapString(describe(panel[i]), ox, oy + 16.0f);

        float x = ox, y = oy + 42.0f;
        for (size_t k = 0; k < panelPage[i].size(); ++k) {
            const std::string &word = panelPage[i][k].text;
            float advance = font.stringWidth(word);
            if (x + advance > ox + w - 60.0f) { x = ox; y += 22.0f; }
            if (y > oy + h - 20.0f) break;
            ofSetColor(colorForSource(panelPage[i][k].source));
            font.drawString(word, x, y);
            x += advance + 10.0f;
        }
    }
}

const char *ofApp::dialName(int i) const
{
    static const char *n[7] = { "phrases", "function_words", "topics",
                                "names", "commas", "quotation", "scare_quotes" };
    return n[(i < 0 || i > 6) ? 0 : i];
}

double ofApp::dialValue(int i) const
{
    switch (i) {
        case 0: return ortho.getPhrases();
        case 1: return ortho.getFunctionWords();
        case 2: return ortho.getTopics();
        case 3: return ortho.getNames();
        case 4: return ortho.getCommas();
        case 5: return ortho.getQuotation();
        default: return ortho.getScareQuotes();
    }
}

/* Setting a dial by hand marks it explicit: the preset will not overwrite it
 * afterwards, so call order never matters. That is the wrapper's contract, not
 * something the example arranges. */
void ofApp::setDial(int i, double v)
{
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;
    switch (i) {
        case 0: ortho.setPhrases(v);       break;
        case 1: ortho.setFunctionWords(v); break;
        case 2: ortho.setTopics(v);        break;
        case 3: ortho.setNames(v);         break;
        case 4: ortho.setCommas(v);        break;
        case 5: ortho.setQuotation(v);     break;
        default: ortho.setScareQuotes(v);  break;
    }
    bare = false;
}

/* All seven, always visible. A dial you cannot read is a dial you cannot
 * reason about — and the recurrence family behaves nothing like the
 * punctuation family, which is only obvious when both are on screen. */
void ofApp::drawDials()
{
    /* A horizontal strip across the top, above the text rather than beside it.
     * A column on the right competes with the prose for the same space and
     * ends up overlapping it at any window width the text actually fills. */
    const float y   = 108.0f;
    const float x0  = 40.0f;
    const float col = 132.0f;

    for (int i = 0; i < 7; ++i) {
        /* The gap after `names` separates the recurrence family from the
         * punctuation family. They behave differently — the first four affect
         * every path, the last three only the readable one — and nothing in
         * the names says so. */
        const float x = x0 + i * col + (i >= 4 ? 24.0f : 0.0f);
        const bool  sel = (i == selDial);
        const double v = dialValue(i);

        ofSetColor(sel ? ofColor(255, 220, 120) : ofColor(120));
        ofDrawBitmapString(ofToString(i + 1) + " " + dialName(i), x, y);

        ofSetColor(40);
        ofDrawRectangle(x, y + 8.0f, 108.0f, 7.0f);
        ofSetColor(sel ? ofColor(255, 220, 120) : ofColor(90));
        ofDrawRectangle(x, y + 8.0f, (float)(v * 108.0), 7.0f);

        ofSetColor(sel ? ofColor(255, 220, 120) : ofColor(110));
        ofDrawBitmapString(ofToString(v, 2), x, y + 30.0f);
    }

    /* The marks as bytes. ofDrawBitmapString is ASCII-only, so a language whose
     * clause mark is an em dash or whose quotes are guillemets draws NOTHING
     * where the mark should be — the punctuation is in the string and simply
     * cannot be shown by this call. Printing the byte count makes that visible
     * instead of looking like the engine failed to punctuate. */
    {
        const std::string cm = ortho.getClauseMark();
        const std::string qo = ortho.getQuoteOpen();
        std::string hex;
        for (size_t b = 0; b < cm.size(); ++b)
            hex += ofToHex((unsigned char)cm[b]) + " ";
        ofSetColor(200, 120, 120);
        ofDrawBitmapString("clause [" + cm + "] " + ofToString((int)cm.size()) +
                           "B  " + hex + "   quote [" + qo + "] " +
                           ofToString((int)qo.size()) + "B" +
                           (cm.size() > 1 || qo.size() > 1
                              ? "   <- multibyte, ofDrawBitmapString cannot draw these"
                              : ""),
                           40.0f, ofGetHeight() - 12.0f);
    }

    ofSetColor(120);
    ofDrawBitmapString("preset " + ofToString(ortho.getPreset(), 2) +
                       (bare ? " (bare)" : "") +
                       "    sentences " + ofToString(numSentences) +
                       "    words " + ofToString(maxWords) +
                       "    letters " + ofToString(maxLetters),
                       x0, y + 52.0f);
}

void ofApp::regenerate()
{
    /* Two separate calls into the kernel. Nothing is combined: the neutral
     * path stays neutral, the readable path stays readable. */
    page.clear();

    if (mode == MODE_TOKENS) {
        page = ortho.tokensWithSource(220, maxLetters);
    } else {
        page = ortho.paragraphWithSource(numSentences, maxWords, maxLetters);
    }
}

ofColor ofApp::colorForSource(uint8_t source) const
{
    switch (source) {
        case ORTHO_SRC_FUNCTION: return ofColor(110, 120, 140); // grammar glue
        case ORTHO_SRC_TOPIC:    return ofColor(120, 200, 190); // the section's subject
        case ORTHO_SRC_NAME:     return ofColor(245, 190,  90); // the section's identities
        case ORTHO_SRC_PHRASE:   return ofColor(215, 120, 180); // recurring phrase
        case ORTHO_SRC_FRESH:
        default:                 return ofColor(225, 225, 225); // fresh coinage
    }
}

void ofApp::draw()
{
    if      (mode == MODE_TOKENS)  drawTokens();
    else if (mode == MODE_COMPARE) drawCompare();
    else                           drawProse();

    if (mode != MODE_COMPARE) drawDials();

    /* header */
    ofSetColor(255);
    if (mode == MODE_COMPARE) {
        ofDrawBitmapString("ofxOrtho  |  four seeds, four languages  |  "
                           "shape, inventory, clause mark", 40, 40);
    } else {
        ofDrawBitmapString("ofxOrtho  |  seed " + ofToString(seed) +
                           "  |  " + describe(ortho) + "  |  " +
                           (mode == MODE_TOKENS ? "tokens (neutral, unpunctuated)"
                                                : "paragraph (readable)"),
                           40, 40);
    }
    ofSetColor(150);
    ofDrawBitmapString("TAB surface   SPACE seed   N section   R regen   1-7 dial   "
                       "UP/DOWN dial   LEFT/RIGHT preset   B bare   [ ] - = , .",
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
    /* clears the dial strip drawn by drawDials() */
    float x = left, y = 190.0f;

    for (size_t i = 0; i < page.size(); ++i) {
        const std::string &w = page[i].text;
        /* stringWidth measures what will actually be drawn, so a multibyte mark
         * costs exactly the space it occupies. Counting characters could not
         * know that. */
        float advance = font.stringWidth(w);

        if (x + advance > right) { x = left; y += 30.0f; }
        if (y > ofGetHeight() - 40.0f) break;

        ofSetColor(colorForSource(page[i].source));
        font.drawString(w, x, y);
        x += advance + 10.0f;
    }
}

/* Readable path: punctuation is baked into the token characters by the
 * kernel post-pass, so nothing special is needed here - just draw them. */
void ofApp::drawProse()
{
    const float left  = 40.0f;
    const float right = ofGetWidth() - 40.0f;
    /* clears the dial strip drawn by drawDials() */
    float x = left, y = 190.0f;

    for (size_t i = 0; i < page.size(); ++i) {
        const std::string &w = page[i].text;
        /* stringWidth measures what will actually be drawn, so a multibyte mark
         * costs exactly the space it occupies. Counting characters could not
         * know that. */
        float advance = font.stringWidth(w);

        if (x + advance > right) { x = left; y += 30.0f; }
        if (y > ofGetHeight() - 40.0f) break;

        ofSetColor(colorForSource(page[i].source));
        font.drawString(w, x, y);
        x += advance + 10.0f;
    }
}

void ofApp::keyPressed(int key)
{
    switch (key) {
        case OF_KEY_TAB:
            /* Switch which kernel surface is drawn. */
            mode = (Mode)((mode + 1) % 3);
            regenerate();
            break;

        case ' ':
            /* A new seed is a new language. Keep the number and you can
             * always return to this exact language later. */
            seed = (uint32_t)ofRandom(0, 4294967295.0);
            ortho.setup(seed);
            regenerate();
            for (int i = 0; i < NPANEL; ++i)
                panelSeed[i] = (uint32_t)ofRandom(0, 4294967295.0);
            regeneratePanels();
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
            setDial(selDial, dialValue(selDial) + 0.05);
            regenerate();
            break;

        case OF_KEY_DOWN:
            setDial(selDial, dialValue(selDial) - 0.05);
            regenerate();
            break;

        case '0': setDial(selDial, 0.0); regenerate(); break;
        case '9': setDial(selDial, 1.0); regenerate(); break;

        case OF_KEY_RIGHT:
            preset = std::min(1.0, preset + 0.05);
            bare = false;
            ortho.setPreset(preset);
            regenerate();
            break;

        case OF_KEY_LEFT:
            preset = std::max(0.0, preset - 0.05);
            bare = false;
            ortho.setPreset(preset);
            regenerate();
            break;

        case 'b':
        case 'B':
            /* Toggle, not a one-way door: restores the preset on second press.
             * clearDials() drops every explicit mark AND the preset, so the
             * second press has to re-apply it. */
            bare = !bare;
            if (bare) ortho.clearDials();
            else      ortho.setPreset(preset);
            regenerate();
            break;

        case '1': case '2': case '3': case '4':
        case '5': case '6': case '7':
            selDial = key - '1';
            break;

        case '-': if (maxWords > 4)   maxWords--;   regenerate(); break;
        case '=': if (maxWords < 60)  maxWords++;   regenerate(); break;
        case ',': if (maxLetters > 2) maxLetters--; regenerate(); break;
        case '.': if (maxLetters < 24) maxLetters++; regenerate(); break;

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
