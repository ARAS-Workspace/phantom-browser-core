#!/usr/bin/env bash
# Keeps the Linux CI tree standing on an x86-64 Ubuntu host. core-sync.sh does
# the same for the mac development tree; this one serves the CI machine, which
# builds and tests linux x64 natively.
set -euo pipefail

STAGES="verify deps configure"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # root of the core tree
TOOLS="$ROOT/phantom_tools"
DEPS_DIR="$TOOLS/.deps"                                    # gclient runs from here
STAGING="$DEPS_DIR/staging"
FLAGS="$TOOLS/flags-ci-linux.gn"
OUT_NAME="ci"
SYSROOT="$ROOT/build/linux/debian_bullseye_amd64-sysroot"

# Chromium's DEPS writes its paths with a src/ prefix and does not set
# use_relative_paths, so gclient's solution name has to be "src". This checkout
# sits under a different directory name, so .deps/src is a symlink to the root of
# this tree and gclient works through it. Same arrangement as core-sync.sh.
usage() {
    echo "usage: core-sync-linux.sh [stage...]"
    echo "stages: $STAGES"
    echo "no stage runs them all, in order"
}

die() { echo "core-sync-linux: $*" >&2; exit 1; }
say() { echo "core-sync-linux: $*"; }

path_without_virtualenv() {
    local out="" entry
    local IFS=:
    for entry in $PATH; do
        if [ -n "${VIRTUAL_ENV-}" ] && [ "$entry" = "$VIRTUAL_ENV/bin" ]; then continue; fi
        out="${out:+$out:}$entry"
    done
    printf '%s' "$out"
}

report() {
    local stage="$1" started="$2"
    local elapsed=$(( SECONDS - started ))
    say "$stage done in ${elapsed}s"
}

DEPS_PATHS="v8 third_party/angle third_party/skia third_party/dawn"

deps_paths_present() {
    local path
    for path in $DEPS_PATHS; do
        [ -d "$ROOT/$path/.git" ] || [ -f "$ROOT/$path/.git" ] || return 1
        [ -n "$(ls -A "$ROOT/$path" 2>/dev/null)" ] || return 1
    done
    return 0
}

toolchain_present() {
    [ -x "$ROOT/third_party/llvm-build/Release+Asserts/bin/clang" ] || return 1
    [ -x "$ROOT/buildtools/linux64/gn" ] || return 1
    [ -x "$ROOT/third_party/ninja/ninja" ] || return 1
    return 0
}

# printing/BUILD.gn runs cups-config out of the sysroot, so the directory being
# there is not enough; the same test core-gate-linux.sh makes on the arm64 one.
sysroot_present() {
    [ -x "$SYSROOT/usr/bin/cups-config" ]
}

stage_verify() {
    local os arch check
    os="$(uname -s)"
    arch="$(uname -m)"
    [ "$os" = "Linux" ] || die "this script is for a linux host, this one is $os"
    [ "$arch" = "x86_64" ] || die "chromium builds linux on an x86-64 host only, this one is $arch"

    # Checks and installs nothing. The packages are put in place once, by hand,
    # with install-build-deps.py --no-prompt, which needs sudo.
    if ! check="$("$ROOT/build/install-build-deps.py" --quick-check 2>&1)"; then
        printf '%s\n' "$check" >&2
        die "build dependencies are missing; run: sudo $ROOT/build/install-build-deps.py --no-prompt"
    fi
    say "host OK: $os $arch, build dependencies in place"
}

