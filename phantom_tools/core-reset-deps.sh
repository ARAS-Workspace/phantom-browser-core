#!/usr/bin/env bash
# Clear what gclient owns, so that core-sync.sh has to bring it back.
#
# Why a script and not a pasted command: everything here is a recursive delete
# inside a checkout that has already been damaged once by a clever one-liner.
# The rule that keeps it safe is mechanical rather than careful -- a path is
# removed only when git tracks no file underneath it, which is true of the
# trees gclient and cipd write and false of anything the repository carries.
# Submodule checkouts fail that test through their gitlink, so they are never
# in the list; gclient's own sync -f -D -R is what resets those.
#
# It prints and stops. Pass --yes to make it delete.
set -euo pipefail
trap 'echo "core-reset-deps: interrupted" >&2' INT TERM

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
YES=0
WITH_OUT=0

die() { echo "core-reset-deps: $*" >&2; exit 1; }
say() { echo "core-reset-deps: $*"; }

usage() {
    echo "usage: core-reset-deps.sh [--yes] [--with-out]"
    echo
    echo "Removes the ignored trees gclient and cipd write, so the next"
    echo "core-sync.sh has to fetch them again. Without --yes it only prints."
    echo
    echo "--with-out  also remove out/, which forces a full rebuild."
}

while [ $# -gt 0 ]; do
    case "$1" in
        --yes) YES=1; shift ;;
        --with-out) WITH_OUT=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) usage; die "unknown argument: $1" ;;
    esac
done

cd "$ROOT"
[ -e .git ] || die "no .git at $ROOT; this is not the core checkout"

# What is kept whatever git says about it. out/ is the build, not a dependency,
# and it goes only when it is asked for. .idea belongs to whoever works here.
# The __pycache__ trees are rewritten by any python run and are not worth the
# noise in the list.
always_kept() {
    case "$1" in
        out/|.idea/) return 0 ;;
        *__pycache__/) return 0 ;;
    esac
    return 1
}

# Read with a loop rather than mapfile, and keep every array expansion behind
# a length test: the bash on the PATH here is 5.3, but /bin/bash on macOS is
# still 3.2, where mapfile does not exist and "${arr[@]}" on an empty array is
# an unbound variable under set -u. Nothing else in phantom_tools needs a
# bash newer than that one, and this script should not be the exception.
chosen=()
while IFS= read -r path; do
    if always_kept "$path"; then
        if [ "$path" != "out/" ] || [ "$WITH_OUT" != 1 ]; then
            continue
        fi
    fi
    # The test that makes this safe is that git tracks nothing underneath.
    # Asking check-ignore about the directory itself is not enough and not
    # even right: .gitignore:103 reads /base/tracing/test/data/*, which ignores
    # what is inside without ignoring the directory, so check-ignore says no
    # while every file below is ignored and none is tracked. A submodule fails
    # this test through its gitlink, which is what keeps those out of the list.
    [ -z "$(git ls-files -- "$path")" ] \
        || die "git tracks files under $path; refusing"
    case "$path" in
        .git/|*/.git/) die "refusing to touch a git directory: $path" ;;
    esac
    chosen+=("$path")
done < <(git ls-files --others --ignored --exclude-standard \
    --directory --no-empty-directory | grep '/$')

[ "${#chosen[@]}" -gt 0 ] || die "nothing to remove"

say "${#chosen[@]} trees, git tracks no file under any of them:"
for path in "${chosen[@]}"; do
    printf '    %-52s %s\n' "$path" "$(du -sh "$path" 2>/dev/null | cut -f1)"
done
say "total $(du -sch "${chosen[@]}" 2>/dev/null | tail -1 | cut -f1)"

changed_before=$(git status --porcelain | wc -l | tr -d ' ')
say "working tree carries $changed_before changed paths before this runs"

if [ "$YES" != 1 ]; then
    echo
    say "nothing was removed. pass --yes to remove the trees above."
    exit 0
fi

for path in "${chosen[@]}"; do
    rm -rf "$path"
done

changed_after=$(git status --porcelain | wc -l | tr -d ' ')
say "removed. the working tree carries $changed_after changed paths"
[ "$changed_before" = "$changed_after" ] \
    || die "the working tree moved from $changed_before to $changed_after changed paths; read git status before going on"
say "run core-sync.sh to bring them back"
