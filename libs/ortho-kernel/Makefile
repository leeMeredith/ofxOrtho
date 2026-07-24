# ortho-kernel — the shared C kernel for the ortho language.
#
#   make          build the oracle
#   make test     run conformance against the reference vectors
#   make clean
#
# REF points at the ortho reference repo (for its golden vectors).

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c99 -Iinclude
SRC      = src/ortho.c src/prng.c
HDRS     = include/ortho.h include/prng.h
REF     ?= ../ortho

all: build/ortho_oracle

build/ortho_oracle: tests/oracle.c $(SRC) $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/oracle.c $(SRC)

test: build/ortho_oracle
	@./tests/conformance.sh $(REF)

clean:
	rm -rf build

.PHONY: all test clean
