#!/usr/bin/env bash
# conformance.sh — prove this C kernel matches the ortho reference vectors.
#
#   ./tests/conformance.sh [path-to-ortho-repo]
#
# Defaults to ../ortho. A clean pass is the definition of "coherent with the
# reference" — same seed and dials produce identical text AND identical source
# classification in C and JavaScript.

set -u
REF="${1:-../ortho}"
VEC="$REF/test/vectors/v5"

if [ ! -d "$VEC" ]; then
  echo "error: vectors not found at $VEC"
  echo "pass the path to the ortho reference repo, e.g.:"
  echo "  ./tests/conformance.sh ~/Documents/ortho"
  exit 2
fi

mkdir -p build
cc -O2 -Wall -Wextra -Iinclude -o build/ortho_oracle \
   tests/oracle.c src/ortho.c src/prng.c || exit 2

pass=0; fail=0
for f in "$VEC"/*.txt; do
  base=$(basename "$f" .txt)
  seed=$(echo "$base" | sed 's/seed_\([0-9]*\)_.*/\1/')
  if echo "$base" | grep -q preset50; then n=80; preset=0.5; else n=50; preset=0; fi
  ./build/ortho_oracle "$seed" "$n" 8 "$preset" > build/out.txt
  if diff -q "$f" build/out.txt >/dev/null; then
    echo "PASS  $base"; pass=$((pass+1))
  else
    echo "FAIL  $base"; fail=$((fail+1))
    diff "$f" build/out.txt | head -6
  fi
done

# ---------------------------------------------------------------------------
# Readable path. The vectors above exercise ortho_tokens only — deliberately,
# since it carries no punctuation or capitalisation to keep in sync. But that
# gap has hidden two real bugs that passed 7/7: a doubly-reduced word length in
# spec 1.x, and English punctuation on every language in the first cut of 3.0.
# These vectors close it.
# ---------------------------------------------------------------------------
RVEC="$REF/test/vectors/v5-readable"
if [ -d "$RVEC" ]; then
  for f in "$RVEC"/*.txt; do
    base=$(basename "$f" .txt)
    seed=$(echo "$base" | sed 's/seed_\([0-9]*\)_.*/\1/')
    ./build/ortho_oracle_readable "$seed" 3 12 8 0.5 > build/rout.txt
    if diff -q "$f" build/rout.txt >/dev/null; then
      echo "PASS  readable/$base"; pass=$((pass+1))
    else
      echo "FAIL  readable/$base"; fail=$((fail+1))
      diff "$f" build/rout.txt | head -6
    fi
  done
fi

echo
if [ "$fail" -eq 0 ]; then
  echo "CONFORMANT — $pass/$pass vectors, spec 4.0, vectors v5"
  exit 0
else
  echo "$fail FAILURE(S) — $pass passed"
  exit 1
fi
