#!/bin/sh
# The app in fm1-base's host simulator (every app/tests/*.txt) and the levels' checks (app/tests/levels.sh).
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
B=app/build
OUT=$B/tests
H=$B/host/mightyarcade_host              # NAME_host: fm1.toml's name in lower case
FAIL=0
mkdir -p "$OUT"
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run host-build sh build.sh --host
for t in app/tests/*.txt; do
    n=$(basename "$t" .txt)
    run "$n" "$H" "$t" "$OUT"
done
run levels sh app/tests/levels.sh
exit $FAIL
