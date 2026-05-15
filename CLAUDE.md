# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

*Plan 9 from User Space* (plan9port): a Unix port of the Plan 9 libraries
and programs. Not a single application — a full suite (editors, shell,
troff document toolchain, graphics, networking, crypto, storage). Almost
all code is C in Plan 9 style.

For the big picture, read **`./codebase_analysis.md`** (whole-tree
overview: directory roles, the 32 libraries, the 9P backbone, the build
chain). For the `acme` editor specifically, read
**`src/cmd/acme/codebase_analysis.md`** and **`src/cmd/acme/CLAUDE.md`**.
This tree diverges from upstream — **`CODE_CHANGES.md`** documents the
locally-added emphasis feature in acme (a per-box-font extension of
`libframe`).

## Build / run

This project builds with **`mk`** (the Plan 9 build tool), **not** `make`.
The root `Makefile` is a decoy that just prints "read the README".

First-time bootstrap (builds `mk` itself, then everything):

```
./INSTALL          # build + install into bin/ and lib/
./INSTALL -b       # build only
./INSTALL -c       # install only (assumes already built)
```

`INSTALL` sets `$PLAN9` to the tree root and writes `config`. After that,
`$PLAN9` must stay set and `$PLAN9/bin` must be on `$PATH` for everything
to work.

Incremental work in any directory with a `mkfile`:

```
mk                 # build the target(s) in this directory
mk install         # install built binaries into $PLAN9/bin
mk clean           # remove object files
mk nuke            # remove objects + installed binaries
```

Compilation goes through wrappers in `bin/`: **`9c`** (compile), **`9l`**
(link), **`9ar`** (archive), and **`9 <cmd>`** (run a command in the Plan 9
environment). `mkfile`s include shared fragments from `src/` (`mkhdr`,
`mkone`, `mkmany`, `mkdirs`). Object files use the `.o` / `o.` prefix
convention, not the usual Unix names.

## Tests

There is **no global automated test suite**. Verification is mostly by
running the program. Exceptions worth knowing:

- A few libraries ship ad-hoc `test*.c` programs (e.g. `src/libsec/port/`,
  `src/lib9/`, `src/libregexp/`) — build and run them by hand.
- `src/cmd/acme/` has a real test suite added for the emphasis feature:
  `cd src/cmd/acme && mk test`, or run a single runner directly, e.g.
  `cd src/cmd/acme/tests && mk && ./o.emph_test`.

## Man pages are the authoritative API reference

**Important.** The authoritative documentation for the APIs in this tree
lives in the plan9port man pages, not in inline comments. Before guessing
the semantics of a library function or a tool, consult it:

```
9 man <section> <name>      # e.g. 9 man 3 draw, 9 man 9 9p
9 man -k <keyword>          # search by topic
```

Sections: 1 (commands), 3 (C library APIs), 4 (file-server interfaces,
incl. 9P services), 7 (file formats / conventions), 8 (admin), 9 (Plan 9
conventions and the 9P protocol). Man pages render after a successful
install; sources are under `man/`.

## Architecture essentials

- **`lib9` is the portability layer.** It reimplements the Plan 9 C
  library (`u.h` / `libc.h`) on top of POSIX, so the rest of the Plan 9
  code compiles nearly unchanged. Host-specific differences (macOS vs
  Linux/BSD) are confined to `lib9`, `devdraw`, and the bootstrap mkfiles
  in `unix/`. Touch these when porting; avoid host `#ifdef`s elsewhere.
- **9P is the backbone.** Services expose themselves as file trees served
  over the 9P protocol; clients read/write virtual files instead of
  calling APIs. `9pserve` multiplexes services, `9pfuse` mounts them.
  acme, plumber, `venti`, `fossil`, `upas/fs` are all 9P servers.
- **Graphics path:** programs use `libdraw`, which talks to the separate
  `devdraw` process (it isolates X11 / Quartz). `libframe` renders text in
  "boxes" for acme / sam / 9term.
- **Plumbing:** `plumber` routes typed messages between applications per
  the rules in `plumb/` — the "click to open" mechanism.
- **Source layout:** `src/lib*/` are the 32 libraries; `src/cmd/` holds
  commands (simple ones as a single `.c` file, complex ones in their own
  subdirectory with a `mkfile`). Public headers are in `include/`.

## Conventions worth knowing

- All text is UTF-8 / runes (Plan 9 originated UTF-8). Convert at I/O
  boundaries only.
- Code is Plan 9 style: short functions, terse identifiers, few comments.
  Match the surrounding style of whatever file you edit.
- This tree is not vanilla plan9port. Before sending anything upstream,
  check the local drift (some directories provide an `mk diffplan9`
  target; `acme` does).
- Toujours répondre en français, SVP.

