#!/usr/bin/env bash
# Works on the ninja graph of an output directory without touching the build.
#
# WHY. gn gen writes one .ninja file per target and never removes the file of a
# target that has gone, so a tree that loses targets accumulates ninja files
# build.ninja no longer subninjas. A survey that asks "does any ninja edge name
# this source" reads those files too and a dead source looks alive. Measured on
# this tree: out/dev carries 761 of them out of 13469, out/dev-linux and
# out/c2check carry none, and a gn gen in out/dev did not remove a single one.
#
# The answer is not to clean the build directory. `gn clean` deletes everything
# except args.gn, which takes the objects and the framework with it, and the
# framework is what the deletion work uses as its witness. A survey needs the
# graph, not the objects: out/dev-linux holds zero .o files and a complete graph.
# So `fresh` builds a gn-gen-only directory of its own and nothing else is
# disturbed.
#
# `prune` exists for the case where a directory has to be repaired in place. It
# is a dry run unless --write is given, it refuses the directories core-sync.sh
# and core-gate-linux.sh own, and it will not remove a symbolic link, anything
# under sdk/, or anything that is not a .ninja file. That last guard is not
# theoretical: `ninja -t cleandead` in out/dev walks sdk/xcode_links and calls
# remove() on paths inside the macOS SDK; here only the read-only SDK stopped it.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TOOLS="$ROOT/phantom_tools"
HERE="$TOOLS/clean-tool"
ENGINE="$HERE/graph_staleness.py"
FLAGS="$TOOLS/flags-dev.gn"
STAGING="$TOOLS/.deps/staging"
OWNED="dev dev-linux"          # core-sync.sh and core-gate-linux.sh own these

die() { echo "core-graph: $*" >&2; exit 1; }
say() { echo "core-graph: $*"; }

usage() {
    cat <<'USAGE'
usage: core-graph.sh measure <out-name> [--verbose] [--json]
       core-graph.sh fresh   <out-name> [--force-empty] [--compdb]
       core-graph.sh prune   <out-name> [--write] [--allow-owned]
       core-graph.sh selftest

measure   reports the ninja files the graph no longer reaches, the sources only
          those files still name, and which of those sources the tree has lost.
          Reads only; safe on a directory that is being built.

fresh     runs gn gen into out/<out-name> with the dev flags and nothing else,
          so the graph has no leftovers by construction. The directory has to be
          absent or empty; --force-empty empties it first, and it refuses a
          directory that holds object files. No compilation. --compdb also
          exports compile_commands.json.

prune     removes the unreached ninja files of out/<out-name>. A dry run unless
          --write. Refuses the owned directories unless --allow-owned. Never
          removes a symbolic link, anything under sdk/, or a file that is not a
          .ninja file.

selftest  runs the engine's own cases; eight of them, and each one turns red
          under a matching injection.
USAGE
}

owned() {
    local name="$1" one
    for one in $OWNED; do [ "$name" = "$one" ] && return 0; done
    return 1
}

need_engine() { [ -f "$ENGINE" ] || die "no engine at $ENGINE"; }

cmd_measure() {
    local name="${1:-}"; shift || true
    [ -n "$name" ] || { usage; die "measure needs an output directory name"; }
    need_engine
    local out="$ROOT/out/$name"
    [ -d "$out" ] || die "no such directory: out/$name"
    python3 "$ENGINE" "$out" --root "$ROOT" "$@"
}

cmd_fresh() {
    local name="${1:-}"; shift || true
    [ -n "$name" ] || { usage; die "fresh needs an output directory name"; }
    local force=0 compdb=0 arg
    for arg in "$@"; do
        case "$arg" in
            --force-empty) force=1 ;;
            --compdb) compdb=1 ;;
            *) die "fresh does not take $arg" ;;
        esac
    done
    owned "$name" && die "out/$name is owned by core-sync.sh or core-gate-linux.sh; \
pick another name so the build directory is left alone"

    local gn="$ROOT/buildtools/mac/gn"
    [ -x "$gn" ] || die "no gn at $gn; run core-sync.sh deps first"
    [ -f "$FLAGS" ] || die "no dev flags at $FLAGS"
    local out="$ROOT/out/$name"

    if [ -d "$out" ] && [ -n "$(ls -A "$out" 2>/dev/null)" ]; then
        local objects
        objects=$(find "$out" -name '*.o' -print -quit 2>/dev/null || true)
        [ -z "$objects" ] || die "out/$name holds object files; this command is for \
measurement directories, not for build directories"
        [ "$force" = "1" ] || die "out/$name is not empty; pass --force-empty to \
empty it, which is what makes the graph free of leftovers"
        say "emptying out/$name"
        rm -rf "$out"
    fi
    mkdir -p "$out"

    # The same arguments core-sync.sh writes, so the graph matches out/dev. The
    # compdb target list rides along in the file there; it is only a comment, and
    # it is kept so that a diff of the two args.gn files stays readable.
    { cat "$FLAGS"; echo 'target_cpu = "arm64"'; echo '# compdb targets: chrome'; } \
        > "$out/args.gn"

    local extra=()
    [ "$compdb" = "1" ] && extra+=(--export-compile-commands=chrome)
    say "generating build files for out/$name"
    ( cd "$ROOT" && env -u VPYTHON_BYPASS -u VIRTUAL_ENV -u PYTHONPATH -u PYTHONHOME \
        PATH="$STAGING/depot_tools:$PATH" \
        "$gn" gen "out/$name" --fail-on-unused-args "${extra[@]+"${extra[@]}"}" ) \
        || die "gn gen failed for out/$name"
    [ -f "$out/build.ninja" ] || die "gn gen finished but there is no build.ninja"

    need_engine
    say "the new graph, measured:"
    python3 "$ENGINE" "$out" --root "$ROOT"
}

cmd_prune() {
    local name="${1:-}"; shift || true
    [ -n "$name" ] || { usage; die "prune needs an output directory name"; }
    local write=0 allow=0 arg
    for arg in "$@"; do
        case "$arg" in
            --write) write=1 ;;
            --allow-owned) allow=1 ;;
            *) die "prune does not take $arg" ;;
        esac
    done
    if owned "$name" && [ "$allow" != "1" ]; then
        die "out/$name is owned by core-sync.sh or core-gate-linux.sh; measure it, \
or build a fresh directory with 'fresh', or pass --allow-owned if you mean it"
    fi
    need_engine
    local out="$ROOT/out/$name"
    [ -d "$out" ] || die "no such directory: out/$name"
    if [ "$write" = "1" ]; then
        python3 "$ENGINE" "$out" --root "$ROOT" --prune --write
    else
        python3 "$ENGINE" "$out" --root "$ROOT" --prune
    fi
}

main() {
    case "${1:-}" in
        measure) shift; cmd_measure "$@" ;;
        fresh)   shift; cmd_fresh "$@" ;;
        prune)   shift; cmd_prune "$@" ;;
        selftest) need_engine; python3 "$ENGINE" --selftest ;;
        -h|--help|"") usage ;;
        *) usage; die "unknown command: $1" ;;
    esac
}

main "$@"
