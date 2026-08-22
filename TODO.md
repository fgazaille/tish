# tish TODO

This list is based on a static review of the current native Ndless build. Items
are ordered roughly by risk, not implementation convenience.

## P0: Correctness and Safety

- [x] **Fix prompt buffer overflow** in `src/render.c:92-133`.
  Reserve space for both a cursor marker and a command character before writing
  either one. A long command with the cursor near the end can write past the
  `line[COLS]` stack buffer.
- [x] **Stop modifying string literals** in `src/cmd.cpp:96-110` and
  `src/render.c:40-58`. `ls --help` casts a `const` raw string to `char *` and
  passes it to `strtok()`. Make `print_multiline()` accept `const char *` and
  scan without modifying its input, or copy the text into writable storage.
- [x] **Make path construction length-aware** in `src/fs.c:32-84`. Reject
  overflow instead of truncating paths. In particular, prevent a near-300-byte
  `cwd` from making the relative-path length negative before `memcpy()`.
- [x] **Remove the extra blocking keypress on exit** in `src/tish.cpp:24-30`.
  `wait_key_pressed()` waits for release and then waits for a new key, so Escape
  and `exit` do not return immediately.
- [x] **Reject same-file copies** in `src/cmd.cpp:337-340`. `cp file file`
  opens the destination with `"w"` after opening the source and truncates the
  source before copying.
- [x] **Check file read/write/close results** in `src/cmd.cpp:256-280`,
  `src/cmd.cpp:354-363`, and `src/render.c:15-20`. Report short writes, read
  failures, close failures, and partial copies instead of reporting success.

## P1: Shell Behavior

- [x] **Report `nl_exec()` failures** in `src/cmd.cpp:54-56`. Keep the redraw
  after launch, but show an error when a matching `.tns` file is malformed,
  unsupported, or rejected by the loader.
- [x] **Reject empty pipeline stages** in `src/cmd.cpp:662-673`. Commands such
  as `echo hello |`, `| cat`, and `echo hello || cat` should produce syntax
  errors before executing earlier stages.
- [x] **Validate redirection arity** in `src/cmd.cpp:588-612`. Reject missing
  filenames and extra tokens, for example `echo hi > first extra` and
  `echo hi > first > second`, instead of silently ignoring tokens.
- [x] **Detect pipe overflow** in `src/render.c:23-35`. The 4 KB pipe
  buffer no longer drops output silently: an overflow flag makes the pipeline
  report `pipe: output too large` when it finishes.
- [ ] **Reject or clearly report unsupported native-program piping and
  redirection** in `src/cmd.cpp:525-538` and `src/cmd.cpp:557-650`. A launched
  `.tns` program owns the screen and does not use Tish's output sink; only the
  shell's `running ...` message is redirected or piped.
- [ ] **Decide how child programs inherit the working directory**. Tish tracks
  `cwd` logically in `src/fs.c`, but does not change the OS current directory.
  After `cd /documents/subdir`, a launched program may resolve relative paths
  against a different native directory. Choose and document one approach:
  preserve the limitation, pass the logical directory explicitly, or establish
  the native directory after verifying Ndless `chdir()` behavior.
- [x] **Protect the logical working directory from mutations** in
  `src/cmd.cpp:402-444`. Reject removing/renaming `cwd` or an ancestor, or
  recompute `cwd` after a successful mutation. Currently `pwd` can continue to
  show a directory that no longer exists.

## P1: Input Mapping

- [x] **Separate printable keys from editor events** in `src/input.c`.
  Every printable key owns its character; only editing actions (backspace,
  enter, esc, history and cursor movement) use control codes, with no
  collisions between the two.
- [x] **Correct clickpad operator mappings** in `src/input.c:22-27`.
  Use `KEY_NSPIRE_GTHAN` and `KEY_NSPIRE_BAR` for the physical `>` and `|`
  keys. `KEY_NSPIRE_MULTIPLY` and `KEY_NSPIRE_EE` now map to `*` and `&`.
- [x] Add tests or a diagnostics mode that prints the raw logical event for
  every supported keypad and touchpad key.

