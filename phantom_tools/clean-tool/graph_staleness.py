#!/usr/bin/env python3
"""Reads a gn output directory and reports the ninja files the graph no longer
reaches, together with the source paths only those files still name.

WHY THIS EXISTS. gn gen writes one .ninja file per target under out/<dir>/obj and
never deletes the file of a target that has gone. A tree that loses targets
therefore accumulates ninja files that build.ninja does not subninja any more.
Measured on this tree: out/dev carries 761 such files of 13469 (5.65%), dated
between the eighth and the twenty-first of September, while out/dev-linux and
out/c2check carry none. Running gn gen does not remove them: gn gen ran in
out/dev after those files were already there and they stayed.

They matter because a survey that asks "does any ninja edge name this source"
reads them too, so a source whose only remaining mention sits in an unreachable
file looks alive when it is not. The set has an exact definition, which is what
this script computes:

    STALE = the .ninja files under the output directory that build.ninja does
            not reach through subninja and include, transitively.

WHAT IT REFUSES TO DELETE. Three guards, each with its own case in the selftest:
  * anything that is not a .ninja file,
  * anything that is a symbolic link,
  * anything under sdk/, where the tree keeps its link farm into the real SDK.
The third guard is not theoretical. `ninja -t cleandead` in out/dev walks
sdk/xcode_links/ and calls remove() on paths inside the macOS SDK itself; on this
machine only the read-only SDK stopped it. A tool that prunes this directory has
to name that boundary and hold it.

SOURCE PATHS. The reference needle is `../../<path>` with a source extension and
no further dot after it. Both halves are load bearing: without the extension
filter the needle also matches the relative fragments of include flags (../..,
../../.., ...), and without the no-further-dot rule it takes constants.java out
of constants.java.tmpl.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

# Extensions a reference has to carry to count as a source path. Anything else
# in a ninja file is a flag fragment, an output, or a stamp.
SOURCE_EXT = (
    'cc', 'c', 'cpp', 'cxx', 'h', 'hh', 'hpp', 'inc', 'm', 'mm', 'S', 'asm',
    'java', 'kt', 'py', 'ts', 'js', 'mojom', 'proto', 'grd', 'grdp', 'gni',
    'gn', 'json', 'json5', 'xml', 'html', 'css', 'txt', 'pyl', 'rs', 'swift',
)
REFERENCE = re.compile(
    r'\.\./\.\./([^\s:|"\']+\.(?:' + '|'.join(SOURCE_EXT) + r'))(?![.\w])')
SUBNINJA = re.compile(r'^\s*(?:subninja|include)\s+(\S+)\s*$', re.M)


def die(message):
    print('graph_staleness: ' + message, file=sys.stderr)
    sys.exit(1)


def reachable(out_dir):
    """The ninja files build.ninja reaches, transitively. Returns paths relative
    to out_dir. A file that is named but absent is still counted as named, so a
    broken subninja shows up as a missing file rather than as stale files."""
    seen, missing, pending = set(), set(), ['build.ninja']
    while pending:
        name = pending.pop()
        if name in seen or name in missing:
            continue
        path = os.path.join(out_dir, name)
        if not os.path.isfile(path):
            missing.add(name)
            continue
        seen.add(name)
        with open(path, encoding='utf-8', errors='replace') as handle:
            body = handle.read()
        for found in SUBNINJA.findall(body):
            if found not in seen:
                pending.append(found)
    return seen, missing


def ninja_files(out_dir):
    """Every .ninja file under out_dir, relative, symlinks reported separately."""
    plain, links = set(), set()
    for root, _dirs, files in os.walk(out_dir):
        for name in files:
            if not name.endswith('.ninja'):
                continue
            full = os.path.join(root, name)
            rel = os.path.relpath(full, out_dir)
            (links if os.path.islink(full) else plain).add(rel)
    return plain, links


def references(out_dir, names):
    """The source paths a set of ninja files names, relative to the tree root.

    A capture that still carries ../ of its own is dropped. The needle anchors on
    the two segments that lead out of the output directory, so a deeper path such
    as ../../../../../../../chrome/test/data/webui/tsconfig_base.json would
    otherwise arrive as ../../../../../chrome/... and be counted as a tree path
    that the tree does not have. Measured before the guard: 563 of the 790 paths
    reported as absent were this shape."""
    found = set()
    for name in names:
        path = os.path.join(out_dir, name)
        try:
            with open(path, encoding='utf-8', errors='replace') as handle:
                body = handle.read()
        except OSError:
            continue
        for hit in REFERENCE.findall(body):
            if hit.startswith('.') or '../' in hit:
                continue
            found.add(hit)
    return found


def tracked(root):
    out = subprocess.run(['git', '-C', root, 'ls-files'],
                         capture_output=True, text=True)
    return {line for line in out.stdout.split('\n') if line}


def absent(root, paths):
    """The paths the tree does not have, asked of the FILE SYSTEM.

    git ls-files is the wrong question here and the difference is large: it does
    not list the contents of the 261 gitlinks, nor build/mac_files, where
    core-sync.sh keeps the hermetic Xcode. Measured on out/dev: 32452 live
    references are outside the index, of which 26063 sit under a submodule and
    3167 under build/, while only 790 are genuinely not on disk."""
    return sorted(p for p in paths if not os.path.exists(os.path.join(root, p)))


def refused(out_dir, rel):
    """Why this path may not be removed, or None."""
    if not rel.endswith('.ninja'):
        return 'not a ninja file'
    if os.path.islink(os.path.join(out_dir, rel)):
        return 'symbolic link'
    first = rel.split(os.sep)[0]
    if first == 'sdk':
        return 'under sdk/, the link farm into the real SDK'
    if os.path.isabs(rel) or rel.split(os.sep)[0] == '..':
        return 'outside the output directory'
    return None


def survey(out_dir, root=None):
    if not os.path.isdir(out_dir):
        die('no such output directory: ' + out_dir)
    if not os.path.isfile(os.path.join(out_dir, 'build.ninja')):
        die('no build.ninja in ' + out_dir + '; gn gen has not run there')
    live, missing = reachable(out_dir)
    plain, links = ninja_files(out_dir)
    every = plain | links
    stale = sorted(every - live)
    result = {
        'out_dir': out_dir,
        'ninja_files': len(every),
        'symlinked_ninja_files': sorted(links),
        'reached': len(live & every),
        'named_but_absent': sorted(missing),
        'stale': stale,
        'stale_count': len(stale),
    }
    live_refs = references(out_dir, live & every)
    stale_refs = references(out_dir, stale)
    result['references_live'] = len(live_refs)
    result['references_stale'] = len(stale_refs)
    result['references_only_stale'] = sorted(stale_refs - live_refs)
    if root:
        result['absent_from_disk_live'] = absent(root, live_refs)
        result['absent_from_disk_only_stale'] = absent(
            root, stale_refs - live_refs)
        names = tracked(root)
        result['outside_git_index_live'] = sum(
            1 for p in live_refs if p not in names)
    result['refused'] = {p: refused(out_dir, p) for p in stale
                         if refused(out_dir, p)}
    result['removable'] = [p for p in stale if not refused(out_dir, p)]
    return result


def report(result, verbose=False):
    print('output directory : %s' % result['out_dir'])
    print('ninja files      : %d (%d reached, %d stale, %.2f%%)'
          % (result['ninja_files'], result['reached'], result['stale_count'],
             100.0 * result['stale_count'] / max(result['ninja_files'], 1)))
    if result['symlinked_ninja_files']:
        print('symlinked ninja  : %d (never removed)'
              % len(result['symlinked_ninja_files']))
    if result['named_but_absent']:
        print('named but absent : %d %s'
              % (len(result['named_but_absent']), result['named_but_absent'][:4]))
    print('source references: %d reached, %d in stale files, %d only in stale files'
          % (result['references_live'], result['references_stale'],
             len(result['references_only_stale'])))
    if 'absent_from_disk_live' in result:
        print('absent from disk : %d named by a reached file, %d only by a stale file'
              % (len(result['absent_from_disk_live']),
                 len(result['absent_from_disk_only_stale'])))
        print('outside git index: %d reached references (submodules and '
              'build/mac_files, not a defect)' % result['outside_git_index_live'])
    if result['refused']:
        print('refused          : %d' % len(result['refused']))
        for path, why in sorted(result['refused'].items())[:6]:
            print('    %-60s %s' % (path[:60], why))
    print('removable        : %d' % len(result['removable']))
    if verbose:
        for path in result['removable'][:40]:
            print('    ' + path)


def prune(result, write, out_dir):
    removable = result['removable']
    if not removable:
        print('prune: nothing to remove')
        return 0
    if not write:
        print('prune: %d files would go, and none of them is a symbolic link, '
              'under sdk/, or anything other than a .ninja file' % len(removable))
        for path in removable[:12]:
            print('    would remove ' + path)
        print('prune: this was a dry run; pass --write to remove')
        return 0
    gone = 0
    for path in removable:
        why = refused(out_dir, path)          # checked once more at the last moment
        if why:
            print('prune: refused %s (%s)' % (path, why))
            continue
        os.remove(os.path.join(out_dir, path))
        gone += 1
    print('prune: removed %d files' % gone)
    return gone


# ----------------------------------------------------------------- selftest
CASES = 10


def _mk(base, rel, body='', link_to=None):
    path = os.path.join(base, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if link_to is not None:
        os.symlink(link_to, path)
    else:
        with open(path, 'w', encoding='utf-8') as handle:
            handle.write(body)
    return path


def selftest():
    checks = []
    base = tempfile.mkdtemp(prefix='graph_staleness_selftest_')
    try:
        out = os.path.join(base, 'out')
        # build.ninja -> a.ninja -> b.ninja, and c.ninja reached through include
        _mk(out, 'build.ninja', 'subninja obj/a.ninja\ninclude obj/c.ninja\n')
        _mk(out, 'obj/a.ninja', 'subninja obj/b.ninja\n'
                                'build x: cxx ../../base/alive.cc\n')
        _mk(out, 'obj/b.ninja', 'build y: cxx ../../base/also_alive.cc\n')
        _mk(out, 'obj/c.ninja', 'build z: cxx ../../base/included.cc\n')
        # unreachable, plus the three guard shapes and a non-ninja file
        _mk(out, 'obj/gone.ninja', 'build w: cxx ../../base/only_stale.cc\n'
                                   '  flags = -I../../.. -I../../../..\n'
                                   '  inputs = ../../base/constants.java.tmpl\n'
                                   '  deeper = ../../../../../../../base/deep.cc\n')
        # base/alive.cc is put on disk so that the file system test has both an
        # answer that is there and answers that are not.
        _mk(base, 'base/alive.cc', '// on disk\n')
        _mk(out, 'sdk/keep.ninja', 'build s: cxx ../../base/sdk_side.cc\n')
        _mk(out, 'obj/linked.ninja', link_to=os.path.join(out, 'obj/a.ninja'))
        _mk(out, 'obj/keep.o', 'not a ninja file\n')

        result = survey(out)
        checks.append(('the reached closure follows subninja and include',
                       result['reached'] == 4, True))
        checks.append(('a file nothing reaches is stale, and only that file',
                       sorted(p for p in result['stale']
                              if p.endswith('gone.ninja')) == ['obj/gone.ninja']
                       and 'obj/a.ninja' not in result['stale'], True))
        # Hand derived from the fixture: build.ninja, a, b, c, gone, sdk/keep and
        # the link are seven ninja files; keep.o is not one of them. Four are
        # reached, so three are stale.
        checks.append(('the case is not empty, and a .o is not a ninja file',
                       result['ninja_files'] == 7 and result['reached'] == 4
                       and result['stale_count'] == 3, True))
        checks.append(('a symbolic link is refused even when unreachable',
                       result['refused'].get('obj/linked.ninja') == 'symbolic link'
                       and 'obj/linked.ninja' not in result['removable'], True))
        checks.append(('a ninja file under sdk/ is refused',
                       'sdk/' in str(result['refused'])
                       and 'sdk/keep.ninja' not in result['removable'], True))
        checks.append(('only the one file that passes all three guards is removable',
                       result['removable'] == ['obj/gone.ninja'], True))
        # the needle: only_stale carries the source and none of the flag fragments
        # The fixture's stale files name two sources, and gone.ninja also carries
        # `-I../../.. -I../../../..`. Both halves are asserted: the two sources
        # arrive, and no reference anywhere is a bare relative fragment.
        only = set(result['references_only_stale'])
        every_ref = only | set(references(out, reachable(out)[0]))
        checks.append(('the reference needle takes sources and not flag fragments',
                       only == {'base/only_stale.cc', 'base/sdk_side.cc'}
                       and not any(r.startswith('..') or r in ('..', '../..')
                                   for r in every_ref)
                       and 'base/constants.java' not in every_ref, True))
        # A reference that still carries ../ of its own is not a tree path.
        every_now = set(result['references_only_stale']) | set(
            references(out, reachable(out)[0]))
        checks.append(('a deeper relative reference is not taken as a tree path',
                       not any('../' in r or r.startswith('.') for r in every_now)
                       and 'base/deep.cc' not in every_now, True))
        # absent is asked of the file system: base/alive.cc was written, the rest
        # were not, and none of them is in any git index.
        with_root = survey(out, base)
        checks.append(('absent is asked of the file system, not of a git index',
                       'base/alive.cc' not in with_root['absent_from_disk_live']
                       and 'base/also_alive.cc' in with_root['absent_from_disk_live'], True))
        before = len(ninja_files(out)[0]) + len(ninja_files(out)[1])
        prune(result, False, out)
        after = len(ninja_files(out)[0]) + len(ninja_files(out)[1])
        checks.append(('a dry prune writes nothing', before == after, True))
    except Exception as error:                                  # noqa: BLE001
        print('  !! a case could not be built: %s: %s'
              % (type(error).__name__, str(error)[:110]))
    finally:
        shutil.rmtree(base, ignore_errors=True)

    bad = 0
    for name, found, want in checks:
        if found != want:
            bad += 1
            print('  XX %-62s expected %r, found %r' % (name, want, found))
        else:
            print('  ok %-62s' % name)
    if len(checks) != CASES:
        bad += 1
        print('  XX %-62s expected %d cases, built %d'
              % ('CASE COUNT', CASES, len(checks)))
    print('SELFTEST: %d/%d' % (len(checks) - bad, CASES))
    return bad == 0


def main():
    parser = argparse.ArgumentParser(
        description='report, and optionally remove, the ninja files a gn output '
                    'directory no longer reaches')
    parser.add_argument('out_dir', nargs='?', help='path to out/<name>')
    parser.add_argument('--root', help='core tree root, to name sources that '
                                       'the tree no longer carries')
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--verbose', action='store_true')
    parser.add_argument('--prune', action='store_true',
                        help='remove the stale files; a dry run unless --write')
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--selftest', action='store_true')
    args = parser.parse_args()

    if args.selftest:
        sys.exit(0 if selftest() else 1)
    if not args.out_dir:
        parser.error('an output directory is required')
    result = survey(os.path.abspath(args.out_dir), args.root)
    if args.json:
        print(json.dumps(result, indent=1))
    else:
        report(result, args.verbose)
    if args.prune:
        prune(result, args.write, os.path.abspath(args.out_dir))


if __name__ == '__main__':
    main()
