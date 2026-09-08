#!/usr/bin/env bash
# Keeps the development tree standing. The production line lives in the
# phantom-browser repository and does not reach in here; this tree is the one an
# IDE is pointed at, tests are run in, and a debugger is attached to.
set -euo pipefail

STAGES="verify deps configure compdb"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # root of the core tree
TOOLS="$ROOT/phantom_tools"
DEPS_DIR="$TOOLS/.deps"                                    # gclient runs from here
STAGING="$DEPS_DIR/staging"
FLAGS="$TOOLS/flags-dev.gn"
OUT_NAME="dev"

# Which targets the compilation database covers. gn writes a command for every
# target it knows unless this is narrowed, and the IDE then reports every file
# that target never generated as missing: 2515 of them here, all under gen/.
# Widen this when a test binary joins the loop, e.g. "chrome unit_tests".
COMPDB_TARGETS="chrome"

# Chromium's DEPS writes its paths with a src/ prefix and does not set
# use_relative_paths, so gclient's solution name has to be "src". This checkout
# sits under a different directory name, so .deps/src is a symlink to the root of
# this tree and gclient works through it.
usage() {
    echo "usage: core-sync.sh [stage...]"
    echo "stages: $STAGES"
    echo "no stage runs them all, in order"
}

die() { echo "core-sync: $*" >&2; exit 1; }
say() { echo "core-sync: $*"; }

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
    local stage="$1" started="$2" elapsed=$(( SECONDS - started ))
    say "$stage done in ${elapsed}s"
}

DEPS_PATHS="v8 third_party/angle third_party/skia third_party/dawn"
PGO_TARGETS="mac"

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
    [ -x "$ROOT/buildtools/mac/gn" ] || return 1
    [ -x "$ROOT/third_party/ninja/ninja" ] || return 1
    return 0
}

stage_verify() {
    "$TOOLS/verify-host"
}

stage_deps() {
    local started=$SECONDS
    local dt="$STAGING/depot_tools"
    local marker="$STAGING/deps-synced-at"
    local head dt_commit

    [ -d "$ROOT/.git" ] || die "this is not the core checkout: no .git at $ROOT"
    head="$(git -C "$ROOT" rev-parse HEAD)"

    if [ -f "$marker" ] && [ "$(cat "$marker")" = "$head" ] && deps_paths_present && toolchain_present; then
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
        [ -L "$DEPS_DIR/src" ] && [ "$(cd "$DEPS_DIR/src" && pwd -P)" = "$ROOT" ] ||
            die "$DEPS_DIR/src does not resolve to $ROOT; remove it and run again"
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
      "checkout_pgo_profiles": True,
    },
  },
];
target_os = ['mac'];
target_os_only = True;
target_cpu = ['arm64'];
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
    say "dependency trees in place: $DEPS_PATHS"

    printf '%s' "$head" > "$marker"
    report deps "$started"
}

stage_configure() {
    local started=$SECONDS
    local gn="$ROOT/buildtools/mac/gn"
    local out="$ROOT/out/$OUT_NAME"
    local desired="$out/.args.desired" previous="$out/.args.previous"

    [ -x "$gn" ] || die "no gn at $gn, the deps stage has not finished"
    [ -f "$FLAGS" ] || die "no dev flags at $FLAGS"

    mkdir -p "$out"
    # The target list rides along in the args file so that changing it re-runs gn.
    { cat "$FLAGS"; echo 'target_cpu = "arm64"'; echo "# compdb targets: $COMPDB_TARGETS"; } > "$desired"

    if [ -f "$out/args.gn" ] && [ -f "$out/build.ninja" ] && cmp -s "$out/args.gn" "$desired"; then
        rm -f "$desired"
        say "configure for $OUT_NAME is already current"
        report configure "$started"
        return 0
    fi

    rm -f "$previous"
    [ -f "$out/args.gn" ] && cp "$out/args.gn" "$previous"
    mv "$desired" "$out/args.gn"
    say "generating build files for out/$OUT_NAME"
    if ! ( cd "$ROOT" && env -u VPYTHON_BYPASS -u VIRTUAL_ENV -u PYTHONPATH -u PYTHONHOME \
        PATH="$STAGING/depot_tools:$(path_without_virtualenv)" \
        "$gn" gen "out/$OUT_NAME" --fail-on-unused-args --export-compile-commands="$COMPDB_TARGETS" ); then
        if [ -f "$previous" ]; then
            mv "$previous" "$out/args.gn"
            die "gn gen failed, the previous arguments are back in place"
        fi
        rm -f "$out/args.gn"
        die "gn gen failed and there were no arguments to go back to"
    fi
    rm -f "$previous"
    [ -f "$out/build.ninja" ] || die "gn gen finished but there is no build.ninja in $out"
    report configure "$started"
}

stage_compdb() {
    local started=$SECONDS
    local src="$ROOT/out/$OUT_NAME/compile_commands.json"
    local dst="$ROOT/compile_commands.json"

    [ -f "$src" ] || die "no compile_commands.json in out/$OUT_NAME; run the configure stage first"
    ln -sfn "out/$OUT_NAME/compile_commands.json" "$dst"
    say "compile_commands.json linked at the tree root ($(wc -c < "$src" | tr -d ' ') bytes)"
    say "point the IDE at $ROOT and let it read this file"
    report compdb "$started"
}

run_stage() {
    case "$1" in
        verify) stage_verify ;;
        deps) stage_deps ;;
        configure) stage_configure ;;
        compdb) stage_compdb ;;
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
