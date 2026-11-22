#!/usr/bin/env bash
# Builds the Linux CI tree on the x86-64 CI host. core-build-mac.sh does the
# same for the mac development tree; this one serves the CI machine, where
# core-sync-linux.sh has generated out/ci.
set -euo pipefail

DEFAULT_JOBS="$(getconf _NPROCESSORS_ONLN)"
DEFAULT_TARGET=chrome
OUT_NAME="ci"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS="$ROOT/phantom_tools"
STAGING="$TOOLS/.deps/staging"

usage() {
    echo "usage: core-build-linux.sh [-j N] [target...]"
    echo "-j defaults to the number of processors ($DEFAULT_JOBS), target defaults to $DEFAULT_TARGET"
    echo "test targets: unit_tests components_unittests browser_tests"
}

die() { echo "core-build-linux: $*" >&2; exit 1; }
say() { echo "core-build-linux: $*"; }

path_without_virtualenv() {
    local out="" entry
    local IFS=:
    for entry in $PATH; do
        if [ -n "${VIRTUAL_ENV-}" ] && [ "$entry" = "$VIRTUAL_ENV/bin" ]; then continue; fi
        out="${out:+$out:}$entry"
    done
    printf '%s' "$out"
}

jobs="$DEFAULT_JOBS"
targets=""
while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        -j) shift; [ $# -gt 0 ] || die "-j needs a number"; jobs="$1" ;;
        -j*) jobs="${1#-j}" ;;
        -*) die "unknown option: $1" ;;
        *) targets="${targets:+$targets }$1" ;;
    esac
    shift
done
targets="${targets:-$DEFAULT_TARGET}"
case "$jobs" in ''|*[!0-9]*) die "-j must be a number, got: $jobs" ;; esac

out="$ROOT/out/$OUT_NAME"
ninja="$ROOT/third_party/ninja/ninja"

[ -f "$out/build.ninja" ] || die "no build files in $out, run 'core-sync-linux.sh configure' first"
[ -x "$ninja" ] || die "no ninja at $ninja, the deps stage has not finished"

"$TOOLS/core-sync-linux.sh" verify

started=$SECONDS
say "building $targets in out/$OUT_NAME with -j $jobs"
say "started at $(date '+%Y-%m-%d %H:%M:%S')"

# shellcheck disable=SC2086  # targets holds one or more names
( cd "$ROOT" && env -u VPYTHON_BYPASS -u VIRTUAL_ENV -u PYTHONPATH -u PYTHONHOME \
    PATH="$STAGING/depot_tools:$(path_without_virtualenv)" \
    "$ninja" -C "out/$OUT_NAME" -j "$jobs" $targets )

elapsed=$(( SECONDS - started ))
case " $targets " in
    *" chrome "*)
        binary="$out/chrome"
        [ -x "$binary" ] || die "ninja finished but there is no executable at $binary"
        version="$("$binary" --version 2>&1 | head -1)"
        [ -n "$version" ] || die "the built binary did not answer --version"
        say "built $version"
        ;;
esac
say "done in ${elapsed}s ($(( elapsed / 60 ))m) with -j $jobs, out/$OUT_NAME is $(du -sh "$out" 2>/dev/null | cut -f1)"
