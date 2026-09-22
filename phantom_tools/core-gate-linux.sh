#!/usr/bin/env bash
# The linux gate. This tree is built for mac; linux is VERIFIED, not built.
#
# Why the name says gate and not build: a linux link on a mac host is not a
# configuration chromium supports, and the value we want does not need one.
# gn gen reads every BUILD.gn with target_os="linux" and resolves every label,
# which is where the two defects this gate was written for would have appeared:
# a //media/gpu/chromeos label with no BUILD.gn behind it any more, and an
# unconditional sources row in ui/aura, a file the mac configuration does not
# even load because use_aura is false there.
#
# Run it before every deletion package. It costs seconds and under a gigabyte;
# out/dev-linux holds ninja files and nothing else.
set -euo pipefail
trap 'echo "core-gate-linux: interrupted" >&2' INT TERM

STAGES="gen check"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS="$ROOT/phantom_tools"
FLAGS="$TOOLS/flags-dev-linux.gn"
OUT_NAME="dev-linux"
TARGET_CPU="arm64"

die() { echo "core-gate-linux: $*" >&2; exit 1; }
say() { echo "core-gate-linux: $*"; }
report() { local stage="$1" started="$2"; say "$stage done in $(( SECONDS - started ))s"; }

usage() {
    echo "usage: core-gate-linux.sh [stage...]"
    echo "stages: $STAGES"
    echo
    echo "gen    gn gen for out/$OUT_NAME with target_os=linux, target_cpu=$TARGET_CPU"
    echo "check  gn check over the same directory"
    echo
    echo "This reads the tree and writes only out/$OUT_NAME. It never builds."
}

# The same shape as core-sync.sh: keep a python that gn's exec_script calls
# away from whatever virtualenv the caller is sitting in.
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

stage_gen() {
    local started=$SECONDS
    local gn out desired previous
    gn="$(gn_bin)"
    out="$ROOT/out/$OUT_NAME"
    desired="$out/.args.desired"
    previous="$out/.args.previous"

    [ -f "$FLAGS" ] || die "no linux gate flags at $FLAGS"

    mkdir -p "$out"
    { cat "$FLAGS"; echo "target_cpu = \"$TARGET_CPU\""; } > "$desired"

    if [ -f "$out/args.gn" ] && [ -f "$out/build.ninja" ] && cmp -s "$out/args.gn" "$desired"; then
        rm -f "$desired"
        say "gate for $OUT_NAME is already current"
        gen_counts "$out"
        report gen "$started"
        return 0
    fi

    rm -f "$previous"
    [ -f "$out/args.gn" ] && cp "$out/args.gn" "$previous"
    mv "$desired" "$out/args.gn"
    say "generating linux build files for out/$OUT_NAME"
    if ! ( cd "$ROOT" && env -u VPYTHON_BYPASS -u VIRTUAL_ENV -u PYTHONPATH -u PYTHONHOME \
        PATH="$(path_without_virtualenv)" \
        "$gn" gen "out/$OUT_NAME" --fail-on-unused-args ); then
        if [ -f "$previous" ]; then
            mv "$previous" "$out/args.gn"
            die "THE LINUX GATE IS RED. gn gen failed; the previous arguments are back in place."
        fi
        rm -f "$out/args.gn"
        die "THE LINUX GATE IS RED. gn gen failed and there were no arguments to go back to."
    fi
    rm -f "$previous"
    [ -f "$out/build.ninja" ] || die "gn gen finished but there is no build.ninja in $out"
    gen_counts "$out"
    report gen "$started"
}

gen_counts() {
    local out="$1" line
    line=$(cd "$ROOT" && "$(gn_bin)" gen "out/$OUT_NAME" 2>/dev/null | tail -1) || true
    case "$line" in
        Done*) say "${line#Done. }";;
    esac
    say "out/$OUT_NAME holds $(du -sh "$out" 2>/dev/null | cut -f1) of ninja files"
}

stage_check() {
    local started=$SECONDS
    local gn out log errors
    gn="$(gn_bin)"
    out="$ROOT/out/$OUT_NAME"
    [ -f "$out/build.ninja" ] || die "no build files in out/$OUT_NAME; run the gen stage first"
    log="$out/.gn-check.log"
    ( cd "$ROOT" && "$gn" check "out/$OUT_NAME" --error-limit=100000 > "$log" 2>&1 ) || true
    errors=$(grep -c '^ERROR' "$log" || true)
    say "gn check: $(wc -l < "$log" | tr -d ' ') lines, $errors errors, full log at out/$OUT_NAME/.gn-check.log"
    report check "$started"
}

main() {
    case "${1:-}" in -h|--help) usage; exit 0;; esac
    local stages="${*:-$STAGES}" s
    for s in $stages; do
        case "$s" in
            gen)   stage_gen ;;
            check) stage_check ;;
            *) usage; die "unknown stage: $s" ;;
        esac
    done
}

main "$@"
