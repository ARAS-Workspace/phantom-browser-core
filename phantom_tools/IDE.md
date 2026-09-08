# CLion setup

How to point CLion at this tree. Build first: the generated headers under
`out/dev/gen` only exist after ninja has run, and without them the IDE cannot
resolve thousands of includes.

    phantom_tools/core-sync.sh
    phantom_tools/core-build.sh -j 10

## Before opening the project

These three touch your home directory.

**1. Raise the file size limit for indexing.** Generated files often exceed the
2500 KB default.

    printf 'idea.max.intellisense.filesize=12500\n' \
      >> ~/Library/Application\ Support/JetBrains/CLion2026.2/idea.properties

**2. Heap.** `clion.vmoptions` ships with `-Xmx2048m`. Set it to at least 8g:

    Help | Edit Custom VM Options   ->   -Xmx8196m

**3. lldb.** Chromium's helpers restore source-level debugging and teach the
debugger to print its pointer, string and vector types.

    cat >> ~/.lldbinit <<'LLDB'
    script sys.path[:0] = ['<tree>/tools/lldb']
    script import lldbinit
    script import chromium_visualizers
    LLDB

Replace `<tree>` with the absolute path of this checkout.

## Opening the project

**4.** `File | Open`, select `compile_commands.json` at the root of this tree, then
**Open as Project**. This is what makes it a C++ project; there is no file-based
equivalent.

**5. Exclude what you never read.** Do this before the first index; it removes
roughly 235000 of the 499000 files.

    third_party/blink/web_tests   ios   ash   chromeos   android_webview   docs   infra

Select them in the Project view, right-click, `Mark Directory As`, `Excluded`.
Keep `third_party/blink/renderer`.

## Building and debugging from the IDE

CLion cannot build a compilation database project by itself. It offers code
analysis and a single-file `Recompile`; everything else goes through a custom
target.

**6. Custom Build Target.** `Settings | Build, Execution, Deployment | Custom Build
Targets`. The Build field takes an **External Tool**, created with the `...` button:

    Program:           <tree>/phantom_tools/core-build.sh
    Arguments:         -j 10
    Working directory: <tree>

These tools are stored per project and do not appear under `Tools | External Tools`.

**7. Run configuration.** `Run | Edit Configurations`, add a **Custom Build
Application**, select the target from step 6, and give it the bundle:

    out/dev/Phantom Browser.app

The target lands in `Before launch`, so running the configuration builds first.

**8. Debugger.** `Settings | Build, Execution, Deployment | Debugger | Debug
Profiles`, create an LLDB profile, then pick it from the toolbar switcher. The
Debugger field on the Toolchains page is deprecated.

## What you do not need to change

The compiler fields on the Toolchains page can stay as detected. Every entry in
`compile_commands.json` already names this tree's own clang, so code insight reads
the real per-file flags.

## Expected behaviour

- **Excluded folders stay visible** in the Project view. Exclusion stops indexing,
  it does not hide. `.idea/misc.xml` is the record of what is excluded.
- **"Found several command objects for file ... Using only one"** appears for files
  compiled for both the host and the target toolchain. CLion picks one. This is
  information, not an error.
- **The debugger may report a local as optimized out.** This tree builds at `-O2`
  with `dcheck_always_on=true`; `symbol_level=2` keeps lines and types.
- **A new source file does not refresh the database.** `core-sync.sh configure`
  compares build arguments, not the source list. Run `core-sync.sh compdb` after
  adding a file.
