# clean-tool

`core-graph.sh` works on the ninja graph of an output directory. It answers two
questions that look like one and are not, and it can build a measuring directory
of its own so that neither answer has to come from the build directory.

## Two kinds of staleness

**Leftover ninja files.** gn gen writes one `.ninja` file per target and never
removes the file of a target that has gone. The set has an exact definition: the
`.ninja` files the output directory holds that `build.ninja` does not reach
through `subninja` and `include`, transitively. Running gn gen does not clear
them; gn gen ran in `out/dev` after those files were already there and every one
of them stayed.

They distort a survey. A survey that asks "does any ninja edge name this source"
reads the leftovers too, so a source whose only remaining mention sits in an
unreachable file reads as alive. Measured on `out/dev`: 761 leftover files of
13469, and **2476** source paths that no reached file names and the tree no
longer has.

**An outdated graph.** A directory can hold zero leftovers and still describe a
tree that has moved on, because every one of its files was written before the
sources went. `out/c2check` is the example: 0 leftovers, and 4143 of the sources
its reached files name are not on disk.

`measure` reports both, so "this graph is clean" can be said about the right one.

## Why not clean the build directory

`gn clean` deletes the contents of the output directory except `args.gn`, which
takes the objects and the framework with them. The framework is the witness the
deletion work compares against, so that is the wrong trade.

`ninja -t cleandead` is worse than the wrong trade. In `out/dev` it walks
`sdk/xcode_links/` and calls `remove()` on paths inside the macOS SDK itself; on
this machine only the read-only SDK stopped it, and it leaves every leftover
`.ninja` file in place. Do not run it here.

## Commands

    core-graph.sh measure <out-name>    read only, safe during a build
    core-graph.sh fresh   <out-name>    gn gen into an empty directory
    core-graph.sh prune   <out-name>    a dry run unless --write
    core-graph.sh selftest              ten cases

`fresh` and `prune` refuse `out/dev` and `out/dev-linux` by name, because
`core-sync.sh` and `core-gate-linux.sh` own those. `prune` will not remove a
symbolic link, anything under `sdk/`, or a file that is not a `.ninja` file, and
it checks each guard once more at the moment of removal.

## The selftest

Ten cases, and each turns red under a matching injection: the reached closure
following `include` as well as `subninja`, the leftover set, the case being
non-empty, the symlink guard, the `sdk/` guard, only the file that passes all
three guards being removable, the reference needle taking sources rather than the
relative fragments of include flags, a deeper relative reference not being read
as a tree path, absent being asked of the file system rather than of a git index,
and a dry run writing nothing.

The last two are there because both were wrong first. The needle took
`constants.java` out of `constants.java.tmpl` and read `-I../../..` as a source
path, and `absent` was asked of `git ls-files`, which does not list the contents
of the 261 gitlinks nor `build/mac_files`: it reported 32452 live references as
missing where the file system reports 227.
