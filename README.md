# tish

A Unix-like shell for the TI-Nspire CX II CAS, built with the
[Ndless](https://github.com/ndless-nspire/ndless) SDK. It gives the calculator
a small command prompt with a few built-ins and a way to launch other Ndless
`.tns` programs by name.

```
user@tinspire:/documents$ ls
MyLib/    hello.tns    ntop.tns
user@tinspire:/documents$ pwd
/documents
user@tinspire:/documents$ ./hello
running hello
```

## Built-in commands

| Command      | Does                                          |
|--------------|-----------------------------------------------|
| `cd [dir]`   | change directory (`cd` alone → documents root)|
| `pwd`        | print working directory                       |
| `ls [path]`  | list directory (directories marked with `/`)  |
| `cat [file]` | print a file (no args: read piped input)      |
| `echo [...]` | print the arguments                           |
| `touch <f>`  | create an empty file                          |
| `cp <a> <b>` | copy a file                                   |
| `mv <a> <b>` | rename / move                                 |
| `rm <file>`  | delete a file                                 |
| `mkdir <d>`  | create a directory                            |
| `rmdir <d>`  | delete an empty directory                     |
| `whoami`     | print the user id                             |
| `clear`      | clear the scrollback                          |
| `hist`       | show the command history                      |
| `help`       | list the built-ins                            |
| `exit`       | quit tish                                     |

Anything else must be prefixed with `./` to run a program: tish looks for
`<name>.tns` in the current directory and the documents root, and launches it
with `nl_exec()`.

## Redirection and pipes

```
user@tinspire:/documents$ ls > files.txt
user@tinspire:/documents$ echo hello >> files.txt
user@tinspire:/documents$ cat files.txt | cat
```

- `cmd > file` writes the command's output to a file, `cmd >> file` appends.
- `cmd | cmd` pipes output between builtins through a 4 KB in-memory buffer;
  `cat` with no arguments reads it. Stages can be chained.
- Both only work for **builtins**: a launched `.tns` program owns the screen
  and has no stdin/stdout for the shell to capture.
- Operators must be surrounded by spaces (`ls > f`, not `ls>f`), and file names
  cannot contain spaces (no quoting yet).

### Typing the operators

The touchpad keypad (CX / CX II) has no `>` key, so:

| Character | Keys                                      |
|-----------|-------------------------------------------|
| `>`       | `shift` + `.` (or the `>` key on clickpads)|
| `|`       | the `|` key, or `shift` + `÷`             |

## Filesystem

- `/` is the real root of the calculator's file area (listable via the `nuc_*`
  API); the documents folder lives at `/documents`.
- The OS's `chdir()`/`getcwd()` are unreliable on this platform (they fail on
  valid targets, succeed on invalid ones, and report wrong directories), so
  `cd`/`pwd`/`ls` track the working directory **logically**: paths are
  resolved by hand (relative, absolute, `.`/`..`), and a directory is accepted
  only if `nuc_opendir()` can list it.
- `cd` alone goes to the documents root.
- The prompt shows the absolute working directory:
  `user@tinspire:/documents/MyLib$ `.
- File contents are read/written with the `nuc_*` file API (`nuc_fopen` etc.),
  which lives in the same reliable namespace as the directory listing.
  `mkdir`/`rmdir`/`rm`/`mv` have no `nuc_*` equivalent and must use the legacy
  OS syscalls, so they are the most likely to misbehave.

## Building

The Ndless toolchain is cross-compile only (host `gcc` cannot produce a `.tns`).
On an Ndless SDK machine with `nspire-gcc`/`nspire-ld`/`genzehn`/`make-prg` on
`PATH`:

```sh
make clean && make        # produces tish.tns
cd apps/hello && make     # produces the hello sample
```

Build and output always land in the project folder. The Makefile compiles the
top-level sources only (nested app projects build on their own).

## Running on the emulator / calculator

1. Copy `tish.tns` to the calculator's documents folder and run it as a normal
   `.tns` program (drag it onto Firebird to test).
2. To try launching, install another program alongside it (e.g. `hello.tns`,
   `ntop.tns`, `TexEdit.tns`) and type `./<name>`.
3. Exit tish with `exit` (or esc).

Note: the touchpad arrow keys are not readable on the Firebird emulator; type
with the on-screen keypad instead.

## Structure

- `include/tish.h` — shared state: screen/scrollback buffers, command line,
  history (`hist`), cwd/docs paths, output sink + pipe buffers, flags.
- `src/tish.c` — main loop: key dispatch, history browsing, line editing.
- `src/fs.c` — init, path normalization, logical path resolution
  (`resolve_path`), cwd/docs tracking.
- `src/render.c` — output sink (`print_line`), scrollback, incremental render,
  screen layout.
- `src/input.c` — key -> char mapping (`read_key`/`wait_key`).
- `src/cmd.cpp` — builtins, pipeline/redirection dispatch, `find_program()` +
  `launch()`.
- `apps/hello/` — a tiny `.tns` program used to test the launch pipeline.

`tish.c` includes the others, so there is a single build unit.

## Layout

30 rows × 53 cols (matches the Nspire console). Rows 0–27 are the scrollback
window (last 40 lines written), row 28 is the prompt + command line, row 29 is the status bar.