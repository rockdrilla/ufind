#!/bin/sh
set -e

deep_cave="${1:-/usr/share}"

simple_test() {
    printf "$* %s/: " "${deep_cave}"
    "$@" "${deep_cave}" 2>/dev/null | wc -l
}

simple_test find ## kind of warmup

simple_test ./ufind-terse.sh

simple_test ./ufind
UFIND_USE_LISTS=1 simple_test ./ufind

simple_test fdfind -u -j 1 .
simple_test fdfind -u .

hyper_test() {
    hyperfine -m "${RUNS:-50}" -N "$* ${deep_cave}";
}

RUNS=10 hyper_test ./ufind-terse.sh

hyper_test ./ufind
UFIND_USE_LISTS=1 hyper_test ./ufind

hyper_test fdfind -u -j 1 .
hyper_test fdfind -u .
