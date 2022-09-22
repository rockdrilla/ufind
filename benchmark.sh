#!/bin/sh
set -e

export MALLOC_ARENA_MAX=8

deep_cave="${1:-/usr/share}"
test_cave="${2:-/usr/bin}"

simple_test() {
    printf "$* %s/: " "${test_cave}"
    "$@" "${test_cave}" 2>/dev/null | wc -l
}

echo "# compare numbers from different utilities:"
echo

## kind of warmup
simple_test find
simple_test ./ufind-terse.sh
simple_test ./ufind -q
simple_test fdfind -u .

hyper_test() {
    hyperfine -m "${RUNS:-40}" -M "${RUNS:-40}" -N "$* ${deep_cave}"
}

echo
echo "# compare performance:"
echo

hyper_test find

RUNS=1 hyper_test ./ufind-terse.sh

hyper_test ./ufind -q

echo "# run ufind with env MALLOC_ARENA_MAX=2:"
MALLOC_ARENA_MAX=2 hyper_test ./ufind -q

echo "# run ufind with env MALLOC_ARENA_MAX=1:"
MALLOC_ARENA_MAX=1 hyper_test ./ufind -q

hyper_test fdfind -u -j 1 .