## P1: Build and Structure

- [x] **Track text-included source/header dependencies** in `Makefile`.
  `src/tish.cpp` includes `fs.c`, `render.c`, `cmd.cpp`, and `input.c`, but the
  Makefile only tracks `src/tish.c` for `src/tish.o`. Add `-MMD -MP` dependency
  generation or explicit prerequisites so editing an included file rebuilds
  the binary.
- [x] **Choose an explicit C/C++ structure**. The current single translation
  unit is compiled through `nspire-g++`, even though the entry file is named
  `tish.c`. If the single-TU design remains, rename it to `tish.cpp` and make
  that intent clear. Otherwise split modules into separate objects with
  deliberate `extern` and `extern "C"` boundaries.
- [ ] Move application-wide state out of `static` definitions in
  `include/tish.h` before introducing separate translation units. Use one
  definition in a source file and `extern` declarations in the header.
- [x] Handle command-token allocation failure in `src/cmd.cpp:560-583`, or
  remove the heap allocation entirely.

## P2: Performance and Memory

- [x] **Avoid unconditional full VRAM commits** in `src/render.c:69-85`.
  Track whether any cell changed and skip `nio_vram_draw()` when there is no
  visual update.
- [ ] Track dirty rows or regions. `build_screen()` reconstructs all scrollback
  rows on every input event even when only the prompt row changed.
- [ ] Replace the per-command allocation of 64 separate 64-byte strings in
  `src/cmd.cpp:560-583` with one bounded token buffer plus an array of pointers.
- [ ] Increase the file-copy buffer from 256 bytes to a modest 1-4 KiB buffer,
  subject to stack and heap measurements on the calculator.
- [x] Avoid scanning `docs` twice in `find_program()` when `cwd` already equals
  `docs`.
- [ ] Keep using `idle()` in polling loops. Do not replace it with a busy loop;
  the current Ndless wait strategy is appropriate for power usage.

## P2: Reliability and Diagnostics

- [ ] Add an explicit command-status path so builtin failures and pipeline
  failures propagate instead of being represented only by printed text.
- [ ] Add an `errno`/native-error diagnostic helper for `nuc_*` operations,
  without assuming that every direct Nucleus syscall updates newlib `errno`.
  (`native_file_error()` in `src/cmd.cpp:6-10` polls the OS `ferror` syscall;
  wired into cat/cp and the file sink. Legacy mkdir/rm/rmdir/mv stay opaque.)
- [ ] Add a shell diagnostic command showing the logical path, documents root,
  Ndless revision, OS ID, hardware type/subtype, and LCD type.
- [ ] Verify Nspire I/O console ownership across `nl_exec()`. `render()` assumes
  `nio_get_default()` remains valid after a child program returns. Confirm the
  per-image behavior on the exact Ndless/Nspire I/O build, and add a controlled
  failure path if the default console is unavailable.
- [ ] Add a test fixture or emulator workflow for long paths, empty pipelines,
  full pipe buffers, full storage, malformed `.tns` files, and all operator
  keys.

## Validation checklist

- [x] Build from a clean checkout with the Ndless cross-toolchain.
- [x] Run `nspire-g++ -Wall -Wextra -marm -Iinclude -fsyntax-only src/tish.cpp`.
- [x] Test the recent fixes on a physical CX II calculator (cat picker grid +
      ragged-row navigation, redirect syntax errors, pipe empty-stage
      rejection, overflow report, hist output, rmdir/mv cwd recompute,
      launches, render sanity — 2026-08-21 session; Firebird untested).
- [ ] Test clickpad and touchpad input mappings separately.
- [x] Test launch success, launch failure, screen restoration, and child path
  behavior (`./prog` run/return/redraw and not-found verified on device;
  nested-relative child cwd remains untested by design, see the inheritance
  decision above).
- [x] Run `genzehn --info` on the generated Zehn output.
- [x] Verify that editing every text-included source and header causes a rebuild
  (`touch include/tish.h && make` recompiles; second `make` is a no-op).
- [ ] Measure render time, VRAM commits, heap use, and maximum stack use before
  and after optimization changes.
