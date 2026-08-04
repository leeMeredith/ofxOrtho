#!/usr/bin/env bash
# conformance.sh — prove ofxOrtho matches the ortho reference vectors.
#
#   ./tests/conformance.sh [path-to-ortho-repo]
#
# Defaults to ../ortho. Mirrors ortho-kernel/tests/conformance.sh exactly,
# except the oracle drives the C++ wrapper instead of the raw kernel.
#
# A clean pass means: the same seed and dials produce identical text AND
# identical source classification in ofxOrtho as in the JavaScript reference.
#
# STANDALONE — compiled with plain g++/clang++. No openFrameworks required.

set -u
REF="${1:-../ortho}"
VEC="$REF/test/vectors/v5"
KERNEL="libs/ortho-kernel"

if [ ! -d "$VEC" ]; then
  echo "error: vectors not found at $VEC"
  echo "pass the path to the ortho reference repo, e.g.:"
  echo "  ./tests/conformance.sh ~/Documents/ortho"
  exit 2
fi

if [ ! -f "$KERNEL/include/ortho.h" ]; then
  echo "error: kernel submodule missing at $KERNEL"
  echo "run:  git submodule update --init --recursive"
  exit 2
fi

CXX="${CXX:-c++}"
CC="${CC:-cc}"

mkdir -p build

# Kernel is C99; wrapper and oracle are C++. Compile separately, link together.
$CC  -O2 -Wall -Wextra -std=c99 -I"$KERNEL/include" \
     -c "$KERNEL/src/ortho.c" -o build/ortho.o || exit 2
$CC  -O2 -Wall -Wextra -std=c99 -I"$KERNEL/include" \
     -c "$KERNEL/src/prng.c"  -o build/prng.o  || exit 2
$CXX -O2 -Wall -Wextra -std=c++11 -I"$KERNEL/include" -Isrc \
     -c src/ofxOrtho.cpp -o build/ofxOrtho.o || exit 2
$CXX -O2 -Wall -Wextra -std=c++11 -I"$KERNEL/include" -Isrc \
     -c tests/oracle.cpp -o build/oracle.o || exit 2
$CXX -o build/ortho_oracle \
     build/oracle.o build/ofxOrtho.o build/ortho.o build/prng.o || exit 2

$CXX -O2 -Wall -Wextra -std=c++11 -I"$KERNEL/include" -Isrc \
     -c tests/oracle_readable.cpp -o build/oracle_readable.o || exit 2
$CXX -o build/ortho_oracle_readable \
     build/oracle_readable.o build/ofxOrtho.o build/ortho.o build/prng.o || exit 2

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

# Readable path — punctuation, capitalisation, terminal marks. The vectors
# above exercise tokens only, which is deliberate but has twice let a readable-
# path bug through. Driven through the wrapper, so a C++ layer that mangled a
# UTF-8 mark would be caught here.
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
