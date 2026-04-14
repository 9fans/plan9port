# Acme Editor — Code Analysis

## Overview

~15,000 lines of C across 32 files. A multi-threaded, event-driven text editor
exposing a 9P filesystem interface. Located in `src/cmd/acme/`.

## Architecture

```
Row (top-level)
 └── Column[] (horizontal divisions)
      └── Window[] (each with tag + body)
           └── Text → File → Buffer → Block (disk-backed)
```

6 threads run concurrently: mouse, keyboard, wait (process reaping), xfid
allocator, new-window creator, and the 9P filesystem server.

## Key Components

| Subsystem | Files | Lines | Role |
|-----------|-------|-------|------|
| Command execution | exec.c, ecmd.c, look.c | ~3,700 | Mouse click dispatch, command table, pipe ops |
| Text/Display | text.c, wind.c, cols.c, rows.c | ~2,400 | Editing, rendering, layout |
| 9P filesystem | fsys.c, xfid.c | ~1,900 | External program interface via synthetic filesystem |
| Edit language | edit.c, addr.c, regx.c | ~2,100 | ed(1)-style command parser, regex engine |
| Buffer/File | file.c, buff.c, disk.c, elog.c | ~800 | Disk-backed buffers, undo/redo via delta/epsilon |
| Init/Utilities | acme.c, util.c, dat.c, scrl.c | ~800 | Startup, globals, scrollbar |

## Core Data Structures (dat.h)

```
Row
├── Column[] (ncol)
    ├── Window[] (nw)
    │   ├── Text tag          // Command/filename line
    │   ├── Text body         // File content
    │   ├── File*             // Associated file (shared across views)
    │   └── Xfid* eventx     // Current event reader
    └── Text tag              // Column-level commands

File
├── Buffer b                  // Main content (disk-backed)
├── Buffer delta              // Undo transcript
├── Buffer epsilon            // Redo transcript (inverse of delta)
├── Buffer* elogbuf           // Pending editor changes
├── Text** text (ntext)       // All Text objects showing this file
└── seq                       // Undo sequence number

Text
├── File* file
├── Frame fr                  // Text rendering frame (libframe)
├── uint org, q0, q1          // Scroll position and selection
└── Rectangle all             // Display area

Buffer
├── uint nc                   // Total character count
├── Rune* c                   // In-memory cache
└── Block** bl (nbl)          // Disk blocks for overflow

Window
├── QLock lk                  // Concurrency control
├── Text tag, body
├── int nopen[QMAX]           // Count of open fids per file type
├── Xfid* eventx              // Current 9P event reader
└── char* events              // Event buffer
```

## 9P Filesystem Interface

The editor exposes itself as a synthetic filesystem:

```
/acme/
├── cons, consctl             // Console
├── index                     // List of windows
├── new/                      // Create new window
├── log                       // Event log
├── label                     // Window labels
└── editout                   // Edit command output

/acme/<winid>/
├── addr              // Current address
├── body              // File content
├── tag               // Window tag line
├── ctl               // Control commands
├── data              // Data access
├── event             // Event stream for external programs
├── errors            // Error messages
└── xdata             // Extended data access
```

External programs connect via 9P. `fsysproc()` reads messages, dispatches to
`fcall[]` handlers, which route work to Xfid threads via channels. Responses
flow back through the pipe.

## Command Dispatch

Three levels:

1. **Mouse clicks (exec.c)** — Maps clicks to `Exectab` entries (Cut, Paste,
   Get, Put, Look, etc.). Middle-click executes, right-click plumbs/searches.

2. **Edit commands (edit.c, ecmd.c)** — ed(1)-style syntax: `[addr] cmd [args]`.
   Commands: a, c, d, e, f, g, i, s, x, etc.

3. **External programs (exec.c)** — Pipe operations (`|cmd`, `<cmd`, `>cmd`).
   Separate process with stdin/stdout redirected to window content.

## Design Strengths

- **Delta/epsilon undo** — Dual-buffer approach supporting unlimited undo/redo
  without replaying operations.
- **Disk-backed buffers** — Transparent paging for arbitrarily large files.
- **Multi-view consistency** — One File shared by multiple Text/Window views,
  all stay in sync automatically.
- **9P as API** — External programs interact via filesystem reads/writes.
- **Channel-based threading** — CSP-style communication via libthread channels.

## Potential Concerns

1. **Global state without locks** — `seltext`, `argtext`, `mousetext`,
   `typetext` accessed across threads without mutexes. Code comment
   acknowledges: "global because cleanup needs them."

2. **Fixed hash table** — `fids[Nhash]` with `Nhash=16`, linear chaining,
   no resize under load.

3. **Hard-coded limits** — `NPROG=1024` (regex program), `NRange=10`
   (captures), `Infinity=0x7FFFFFFF`.

4. **Partial rune handling** — UTF-8/rune conversions scattered throughout,
   partial rune buffering in `xfidwrite`.

5. **Error path cleanup** — Some paths may skip `fbuffree()` or other
   resource deallocation.

## Complexity Hotspots

- **exec.c** (1,817 lines) — Largest file. All mouse-click command dispatch
  including pipe operations.
- **text.c** (1,664 lines) — Insertion cache coherency with File buffers
  is subtle.
- **ecmd.c** (1,396 lines) — Deeply nested edit command loops
  (`looper` → `filelooper` → `linelooper`).
- **xfid.c** (1,147 lines) — 9P fid handlers with tricky concurrency
  (one event reader per window).

## Source File Index

| File | Lines | Purpose |
|------|-------|---------|
| acme.c | 1,161 | Main entry, thread creation, channel setup |
| addr.c | 297 | Address/range parsing for edit commands |
| buff.c | 325 | Buffer management, block allocation, caching |
| cols.c | 591 | Column management, window layout within columns |
| dat.c | 62 | Global variable definitions |
| dat.h | 582 | All major type definitions and enums |
| disk.c | 133 | Disk-backed storage for large buffers |
| ecmd.c | 1,396 | Edit command implementations |
| edit.c | 686 | Edit command parser and execution |
| edit.h | 99 | Edit command structures |
| elog.c | 354 | Edit log for pending changes |
| exec.c | 1,817 | Command execution and mouse event dispatch |
| file.c | 311 | File abstraction, undo/redo via delta/epsilon |
| fns.h | 109 | Function prototypes |
| fsys.c | 749 | 9P server implementation |
| logf.c | 199 | Window operation logging |
| look.c | 942 | File/command lookup, plumbing, search |
| regx.c | 843 | Regular expression engine (NFA-based) |
| rows.c | 830 | Row management, multi-column layout |
| scrl.c | 159 | Scrollbar rendering and interaction |
| text.c | 1,664 | Core text editing, rendering, selection |
| time.c | 124 | Timer management for UI events |
| util.c | 497 | Utility functions, UTF conversion |
| wind.c | 726 | Window creation, deletion, resizing |
| xfid.c | 1,147 | 9P filesystem read/write/open/close handlers |
