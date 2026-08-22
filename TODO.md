# tish TODO

This list is based on a static review of the current native Ndless build. Items
are ordered roughly by risk, not implementation convenience.

## P0.2: Correctness and Safety — Rendering and Display (New Critical)

- [ ] **Add NULL check for `nio_get_default()` in `render()`** in `src/render.c:80-96`. `render()` dereferences `nio_console *csl = nio_get_default()` via `nio_csl_savechar`/`nio_vram_csl_drawchar` without checking for NULL. After `nl_exec()` the console can be unavailable; must return early or reinitialize instead of crashing. Related to open P2.2 console-ownership item.
- [ ] **Harden `build_prompt()` against small `out_size`** in `src/render.c:106-111`. `snprintf(out, out_size, "user@tinspire:%.*s$ ", out_size-17, cwd)` passes a negative precision when `out_size<17`, which C99 treats as “precision omitted” and prints the whole `cwd`, defeating the truncation guarantee. Guard with `if (out_size<17) { snprintf(out,out_size,"$ "); return out; }`.
- [ ] **Handle `SINK_FILE` with `out_file==NULL`** in `src/render.c:21-30`. `print_line()` checks `sink==SINK_FILE && out_file` then falls through to `SINK_PIPE`/scrollback when `out_file` is NULL, silently showing redirected output on screen. Should set `io_error=1` and return instead of falling through.
- [ ] **Fix silent prompt truncation** in `src/render.c:126`. `build_screen()` copies at most `COLS-4` chars of the prompt (`for (i=0; prompt[i] && x<COLS-4; i++)`) without indicating truncation. A long `cwd` (up to 299 chars) is silently clipped with no `…` marker; document or indicate.
- [ ] **Fix `set_row()` missing bounds check** in `src/render.c:72-78`. `set_row(y,s)` and `draw_cat_menu()` call it with `y = 28-CAT_ROWS+row` without asserting `0<=y<ROWS`. OOB write would corrupt `scr`/`prev`.

## P1.1: Shell Behavior

