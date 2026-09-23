#!/usr/bin/env bash
# Checks the tree against a linux configuration. This tree is built for mac;
# linux is verified, not built. gn gen resolves every label and gn check reads
# every include, which is where a deletion only linux would notice shows up.
set -euo pipefail

STAGES=(gen check)
OUT_NAME="dev-linux"
TARGET_CPU="arm64"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS="$ROOT/phantom_tools"
FLAGS="$TOOLS/flags-dev-linux.gn"
SYSROOT="$ROOT/build/linux/debian_bullseye_$TARGET_CPU-sysroot"

die() { echo "core-gate-linux: $*" >&2; exit 1; }
say() { echo "core-gate-linux: $*"; }

usage() {
    echo "usage: core-gate-linux.sh [stage...]"
    echo "stages: ${STAGES[*]}"
    echo
    echo "gen    gn gen for out/$OUT_NAME with target_os=linux, target_cpu=$TARGET_CPU"
    echo "check  gn check over the same directory; any error is red"
    echo
    echo "This reads the tree and writes only out/$OUT_NAME. It never builds."
}

# Keep a python that gn's exec_script calls away from the caller's virtualenv.
path_without_virtualenv() {
    local out="" p
    IFS=: read -ra parts <<< "$PATH"
    for p in "${parts[@]}"; do
        case "$p" in *"/.venv/"*|*"/venv/"*) continue;; esac
        out="${out:+$out:}$p"
    done
    printf '%s' "$out"
}

gn_bin() {
    local gn="$ROOT/buildtools/mac/gn"
    [ -x "$gn" ] || die "no gn at $gn; run core-sync.sh deps first"
    printf '%s' "$gn"
}

# printing/BUILD.gn runs cups-config out of the sysroot, so a path that points
# at nothing will not do. core-sync.sh brings it with the linux DEPS entries.
require_sysroot() {
    [ -x "$SYSROOT/usr/bin/cups-config" ] \
        || die "no debian sysroot at ${SYSROOT#"$ROOT"/}; run core-sync.sh deps first"
}

stage_gen() {
    local gn out
    gn="$(gn_bin)"
    out="$ROOT/out/$OUT_NAME"
    [ -f "$FLAGS" ] || die "no linux gate flags at $FLAGS"
    require_sysroot

    mkdir -p "$out"
    { cat "$FLAGS"; echo "target_cpu = \"$TARGET_CPU\""; } > "$out/args.gn"
    ( cd "$ROOT" && env -u VPYTHON_BYPASS -u VIRTUAL_ENV -u PYTHONPATH -u PYTHONHOME \
        PATH="$(path_without_virtualenv)" "$gn" gen "out/$OUT_NAME" --fail-on-unused-args ) \
        || die "THE LINUX GATE IS RED. gn gen failed."
}

stage_check() {
    local gn out log errors missing
    gn="$(gn_bin)"
    out="$ROOT/out/$OUT_NAME"
    [ -f "$out/build.ninja" ] || die "no build files in out/$OUT_NAME; run the gen stage first"
    require_sysroot

    log="$out/.gn-check.log"
    ( cd "$ROOT" && "$gn" check "out/$OUT_NAME" --error-limit=100000 > "$log" 2>&1 ) || true
    errors=$(grep -c '^ERROR' "$log" || true)
    missing=$(grep -c 'Source file not found' "$log" || true)
    say "gn check: $errors errors, $missing of them a missing source"
    [ "$errors" = "0" ] \
        || die "THE LINUX GATE IS RED. read out/$OUT_NAME/.gn-check.log"
}

main() {
    case "${1:-}" in -h|--help) usage; exit 0;; esac
    [ $# -gt 0 ] || set -- "${STAGES[@]}"
    local s
    for s in "$@"; do
        case "$s" in
            gen)   stage_gen ;;
            check) stage_check ;;
            *) usage; die "unknown stage: $s" ;;
        esac
    done
}

main "$@"
