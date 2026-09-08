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
    phantom_tools/core-build.sh [-j N] [target...]

Stages of `core-sync.sh`:

- **verify** checks the host (Xcode, metal).
- **deps** fetches the depot_tools commit that `DEPS` pins, then runs `gclient sync`.
  If the marker (`phantom_tools/.deps/staging/deps-synced-at`) matches HEAD and the
  dependency trees are in place, it does nothing.
- **configure** runs `gn gen` for `out/dev` with `--fail-on-unused-args` and
  `--export-compile-commands="$COMPDB_TARGETS"`. It skips when the arguments have
  not changed.

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