- [ ] **Reject or clearly report unsupported native-program piping and redirection** in `src/cmd.cpp:525-538` and `src/cmd.cpp:557-650`. A launched `.tns` program owns the screen and does not use Tish's output sink; only the shell's `running ...` message is redirected or piped.
- [ ] **Decide how child programs inherit the working directory**. Tish tracks `cwd` logically in `src/fs.c`, but does not change the OS current directory. After `cd /documents/subdir`, a launched program may resolve relative paths against a different native directory. Choose and document one approach: preserve the limitation, pass the logical directory explicitly, or establish the native directory after verifying Ndless `chdir()` behavior.
- [ ] **Reject native-program launch when a pipe or redirection is active** in `src/cmd.cpp:63-75` and `src/cmd.cpp:635-803`. `launch()` prints `running <name>` via `print_line()` (which obeys `sink`) and calls `nl_exec()` even when `sink==SINK_PIPE` or `SINK_FILE`. `echo hi | ./hello` should error (“cannot pipe native program”) instead of silently discarding UI and returning success. Also `launch()` only checks `result==0xDEAD`; other failure codes are treated as success — check any non-zero/error return.
- [ ] **Report silent pipeline-segment truncation** in `src/cmd.cpp:736-803` / `src/cmd.cpp:635-637`. `run_command()` uses `char seg[COLS]` (53) and `if (len>sizeof(seg)-1) len=sizeof(seg)-1; memcpy(seg,p,len)` truncates a segment >52 chars without error (e.g., long `echo` arguments). Should reject with `command too long` / `argument list too long`.
- [ ] **Propagate pipe overflow as failure** in `src/render.c:32-40` and `src/cmd.cpp:789-795`. `print_line()` sets `pipe_overflow=1` and drops the line, but `run_command()` prints `pipe: output too large` and returns `err` from the last stage (usually `STATUS_SUCCESS`). Should set `err=STATUS_FAIL` when overflow occurred.
- [ ] **Fix off-by-one pipe capacity** in `src/render.c:34`. `if (pipe_out_len + l + 1 < PIPE_CAP)` wastes one byte (max 4095 not 4096). Should be `<= PIPE_CAP` / `< PIPE_CAP+1` or `pipe_out_len + l + 1 <= PIPE_CAP`.
- [ ] **Validate `cmd_pwd` arity** in `src/cmd.cpp:77-81`. `pwd extra` currently prints `cwd` and returns success; should reject extra arguments with `STATUS_MISUSE`.
- [ ] **Reject excess arguments beyond 64** in `src/cmd.cpp:635-672`. `run_segment()` tokenizes with `while(token && argc<64)` and silently ignores tokens 65+. Should error `too many arguments`.
- [ ] **Detect extra redirection operator as syntax error more precisely** in `src/cmd.cpp:675-696`. `echo hi > first > second` is reported as “extra redirect argument” — acceptable but should be “ambiguous redirect”. More importantly, a second `>` beyond `redir_i+2` is only caught after first `>` is found; ensure `>>` vs `>` both handled.
- [ ] **Return failure for truncated `find_program` path** in `src/cmd.cpp:33-60`. `snprintf(path_buf, buf_size, "%s/%s", roots[r], want)` can truncate when `buf_size` is small; must check return `>=buf_size` and continue/return 0 instead of returning a truncated path to `nl_exec()`.
- [ ] **Fix `cmd_ls` silent truncation and missing `nuc_stat` error handling** in `src/cmd.cpp:112-167`. `snprintf(f,600,"%s/%s",dir,ep->d_name)` truncates long names silently then `nuc_stat(f)` either fails or stats the wrong file, causing missing `/` marker. `snprintf(line,COLS,"%s%s",ep->d_name,mark)` also truncates long filenames silently. Check `snprintf` return and skip/mark with `?`.
- [ ] **Remove debug leak in `cmd_hist`** in `src/cmd.cpp:169-180`. `hist` prints `len=%d browse=%d` exposing internal cursor; should be removed or behind a debug flag.
- [ ] **Fix `is_ancestor_of`/`cmd_rmdir`/`cmd_mv` root edge cases and truncation** in `src/fs.c:105-112`, `src/cmd.cpp:466-513`. `cmd_mv` does `snprintf(new_cwd,"%s%s",dst,cwd+strlen(src))` which drops the separator when `src=="/"`: `src="/", dst="/a", cwd="/b"` → `"/ab"` not `"/a/b"`. Must insert `/` when `src` is root. Both `cmd_mv` and `cmd_rmdir` must check `snprintf` truncation of `new_cwd`/`cwd` and abort instead of storing a 299-char truncated invalid path. `is_ancestor_of` assumes normalized inputs; document or assert.
- [ ] **Unify `\r` handling between `cat_file` and `cat_pipe`** in `src/cmd.cpp:245-317`. `cat_file` skips `\r` for CRLF, `cat_pipe` and `cat_file` line-wrap logic differ, and `print_multiline` also skips `\r` — pipe data with `\r` will render as garbage.

## P1.2: Input Handling and Key Mapping

