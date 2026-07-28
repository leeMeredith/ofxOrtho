/* oracle_readable.c — readable-path oracle.
 *
 * The main oracle dumps tokensWithSource(), which is deliberate: no
 * punctuation or capitalisation decisions to keep in sync. But that leaves the
 * readable path unverified, and it has now hidden two bugs — a doubly-reduced
 * word length in 1.x, and English punctuation on every language in the first
 * cut of 3.0. Both passed 7/7.
 *
 * This dumps ortho_paragraph() output, one word per line, so the JS reference
 * and every port can be diffed on the path a listener actually hears.
 *
 * Usage:
 *   oracle_readable <seed> <numSentences> <maxWords> <maxLetters> [preset]
 */

#include <stdio.h>
#include <stdlib.h>
#include "ortho.h"

int main(int argc, char **argv)
{
    ortho_t o;
    ortho_dials d;
    ortho_token buf[2048];
    uint32_t seed  = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 0;
    int nsent      = (argc > 2) ? atoi(argv[2]) : 2;
    int maxwords   = (argc > 3) ? atoi(argv[3]) : 10;
    int maxletters = (argc > 4) ? atoi(argv[4]) : 8;
    double preset  = (argc > 5) ? atof(argv[5]) : 0.0;
    int n, i;

    ortho_dials_clear(&d);
    if (preset > 0.0) ortho_dials_preset(&d, preset);
    ortho_init(&o, seed, &d);

    n = ortho_paragraph(&o, nsent, maxwords, maxletters, buf, 2048);
    for (i = 0; i < n; i++) printf("%d\t%s\n", i, buf[i].text);
    return 0;
}
