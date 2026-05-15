# CLAUDE.md

Toujours répondre en français, SVP.
This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Source of the `acme` editor (Plan 9 / plan9port port). C, Plan 9 style, multithreaded via `libthread`. Acme exposes its windows as a 9P filesystem (`cons`, `index`, `log`, `new/`, and per-window `addr`, `body`, `tag`, `ctl`, `data`, `event`, `xdata`, `errors`, `editout`). External programs drive the editor by reading/writing these virtual files. The authoritative reference for that interface is `man/man4/acme.4` (or `9 man 4 acme`).

## Build / run

Requires plan9port (`$PLAN9` set, `mk` and `9c` on PATH).

```
mk            # build acme and mail/
mk install    # install into $PLAN9/bin
mk clean
mk likeplan9  # generate a Plan-9-style view of sources under likeplan9/ (sed rewrites)
mk diffplan9  # diff against ../plan9 (sibling upstream) — useful to see local drift
```

No automated test suite — verification is by running `acme` and exercising the change.

CLI:

```
acme [-a] [-c ncol] [-f varfont] [-F fixfont] [-e emphfont] [-E emphfixfont] [-l loadfile] [-W winsize] [file ...]
```

`-e` / `-E` populate `fontnames[2]` / `fontnames[3]` and `$emphfont`; they supply the per-window emphasis font loaded on demand by the Emph feature.

## References — read the plan9port man pages

**Important.** A great deal of authoritative information about the APIs this code uses lives in the plan9port man pages, **not** in inline comments or external docs. Before guessing semantics or reading source for a third-party library, consult the relevant man page via `9 man <section> <name>`:

- `9 man 4 acme` — acme(4): the per-window 9P filesystem (`man/man4/acme.4` in this tree).
- `9 man 1 acme` — acme(1): user-facing CLI and command reference (`man/man1/acme.1` in this tree).
- `9 man 3 frame` — libframe API (`Frame`, `Frbox`, `frinit`, `frinsert`, `frdelete`, `frdrawsel`, `frredraw`, etc.). Essential when touching `text.c` or planning rendering changes.
- `9 man 3 draw` — libdraw primitives (`Image`, `Rect`, `draw`, `string`, `stringnbg`).
- `9 man 3 thread` — libthread (channels, `threadmain`, `proccreate`).
- `9 man 9 intro` — Plan 9 conventions.
- `9 man 3 regexp` / `9 man 7 regexp` — regex API (the regx.c engine in this tree is a rune-based Plan 9 variant; man pages cover the closely related libregexp).
- `9 man 7 plumb` — plumb message format (consumed by `look.c`, `plumb.c`).
- `9 man 4 9pserve`, `9 man 9 9p` — the 9P transport and protocol that `fsys.c` speaks.
- `9 man 7 font`, `9 man 3 cachechars`, `9 man 3 subfont` — font/glyph machinery underlying `Reffont` and `rfget`.

`NOTES` in this directory has a partial list of relevant pages from the maintainer; the list above supersedes it. When in doubt, `9 man -k <keyword>` searches by topic.

## Architecture in one screen

Entry: `acme.c:threadmain` parses argv, initialises display/fonts, starts the 9P server (`fsys.c`), then runs the main loop dispatching channel events (mouse, keyboard, plumb, wait, command).

Concurrency is CSP-style: each subsystem is a thread, communication is via `Channel`s declared in `dat.h` (`cwait`, `ccommand`, `cplumb`, `cxfidalloc`, …). Per-window mutation is serialised by `Window.lk` (`winlock` / `winunlock`).

Data stack, bottom up:

```
Block (disk.c)   fixed-size rune blocks, disk-paged
  ↑
Buffer (buff.c)  rune sequence with a cached working block
  ↑
File (file.c)    Buffer + delta/epsilon undo buffers + name/mtime/sha1
  ↑
Text (text.c)    File + libframe Frame + Reffont + selection + view geometry
  ↑
Window (wind.c)  tag Text + body Text + 9P state (nopen[QMAX], events, dump info)
  ↑
Column / Row (cols.c, rows.c)  geometry
```

Edits go through `Elog` (`elog.c`) when batched (the `Edit` sam-style command pipeline). Direct interactive edits skip the log.

Two parallel "command" surfaces — keep them distinct when adding features:

1. **User commands** (button-2 click on text like `Put`, `Edit`, `Look`). Dispatch via `execute()` → `exectab[]` in `exec.c`. Each handler has signature `void f(Text *et, Text *t, Text *argt, int flag1, int flag2, Rune *arg, int narg)`. Command names are `Rune*` literals (`L…`) typically declared near the table.
2. **External 9P control** (writes to `<id>/ctl`). Dispatch is a chain of `strncmp` in `xfidctlwrite` (`xfid.c`). Each message is one line: `clean`, `dirty`, `dot=addr`, `addr=dot`, `limit=addr`, `name X`, `font X`, `nomark`, `mark`, `cleartag`, `show`, `get`, `put`, `del`, `delete`, `dump …`, `dumpdir …`. New `ctl` verbs go here and **must also be documented in `man/man4/acme.4`**.

9P plumbing: `fsys.c` defines the `Dirtab` (per-file `Qid` constants `Q…`/`QW…` in the `enum` at top of `dat.h`); each request is wrapped in an `Xfid` and routed to the right `xfid*` handler in `xfid.c`. Read/write of `body`/`tag` go through `xfidutfread` / `xfidwrite`. Random access uses `addr` + `data`/`xdata`.

Regex: `regx.c` implements the engine on runes (`rxcompile`, `rxexecute`, `rxbexecute`); results land in a `Rangeset` (`dat.h`, `NRange=10`). Reuse this rather than `include/regexp9.h` directly — the existing code, including `Edit x///`, `g//`, and `Look`, expects `regx.c` semantics. `addr.c` evaluates sam-style addresses against this engine.

Editing language (`Edit …`): parser in `edit.c` (yacc-ish hand-written), AST in `edit.h` (`Cmd`, `Addr`), per-command handlers in `ecmd.c`, change log in `elog.c`. Adding sam-level commands means touching `cmdtab[]` and writing an `X_cmd(Text*, Cmd*)`.

External library API: `include/acme.h` is consumed by clients like `mail/` and `win` — not by acme itself.

## Conventions worth knowing

- All text inside acme is `Rune` (UTF-32). Convert at the 9P / disk boundary only (`bytetorune`, `runetobyte`, `cvttorunes`).
- Allocations: `emalloc`/`erealloc`/`estrdup`/`runemalloc` (`util.c`) — they abort on failure, so don't add null checks after them.
- Errors inside `Edit` commands use `editerror(...)` (long-jumps out). Elsewhere use `warning(...)` for user-visible messages, `error(...)` for fatal.
- Locking: take `winlock(w, q)` before mutating a window from a thread that isn't the one owning it. `xfid*` handlers do this; user-command handlers usually run on the right thread already.
- Don't break the `Dirtab` ordering or the `Q…`/`QW…` enum — `QID` and `FILE` macros depend on it.
- `mkfile` has a `likeplan9` target that rewrites local sources to upstream style via `sed`. If you rename fields used in those `sed` patterns (`fcall`, `lk`, `b`, `fr`, `ref`, `m`, `u`, `u1`), keep the rewrite working or update the rule.
- This tree diverges from upstream plan9port; commits like `02eb4e73` (fontnames[4]) are local. Use `mk diffplan9` to see drift before sending anything upstream.

## Emphasis feature

The emphasis feature is fully implemented. Summary of all touchpoints:

- **ctl verbs** `emph=regex` / `noemph`: implemented in `xfidctlwrite` (`xfid.c`).
- **User command** `Emph [regex]`: `LEmph` entry in `exectab[]` + `emph` handler (`exec.c`); toggles when called with no argument.
- **Font/Emph coupling**: the emphasis font follows the body font mode (variable/fixed). `emphfontname` (`text.c`) picks `fontnames[2]` or `fontnames[3]`; `setemph` loads the right one, and `fontx` (the `Font` command) calls `emphfontupdate` so a `Font` toggle switches both body and emphasis fonts.
- **Per-window state** in `dat.h` `Window`: `emphon`, `emphpat`/`nemphpat`, `emphmatch`/`nemphmatch`/`aemphmatch`, `emphfont`.
- **Rendering**: extended via `libframe` — `Frbox.font`, macro `FRBOXFONT`, `frsetboxfont`; acme applies it through `emphapply` (`text.c`). libframe now supports per-range fonts natively.
- **Range logic**: `emphranges.c`, tested under `tests/`.
- **Documentation**: update `man/man4/acme.4` (ctl verbs) and `man/man1/acme.1` (command list) when changing this feature.

## Subproject

`mail/` is a separate program (its own `mkfile`) that drives acme through `libacme` (`include/acme.h`). Changes here rarely require touching `mail/`, but if you change the external API or message formats, check `mail/win.c` and `mail/mesg.c`.