- [ ] **Fix `insert_char` limit inconsistency** in `src/input.c:158-167`. `insert_char` rejects when `cmdlen>=COLS-3` (50), but history restore via `snprintf(cmdline,COLS,"%s",hist[browse])`/`cmdlen=strlen(cmdline)` can hold 52 chars, and `hist` stores 52-char entries. After browsing a 52-char history entry, 2 chars cannot be inserted/deleted consistently. Choose one limit (`COLS-1` or `COLS-3` with documented reservation) and enforce everywhere, or warn on truncation.
- [ ] **Fix backspace stale byte** in `src/input.c:174-180`. `memmove(&cmdline[cursor-1], &cmdline[cursor], cmdlen-cursor)` does not clear the stale byte at `cmdline[cmdlen-1]` and does not NUL-terminate. When `cursor==cmdlen` it moves 0 bytes. Should `cmdline[--cmdlen]='\0'` and ensure NUL.
- [ ] **Fix `cat_row_len` negative return and `cat_move` OOB** in `src/input.c:109-124`. `cat_row_len(row)` returns `CAT_N - row*CAT_PER_ROW` clamped only on upper side, so `row>=3` returns negative (-13). `cat_move` then does `if(col>=len) col=len-1` where `len==0` → `col=-1` → `return nrow*CAT_PER_ROW-1` OOB read of `cat_chars`. Not reachable with `CAT_N=32` today, but breaks if `CAT_N` changes to a multiple of 15; clamp `if(len<0) return 0; if(len>CAT_PER_ROW) return CAT_PER_ROW;` and guard `len==0`.
- [ ] **Fix `handleinput` ENTER display truncation and history `memmove` pedantry** in `src/input.c:204-226`. `char buf[COLS]` holds `prompt` (up to 52) + `cmdline` (up to 52) truncated to `COLS-1` without warning (`buf` only 53). `memmove(&hist[1][0], &hist[0][0], 15*COLS)` relies on 2D array contiguity (works but pedantic); prefer `memmove(hist[1], hist[0], 15*COLS)` and clear stale `hist` slot if needed. Also `snprintf(hist[0],COLS,...)` vs `insert_char` limit mismatch noted above.
- [ ] **Debounce/polling for CAT picker** in `src/input.c:127-155`. `cat_menu` polls `isKeyPressed` at `idle()` speed with no repeat delay for arrows, so holding an arrow scrolls extremely fast. Add a repeat delay or use `wait_no_key_pressed()` consistently. Also ensure `CAT_N` ragged last row navigation (`cat_move`) keeps column anchoring correct.

## P1.3: Build and Structure

- [ ] Move application-wide state out of `static` definitions in `include/tish.h` before introducing separate translation units. Use one definition in a source file and `extern` declarations in the header.
- [ ] **Make `include/tish.h` self-contained** in `include/tish.h:81`. `bool valid_path(...)` uses `bool` without `#include <stdbool.h>`; relies on transitive `os.h`. Add explicit include. Also `CAT_N` macro `(sizeof(cat_chars)-1)` expands where `cat_chars` is not in scope (defined only in `src/input.c`); move `CAT_N` or `cat_chars` declaration to the header or co-locate.
- [ ] **Fix Makefile pattern rules compiling C as C++** in `Makefile:28-31`. Both `%.o: %.c` and `%.o: %.cpp` invoke `$(GXX)`. When the single-TU include model is split, `.c` files will be compiled as C++ with wrong flags. Use `$(GCC)` for `.c`, `$(GXX)` for `.cpp`, or document single-TU intent.
- [ ] **Tighten `find_program` length check** in `src/cmd.cpp:41`. `if(strlen(name)+sizeof(".tns")>sizeof(want))` is arithmetically correct (`sizeof ".tns"` is 5 inc. NUL) but brittle; prefer `strlen(name)+4+1>sizeof(want)` / `strlen(name)+strlen(".tns")+1`.

## P1.4: Filesystem and Path Handling

