/*
 * oracle.cpp — the ofxOrtho conformance oracle.
 *
 * Mirrors ortho-kernel/tests/oracle.c and test/oracle.js from the reference
 * repo, printing the same three columns:
 *
 *     <index>\t<word>\t<source>
 *
 * The difference: this drives the C++ WRAPPER, not the kernel directly. It
 * proves ofxOrtho -> ortho-kernel -> vectors.
 *
 * STANDALONE BY DESIGN. No openFrameworks headers, no GLFW, no oF toolchain.
 * Building GLFW proves nothing about language conformance.
 *
 *   ortho_oracle <seed> <n> [maxLetters] [preset]
 */

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "ofxOrtho.h"

int main(int argc, char **argv)
{
    uint32_t seed  = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 0;
    int n          = (argc > 2) ? atoi(argv[2]) : 100;
    int maxLetters = (argc > 3) ? atoi(argv[3]) : 8;
    double preset  = (argc > 4) ? atof(argv[4]) : 0.0;

    ofxOrtho ortho;

    /* Dials before setup() — exercises the buffering path on every run. */
    if (preset > 0.0) ortho.setPreset(preset);

    ortho.setup(seed);

    std::vector<ofxOrtho::Token> toks = ortho.tokensWithSource(n, maxLetters);

    for (size_t i = 0; i < toks.size(); ++i) {
        printf("%d\t%s\t%u\n",
               (int)i,
               toks[i].text.c_str(),
               (unsigned)toks[i].source);
    }
    return 0;
}
