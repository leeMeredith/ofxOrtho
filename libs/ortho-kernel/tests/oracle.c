/*
 * oracle.c — the C-side conformance oracle.
 *
 * Mirrors test/oracle.js from the `ortho` reference repo exactly. Prints
 *     <index>\t<word>\t<source>
 * so the two can be diffed directly.
 *
 *   ortho_oracle <seed> <n> [maxLetters] [preset]
 */

#include <stdio.h>
#include <stdlib.h>
#include "ortho.h"

#define MAX_TOKENS 100000

static ortho_token buf[MAX_TOKENS];

int main(int argc, char **argv)
{
    uint32_t seed = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 0;
    int n         = (argc > 2) ? atoi(argv[2]) : 100;
    int maxLetters= (argc > 3) ? atoi(argv[3]) : 8;
    double preset = (argc > 4) ? atof(argv[4]) : 0.0;

    ortho_t o;
    ortho_dials d;
    int count, i;

    ortho_dials_clear(&d);
    if (preset > 0.0) ortho_dials_preset(&d, preset);

    ortho_init(&o, seed, &d);

    if (n > MAX_TOKENS) n = MAX_TOKENS;
    count = ortho_tokens(&o, n, maxLetters, buf, MAX_TOKENS);

    for (i = 0; i < count; i++) {
        printf("%d\t%s\t%u\n", i, buf[i].text, (unsigned)buf[i].source);
    }
    return 0;
}