- [ ] **Reject or canonicalize empty `normalize_path` input** in `src/fs.c:15-29`. `normalize_path("",dst)` copies `""` and returns 1; callers (`init_fs`) could propagate an empty logical path. Should normalize `""` to `"/"` or return 0.
- [ ] **Make `resolve_path` reentrant** in `src/fs.c:40-92`. Uses `strtok(tmp,"/")` which is non-reentrant; `run_segment` also uses `strtok` on `buf`. Currently sequential, but any future nested call corrupts state. Use `strtok_r` and length-checked copies (`strncpy`/`memcpy` not `strcpy`).
- [ ] **Clear `out` on `resolve_path` failure** in `src/fs.c:83-92`. On `out_size` overflow `resolve_path` returns 0 but leaves `out` partially written (truncated prefix). Callers like `cmd_mv` reuse buffers; zero or restore `out` on failure.
- [ ] **Check `snprintf` truncation in `init_fs` and elsewhere** in `src/fs.c:98-103` and `src/cmd.cpp:492-511`. `snprintf(cwd,sizeof(cwd),"%s",docs)` and `snprintf(new_cwd,...,"%s%s",dst, suffix)` silently truncate long paths (>299) leaving an invalid `cwd` without error. Check return `>=sizeof(cwd)` and report `path too long`.
- [ ] **Audit `valid_path` vs `nuc_opendir` error semantics** in `src/fs.c:114-121`. `valid_path`/`cmd_cd` Probe use `nuc_opendir` to test existence, but failure reason (not-found vs not-a-directory vs permission) is collapsed to generic `no such directory`. Consider `nuc_stat` where available.

## P2.1: Performance and Memory

- [ ] Track dirty rows or regions. `build_screen()` reconstructs all scrollback rows on every input event even when only the prompt row changed.
- [ ] Replace the per-command allocation of 64 separate 64-byte strings in `src/cmd.cpp:560-583` with one bounded token buffer plus an array of pointers.
- [ ] Increase the file-copy buffer from 256 bytes to a modest 1-4 KiB buffer, subject to stack and heap measurements on the calculator.
- [ ] Keep using `idle()` in polling loops. Do not replace it with a busy loop; the current Ndless wait strategy is appropriate for power usage.
- [ ] **Fix inconsistent file-copy chunk sizes** in `src/cmd.cpp:373-421`. `cat_file` uses `chunk[257]` reading 256 bytes, `cmd_cp` uses `chunk[257]` reading 257 bytes. Unify to 256 or measured optimum and document stack use.
- [ ] **Avoid per-command heap fragmentation** in `src/cmd.cpp:635-658`. `run_segment` does 64× `new (std::nothrow) char[64]` per command; on calculator heap fragmentation this can fail. Prefer a single stack token buffer `char buf[COLS]; char *argv[64];` plus `strtok_r` as noted in P2.1 token-buffer item, or at least free on every error path (current cleanup is correct but still fragments).

## P2.2: Reliability and Diagnostics

- [ ] Add opt-in pipefail flag in `include/tish.h:56`: should be available to user as set -o pipefail.
- [ ] Add a shell diagnostic command showing the logical path, documents root, Ndless revision, OS ID, hardware type/subtype, and LCD type.
- [ ] Verify Nspire I/O console ownership across `nl_exec()`. `render()` assumes `nio_get_default()` remains valid after a child program returns. Confirm the per-image behavior on the exact Ndless/Nspire I/O build, and add a controlled failure path if the default console is unavailable.
- [ ] Add a test fixture or emulator workflow for long paths, empty pipelines, full pipe buffers, full storage, malformed `.tns` files, and all operator keys.
- [ ] **Propagate file I/O errors promptly** in `src/cmd.cpp:270-421` and `src/render.c:24-29`. `native_file_error()` via `syscall<e_ferror>` is only checked after `nuc_fclose`; short reads inside the loop are not detected until EOF. `cmd_cp` write failure path does `nuc_fclose(out); nuc_fclose(in); return` without checking `nuc_fclose` return after a prior error — set `io_error` consistently and let `run_command` report `tish: file I/O error` then return `STATUS_FAIL` (currently `run_command` does so but `pipe_overflow` path does not).
- [ ] **Only last pipeline status is kept** in `src/cmd.cpp:736-803` / `include/tish.h:59-66`. `run_command` overwrites `err` per stage so `false | true` returns success. Either document “only last status” (as Completed notes) or implement `pipefail` that returns first failure when enabled. Also ensure `$?` (`cmd_echo` handling of `$?` in `src/cmd.cpp:219-243`) reflects that choice.
- [ ] **Improve `nuc_*` error diagnostics** in `src/cmd.cpp:6-19` and `src/cmd.cpp:425-513`. `native_file_error()` polls `ferror` only if `nl_hassyscall(ferror)`; legacy `mkdir`/`rmdir`/`rm`/`mv`/`unlink` still print opaque `failed` without `errno`/syscall reason. Wire `native_file_error` or `errno` where possible.
- [ ] **Fix `cmd_echo` buffer accounting** in `src/cmd.cpp:219-243`. `char buf[10]` with `snprintf(buf,9,"%i",last_status)` leaves one byte unused (should be `sizeof(buf)`), and `if(l+al+2>sizeof(line)) break` miscounts the leading space. Not user-visible today (`last_status` <1000) but brittle.

