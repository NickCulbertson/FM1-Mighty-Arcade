#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out=$(mktemp -d /tmp/mighty-arcade-levels.XXXXXX)
cc -O1 -ffp-contract=off -std=c99 -DPLAT_HOST -DFM1B_CPU2=1 \
    -Ibase/device -Ibase/include -Ibase/lib -Iapp -Iapp/build/gen \
    app/tests/levels.c app/dsp/engine.c -lm -o "$out/levels"
"$out/levels" "$out"
printf 'Screenshots: %s\n' "$out"
