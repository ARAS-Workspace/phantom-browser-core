#!/usr/bin/env bash
# Runs the test suites of one Linux CI run. Each suite goes through core-test.sh
# into RUNS/RUN_NAME/<suite> and is judged against the last green run of that
# suite, which the link taban-<suite> in the CI directory points at; a green
# suite moves its link to this run. The newest KEEP runs stay and older ones are
# removed, except a run a link points into.
set -euo pipefail

CI_DIR="${PHANTOM_CI_DIR:-/srv/phantom/ci}"
KEEP="${PHANTOM_CI_KEEP:-10}"
SUITES="unit browser"
# More new red tests than this is a broken build, not load noise; core-test.sh
# then fails the suite without running them again.
CONFIRM_MAX=50

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS="$ROOT/phantom_tools"
RUNS="$CI_DIR/runs"

usage() {
    echo "usage: core-ci-test.sh RUN_NAME"
    echo "RUN_NAME  letters, digits, '.', '_' and '-'; the run goes to $RUNS/RUN_NAME"
    echo "suites: $SUITES, filters $TOOLS/ci-filter-<suite>.json, bases $CI_DIR/taban-<suite>"
}

die() { echo "core-ci-test: $*" >&2; exit 2; }
say() { echo "core-ci-test: $*"; }

# core-test.sh options of each suite. browser_tests runs one test per process
# whatever -b says (content/public/test/test_launcher.cc GetBatchSize), and its
# failed tests run once more with 8 jobs, since -j 32 leaves load failures behind.
suite_options() {
    case "$1" in
        unit) echo "-b 10" ;;
        browser) echo "-R 8" ;;
        *) die "no options for suite $1" ;;
    esac
}

[ $# -eq 1 ] || { usage; exit 2; }
case "$1" in -h|--help) usage; exit 0 ;; esac
name="$1"
case "$name" in ''|*[!A-Za-z0-9._-]*|.|..) die "bad run name: $name" ;; esac
case "$KEEP" in ''|*[!0-9]*|0) die "PHANTOM_CI_KEEP must be a number above 0, got: $KEEP" ;; esac
run="$RUNS/$name"
[ ! -e "$run" ] || die "$run exists; each run writes into a new directory"
for suite in $SUITES; do
    [ -f "$TOOLS/ci-filter-$suite.json" ] || die "no filter file at $TOOLS/ci-filter-$suite.json"
done
mkdir -p "$run"

status=0
for suite in $SUITES; do
    link="$CI_DIR/taban-$suite"
    base_args=""
    if [ -d "$link" ]; then
        base="$(cd "$link" && pwd -P)"
        base_args="--base $base -C $CONFIRM_MAX"
        say "$suite against $base"
    elif [ -L "$link" ]; then
        say "$suite: $link points at $(readlink "$link"), which is gone; every red test fails the suite"
    else
        say "$suite: no base at $link; every red test fails the suite"
    fi
    rc=0
    # shellcheck disable=SC2046,SC2086  # the options are words on purpose
    "$TOOLS/core-test.sh" $(suite_options "$suite") $base_args \
        "$TOOLS/ci-filter-$suite.json" "$run/$suite" || rc=$?
    case "$rc" in
        0)
            ln -sfn "$run/$suite" "$link"
            say "$suite GREEN, its base moves to $run/$suite"
            verdict=GREEN
            ;;
        1)
            say "$suite RED, its base stays where it was"
            verdict=RED
            status=1
            ;;
        *)
            say "$suite: core-test.sh stopped with $rc"
            verdict="ERROR $rc"
            status=1
            ;;
    esac
    if [ -n "${GITHUB_STEP_SUMMARY-}" ]; then
        {
            echo "### $suite: $verdict"
            echo '```'
            if [ -f "$run/$suite/ozet.txt" ]; then
                grep -E '^(=== |--- |NEW RED |CONFIRM |core-test: )' "$run/$suite/ozet.txt" || true
            else
                echo "no ozet.txt"
            fi
            echo '```'
        } >> "$GITHUB_STEP_SUMMARY"
    fi
done

# A base link points at <run>/<suite>, so the run a link holds is its parent.
held=" "
for suite in $SUITES; do
    link="$CI_DIR/taban-$suite"
    [ -d "$link" ] || continue
    held="$held$(dirname "$(cd "$link" && pwd -P)") "
done
# Newest first by the time the run directory was made; a run directory is not
# written after its run.
ls -1t "$RUNS" | tail -n +"$(( KEEP + 1 ))" | while read -r old; do
    dir="$(cd "$RUNS/$old" && pwd -P)"
    case "$held" in *" $dir "*) say "keeping $old, a base points into it"; continue ;; esac
    rm -rf -- "$dir"
    say "removed $old, older than the last $KEEP runs"
done

exit "$status"