## Validation checklist

- [ ] Test clickpad and touchpad input mappings separately.
- [ ] Measure render time, VRAM commits, heap use, and maximum stack use before and after optimization changes.
- [ ] Test cursor movement at edges: `cursor=0` with `cmdlen=52`, `cursor=cmdlen` at end, and left/right through long prompt truncation in `src/render.c:129-139`.
- [ ] Test `render()` after launching and after a failed `nl_exec()` on both emulator and hardware to verify NULL console path in `src/render.c:80-96`.
- [ ] Test pipelines longer than `PIPE_CAP` (4096), exactly 4095/4096 bytes, and empty stages with spaces (`echo hi |   `) in `src/cmd.cpp:736-803`.
- [ ] Test `find_program`/`resolve_path` with `cwd==docs`, root `/`, 299-char paths, empty `""` path, and segment >52 chars truncation.
- [ ] Test `mv`/`rmdir` when `cwd` is the target or a descendant, especially `src="/"` → `dst="/a"` root-suffix case in `src/cmd.cpp:492-513`.
- [ ] Test `cat` CRLF vs LF and `cat` on pipe with `\r` in `src/cmd.cpp:245-317`.

## Completed

### P0.1: Correctness and Safety

- [x] **Fix prompt buffer overflow** in `src/render.c:92-133`. Reserve space for both a cursor marker and a command character before writing either one. A long command with the cursor near the end can write past the `line[COLS]` stack buffer.
- [x] **Stop modifying string literals** in `src/cmd.cpp:96-110` and `src/render.c:40-58`. `ls --help` casts a `const` raw string to `char *` and passes it to `strtok()`. Make `print_multiline()` accept `const char *` and scan without modifying its input, or copy the text into writable storage.
- [x] **Make path construction length-aware** in `src/fs.c:32-84`. Reject overflow instead of truncating paths. In particular, prevent a near-300-byte `cwd` from making the relative-path length negative before `memcpy()`.
- [x] **Remove the extra blocking keypress on exit** in `src/tish.cpp:24-30`. `wait_key_pressed()` waits for release and then waits for a new key, so Escape and `exit` do not return immediately.
- [x] **Reject same-file copies** in `src/cmd.cpp:337-340`. `cp file file` opens the destination with `"w"` after opening the source and truncates the source before copying.
- [x] **Check file read/write/close results** in `src/cmd.cpp:256-280`, `src/cmd.cpp:354-363`, and `src/render.c:15-20`. Report short writes, read failures, close failures, and partial copies instead of reporting success.

### P1.1: Shell Behavior

