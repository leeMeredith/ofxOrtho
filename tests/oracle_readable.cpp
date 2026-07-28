/* oracle_readable.cpp — readable-path oracle for ofxOrtho.
 *
 * The main oracle drives tokensWithSource(): count-exact, structure-free, no
 * punctuation to keep in sync. That gap has hidden two real bugs that passed
 * 7/7 — a doubly-reduced word length in spec 1.x, and English punctuation on
 * every language in the first cut of 3.0.
 *
 * This drives paragraph() through the WRAPPER, not the kernel directly, so a
 * C++ layer that mangled a UTF-8 mark or dropped a capital would be caught.
 *
 * Usage:
 *   oracle_readable <seed> <numSentences> <maxWords> <maxLetters> [preset]
 */

#include <cstdio>
#include <cstdlib>
#include "ofxOrtho.h"

int main(int argc, char **argv)
{
    uint32_t seed  = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 0;
    int nsent      = (argc > 2) ? atoi(argv[2]) : 2;
    int maxwords   = (argc > 3) ? atoi(argv[3]) : 10;
    int maxletters = (argc > 4) ? atoi(argv[4]) : 8;
    double preset  = (argc > 5) ? atof(argv[5]) : 0.0;

    ofxOrtho o;
    if (preset > 0.0) o.setPreset(preset);
    o.setup(seed);

    std::vector<std::string> words = o.paragraph(nsent, maxwords, maxletters);
    for (size_t i = 0; i < words.size(); ++i)
        printf("%d\t%s\n", (int)i, words[i].c_str());
    return 0;
}