stage_deps() {
    local started=$SECONDS
    local dt="$STAGING/depot_tools"
    local marker="$STAGING/deps-synced-at"
    local head dt_commit

    [ -d "$ROOT/.git" ] || die "this is not the core checkout: no .git at $ROOT"
    head="$(git -C "$ROOT" rev-parse HEAD)"

    if [ -f "$marker" ] && [ "$(cat "$marker")" = "$head" ] && deps_paths_present \
        && toolchain_present && sysroot_present; then
        say "deps already synced for $head, nothing to do"
        return 0
    fi

    dt_commit="$(sed -n "s|.*depot_tools\.git' + '@' + '\([0-9a-f]\{40\}\)'.*|\1|p" "$ROOT/DEPS" | head -1)"
    [ -n "$dt_commit" ] || die "could not read the depot_tools commit out of $ROOT/DEPS"
    say "depot_tools pinned by DEPS at $dt_commit"

    mkdir -p "$STAGING"
    if [ -d "$dt/.git" ] && [ "$(git -C "$dt" rev-parse HEAD 2>/dev/null)" = "$dt_commit" ]; then
        say "depot_tools already at $dt_commit, keeping its bootstrapped state"
    else
        if [ ! -d "$dt/.git" ]; then
            rm -rf "$dt"; mkdir -p "$dt"
            git -C "$dt" init -q
            git -C "$dt" remote add origin \
                "https://chromium.googlesource.com/chromium/tools/depot_tools"
        fi
        git -C "$dt" fetch --depth=1 origin "$dt_commit"
        git -C "$dt" reset --hard "$dt_commit"
        git -C "$dt" clean -ffd
    fi

    # Create the link when it is missing, but do not silently rewrite one that
    # points elsewhere: someone changed it on purpose, or the layout moved, and
    # either way gclient would write the dependency trees into the wrong place.
    mkdir -p "$DEPS_DIR"
    if [ -L "$DEPS_DIR/src" ] || [ -e "$DEPS_DIR/src" ]; then
        if ! [ -L "$DEPS_DIR/src" ] ||
            [ "$(cd "$DEPS_DIR/src" && pwd -P)" != "$ROOT" ]; then
            die "$DEPS_DIR/src does not resolve to $ROOT; remove it and run again"
        fi
    else
        ln -s ../.. "$DEPS_DIR/src"
        [ "$(cd "$DEPS_DIR/src" && pwd -P)" = "$ROOT" ] || die "the new $DEPS_DIR/src link does not resolve to $ROOT"
    fi

    cat > "$DEPS_DIR/.gclient" <<EOF
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {
      "checkout_configuration": "small",
      # flags-ci-linux.gn sets chrome_pgo_phase = 0, so nothing reads a profile.
      "checkout_pgo_profiles": False,
    },
  },
];
# The CI machine builds linux x64 and nothing else, so gclient brings the linux
# entries of DEPS and the amd64 debian sysroot, and no other os or cpu.
target_os = ['linux'];
target_os_only = True;
target_cpu = ['x64'];
target_cpu_only = True;
EOF

    say "running gclient sync with hooks, this brings chromium's own toolchain"
    ( cd "$DEPS_DIR" && env -u VPYTHON_BYPASS -u VIRTUAL_ENV -u PYTHONPATH -u PYTHONHOME \
        GCLIENT_FILE="$DEPS_DIR/.gclient" \
        DEPOT_TOOLS_UPDATE=0 \
        DEPOT_TOOLS_METRICS=0 \
        PYTHONDONTWRITEBYTECODE=1 \
        PATH="$dt:$(path_without_virtualenv)" \
        "$dt/gclient" sync -f -D -R --no-history )

    local clang="$ROOT/third_party/llvm-build/Release+Asserts/bin/clang"
    [ -x "$clang" ] || die "gclient finished but chromium's own clang is not at $clang"
    say "chromium's own clang is in place: $("$clang" --version | head -1)"

    deps_paths_present || die "gclient finished but a dependency tree is missing or empty ($DEPS_PATHS)"
    toolchain_present || die "gclient finished but gn or ninja is missing"
    sysroot_present || die "gclient finished but there is no debian sysroot at ${SYSROOT#"$ROOT"/}"
    say "dependency trees in place: $DEPS_PATHS, and ${SYSROOT#"$ROOT"/}"

    printf '%s' "$head" > "$marker"
    report deps "$started"
}

stage_configure() {
    local started=$SECONDS
    local gn="$ROOT/buildtools/linux64/gn"
    local out="$ROOT/out/$OUT_NAME"
    local desired="$out/.args.desired" previous="$out/.args.previous"

    [ -x "$gn" ] || die "no gn at $gn, the deps stage has not finished"
    [ -f "$FLAGS" ] || die "no CI flags at $FLAGS"
    sysroot_present || die "no debian sysroot at ${SYSROOT#"$ROOT"/}, the deps stage has not finished"

    mkdir -p "$out"
    cp "$FLAGS" "$desired"

    # gn gen runs on every configure, as in core-sync.sh: ninja regenerates
    # build.ninja on its own when a gn file changes, but the rule it uses does
    # not carry --fail-on-unused-args, so an argument whose declare_args has gone
    # would be reported by nobody.
    local wrote_args=0
    if [ -f "$out/args.gn" ] && cmp -s "$out/args.gn" "$desired"; then
        rm -f "$desired"
    else
        rm -f "$previous"
        if [ -f "$out/args.gn" ]; then cp "$out/args.gn" "$previous"; fi
        mv "$desired" "$out/args.gn"
        wrote_args=1
    fi

    say "generating build files for out/$OUT_NAME"
    if ! ( cd "$ROOT" && env -u VPYTHON_BYPASS -u VIRTUAL_ENV -u PYTHONPATH -u PYTHONHOME \
        PATH="$STAGING/depot_tools:$(path_without_virtualenv)" \
        "$gn" gen "out/$OUT_NAME" --fail-on-unused-args ); then
        if [ -f "$previous" ]; then
            mv "$previous" "$out/args.gn"
            die "gn gen failed, the previous arguments are back in place"
        fi
        if [ "$wrote_args" = "1" ]; then rm -f "$out/args.gn"; fi
        die "gn gen failed and there were no arguments to go back to"
    fi
    rm -f "$previous"
    [ -f "$out/build.ninja" ] || die "gn gen finished but there is no build.ninja in $out"
    report configure "$started"
}

run_stage() {
    case "$1" in
        verify) stage_verify ;;
        deps) stage_deps ;;
        configure) stage_configure ;;
        *) die "unknown stage: $1" ;;
    esac
}

main() {
    local stages="$STAGES"
    if [ $# -gt 0 ]; then
        case "$1" in -h|--help) usage; exit 0 ;; esac
        stages="$*"
    fi
    local stage
    for stage in $stages; do run_stage "$stage"; done
}

main "$@"