- [x] **Report `nl_exec()` failures** in `src/cmd.cpp:54-56`. Keep the redraw after launch, but show an error when a matching `.tns` file is malformed, unsupported, or rejected by the loader.
- [x] **Reject empty pipeline stages** in `src/cmd.cpp:662-673`. Commands such as `echo hello |`, `| cat`, and `echo hello || cat` should produce syntax errors before executing earlier stages.
- [x] **Validate redirection arity** in `src/cmd.cpp:588-612`. Reject missing filenames and extra tokens, for example `echo hi > first extra` and `echo hi > first > second`, instead of silently ignoring tokens.
- [x] **Detect pipe overflow** in `src/render.c:23-35`. The 4 KB pipe buffer no longer drops output silently: an overflow flag makes the pipeline report `pipe: output too large` when it finishes.
- [x] **Protect the logical working directory from mutations** in `src/cmd.cpp:402-444`. Reject removing/renaming `cwd` or an ancestor, or recompute `cwd` after a successful mutation. Currently `pwd` can continue to show a directory that no longer exists.

### P1.2: Input Mapping

- [x] **Separate printable keys from editor events** in `src/input.c`. Every printable key owns its character; only editing actions (backspace, enter, esc, history and cursor movement) use control codes, with no collisions between the two.
- [x] **Correct clickpad operator mappings** in `src/input.c:22-27`. Use `KEY_NSPIRE_GTHAN` and `KEY_NSPIRE_BAR` for the physical `>` and `|` keys. `KEY_NSPIRE_MULTIPLY` and `KEY_NSPIRE_EE` now map to `*` and `&`.
- [x] Add tests or a diagnostics mode that prints the raw logical event for every supported keypad and touchpad key.

### P1.3: Build and Structure

- [x] **Track text-included source/header dependencies** in `Makefile`. `src/tish.cpp` includes `fs.c`, `render.c`, `cmd.cpp`, and `input.c`, but the Makefile only tracks `src/tish.c` for `src/tish.o`. Add `-MMD -MP` dependency generation or explicit prerequisites so editing an included file rebuilds the binary.
- [x] **Choose an explicit C/C++ structure**. The current single translation unit is compiled through `nspire-g++`, even though the entry file is named `tish.c`. If the single-TU design remains, rename it to `tish.cpp` and make that intent clear. Otherwise split modules into separate objects with deliberate `extern` and `extern "C"` boundaries.
- [x] Handle command-token allocation failure in `src/cmd.cpp:560-583`, or remove the heap allocation entirely.

### P2.1: Performance and Memory

- [x] **Avoid unconditional full VRAM commits** in `src/render.c:69-85`. Track whether any cell changed and skip `nio_vram_draw()` when there is no visual update.
- [x] Avoid scanning `docs` twice in `find_program()` when `cwd` already equals `docs`.

### P2.2: Reliability and Diagnostics

- [x] Add an explicit command-status path so builtin failures and pipeline failures propagate instead of being represented only by printed text. NOTE: for pipelines, only last status is recorded.
- [x] Add an `errno`/native-error diagnostic helper for `nuc_*` operations, without assuming that every direct Nucleus syscall updates newlib `errno`. (`native_file_error()` in `src/cmd.cpp:6-10` polls the OS `ferror` syscall; wired into cat/cp and the file sink. Legacy mkdir/rm/rmdir/mv stay opaque.)

### Validation checklist

- [x] Build from a clean checkout with the Ndless cross-toolchain.
- [x] Run `nspire-g++ -Wall -Wextra -marm -Iinclude -fsyntax-only src/tish.cpp`.
- [x] Test the recent fixes on a physical CX II calculator (cat picker grid + ragged-row navigation, redirect syntax errors, pipe empty-stage rejection, overflow report, hist output, rmdir/mv cwd recompute, launches, render sanity — 2026-08-21 session; Firebird untested).
- [x] Test launch success, launch failure, screen restoration, and child path behavior (`./prog` run/return/redraw and not-found verified on device; nested-relative child cwd remains untested by design, see the inheritance decision above).
- [x] Run `genzehn --info` on the generated Zehn output.
- [x] Verify that editing every text-included source and header causes a rebuild (`touch include/tish.h && make` recompiles; second `make` is a no-op).
