#!/usr/bin/env bash
# Checks the tree against the mac configuration, the sibling of
# core-gate-linux.sh. core-sync.sh owns out/dev and its gn gen; gn check reads
# the BUILD.gn files itself and writes nothing, so this is safe during a build.
set -euo pipefail

OUT_NAME="dev"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

die() { echo "core-gate-mac: $*" >&2; exit 1; }
say() { echo "core-gate-mac: $*"; }

usage() {
    echo "usage: core-gate-mac.sh"
    echo
    echo "gn check over out/$OUT_NAME; any error is red."
    echo "core-sync.sh configure owns the directory and its gn gen."
}

main() {
    case "${1:-}" in
        "") ;;
        -h|--help) usage; exit 0 ;;
        *) usage; die "takes no arguments; it runs gn check and nothing else" ;;
    esac

    local gn out log errors missing
    gn="$ROOT/buildtools/mac/gn"
    [ -x "$gn" ] || die "no gn at $gn; run core-sync.sh deps first"
    out="$ROOT/out/$OUT_NAME"
    [ -f "$out/build.ninja" ] || die "no build files in out/$OUT_NAME; run core-sync.sh configure first"

    log="$out/.gn-check.log"
    ( cd "$ROOT" && "$gn" check "out/$OUT_NAME" --error-limit=100000 > "$log" 2>&1 ) || true
    errors=$(grep -c '^ERROR' "$log" || true)
    missing=$(grep -c 'Source file not found' "$log" || true)
    say "gn check: $errors errors, $missing of them a missing source"
    [ "$errors" = "0" ] || die "THE MAC GATE IS RED. read out/$OUT_NAME/.gn-check.log"
}

main "$@"
