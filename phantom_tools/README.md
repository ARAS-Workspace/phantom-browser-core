# phantom_tools: development line

This tree is for **development**: the IDE, the debugger, running tests, a fast
incremental loop. The production package is built in the `phantom-browser`
repository, whose `config/flags.gn` holds the production flags. Those flags are not
copied here, and the two trees do not share a build directory.

|                 | here (core)                                                  | phantom-browser (line)                |
|-----------------|--------------------------------------------------------------|---------------------------------------|
| purpose         | development, tests, debugging                                | production, CI, packaging             |
| flags           | `flags-dev.gn`: DCHECK on, LTO and PGO off, `symbol_level=2` | `config/flags.gn`: official, LTO, PGO |
| build directory | `out/dev`                                                    | `out/arm64`, `out/x64`                |

## Scripts

    phantom_tools/core-sync.sh [verify deps configure compdb]
    phantom_tools/core-build-mac.sh [-j N] [target...]
    phantom_tools/core-gate-linux.sh [gen check]

Stages of `core-sync.sh`:

- **verify** checks the host (Xcode, metal).
- **deps** fetches the depot_tools commit that `DEPS` pins, then runs `gclient sync`.
  If the marker (`phantom_tools/.deps/staging/deps-synced-at`) matches HEAD and the
  dependency trees are in place, it does nothing.
- **configure** runs `gn gen` for `out/dev` with `--fail-on-unused-args` and
  `--export-compile-commands="$COMPDB_TARGETS"`, on every call. ninja regenerates
  `build.ninja` on its own when a gn file changes, but the rule it uses carries
  neither flag, so a run from here is what keeps `compile_commands.json` and the
  unused-argument check current. The arguments file is rewritten only when it
  differs.

  `COMPDB_TARGETS` decides which targets the compilation database covers, and it
  defaults to `chrome`. Without it `gn` writes a command for every target it knows,
  including tests and fuzzers that never ran, and the IDE then reports every file
  those targets would have generated as missing. The list is written into `args.gn`
  as a comment, so changing it re-runs `gn gen`. Widen it when a test binary joins
  the loop, for example `"chrome unit_tests"`.
- **compdb** links `compile_commands.json` at the root of the tree. This is what
  gives the IDE a real understanding of the code.

`IDE.md` is the CLion how-to: what to set before opening the project, how to open
it, and how to build and debug from inside the IDE.

## Why `.deps/src` is a symlink

Chromium's `DEPS` file writes its paths with a `src/` prefix and does not set
`use_relative_paths`, so gclient's solution name **has to be `src`**. This checkout
sits under a different directory name, so `phantom_tools/.deps/src` is created as a
symlink to the root of this tree and gclient works through it. `core-sync.sh` asserts
that the link really resolves to the root.

`.deps/` is local and is not committed (`phantom_tools/.gitignore`). The dependency
trees gclient leaves in the tree, `out/`, and the generated `compile_commands.json`
are listed in this repository's own `.gitignore`, under a block that names them.
This is a fork: upstream reaches this tree through a loop we control, not through a
merge, so that file is ours to edit.

## The linux gate

This tree is built for mac. Linux is verified, not built: a linux link on a mac
host is not a configuration chromium supports, and the value we want does not
need one.

    phantom_tools/core-gate-linux.sh gen    # gn gen out/dev-linux, target_os=linux
    phantom_tools/core-gate-linux.sh check  # gn check over the same directory

`flags-dev-linux.gn` carries the arguments; `flags-dev.gn` is left alone, since
it holds `use_system_xcode=false`, which only means something on mac.
`out/dev-linux` holds ninja files and nothing else, well under a gigabyte.

Run it before every deletion package. `gn gen` reads every BUILD.gn with
target_os="linux" and resolves every label, which is where two defects appeared
that the mac gates had stayed silent about: a `//media/gpu/chromeos` label with
no BUILD.gn behind it any more, and an unconditional sources row in `ui/aura`,
a file the mac configuration does not load at all because `use_aura` is false
there.

## Linux CI

A push to `ci/build` builds and tests this tree on the self-hosted linux x64
runner (`.github/workflows/phantom-linux-build.yml`). The machine keeps the tree
at `/srv/phantom/core` and the test runs under `/srv/phantom/ci`.

    phantom_tools/core-sync-linux.sh [verify deps configure]
    phantom_tools/core-build-linux.sh [-j N] [target...]
    phantom_tools/core-test.sh [-j N] [-b N] [-r N] [-R N] [-C N] [--base DIR] FILTER_JSON RESULT_DIR
    phantom_tools/core-ci-test.sh RUN_NAME

- `core-sync-linux.sh` does for `out/ci` what `core-sync.sh` does for `out/dev`,
  with `flags-ci-linux.gn` and a gclient checkout limited to linux x64.
- `core-test.sh` runs the binaries of a filter file under `testing/xvfb.py` and
  keeps the launcher's summaries. A test counts by its last run. With `--base`
  the run fails only on tests that are red now and were not red in the earlier
  run. `-R` runs the failed tests once more with N jobs; `-C` runs the new red
  tests once more, one per process and one at a time, before the verdict.
- `core-ci-test.sh` runs the two CI suites, `ci-filter-unit.json` (`-b 10`) and
  `ci-filter-browser.json` (`-R 8`), each against the last green run of that
  suite (`/srv/phantom/ci/taban-<suite>`), and moves the base of a green suite
  to the new run.

browser_tests runs 32 browsers on one virtual display, and some tests fail only
there. A test that fails beside the others and passes on its own is listed as
flaky and does not fail the run.

The machine's AppArmor profile allows the namespace sandbox to the binaries in
`out/ci` and to no other output directory.
