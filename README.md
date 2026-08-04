# tish

A Unix-like shell for the TI-Nspire CX II CAS, built with the
[Ndless](https://github.com/ndless-nspire/ndless) SDK. It gives the calculator
a small command prompt with a few built-ins and a way to launch other Ndless
`.tns` programs by name.

```
$ ls
MyLib/    hello.tns    ntop.tns
$ pwd
/documents
$ hello
running hello
```
## Built-in commands

| Command      | Does                                          |
|--------------|-----------------------------------------------|
| `cd [dir]`   | change directory (`cd` alone → documents root)|
| `pwd`        | print working directory                       |
| `ls [path]`  | list directory (directories marked with `/`)  |
| `whoami`     | print the user id                             |
| `clear`      | clear the scrollback                          |
| `help`       | list the built-ins                            |
| `exit`       | quit tish                                     |

Anything else is treated as a program name: tish looks for `<name>.tns` in the
current directory and the documents root, and launches it with `nl_exec()`.

## Filesystem

- The root is `get_documents_dir()` — the calculator's documents folder.
- `cd`/`pwd`/`ls` track a working directory via `chdir()`/`getcwd()`.
- The prompt shows the working directory relative to the documents root, e.g.
  `$ ~` at the root or `$ ~/MyLib` after `cd MyLib`.

## Building

The Ndless toolchain is cross-compile only (host `gcc` cannot produce a `.tns`).
On an Ndless SDK machine with `nspire-gcc`/`nspire-ld`/`genzehn`/`make-prg` on
`PATH`:

```sh
make clean && make        # produces tish.tns
cd apps/hello && make     # produces the hello sample
```

Build and output always land in the project folder. The Makefile compiles the
top-level `.c`/`.cpp`/`.S` files only (nested app projects build on their own).

## Running on the emulator / calculator

1. Copy `tish.tns` to the calculator's documents folder and run it as a normal
   `.tns` program (drag it onto Firebird to test).
2. To try launching, install another program alongside it (e.g. `hello.tns`,
   `ntop.tns`, `TexEdit.tns`) and type its name.
3. Exit tish with `exit` (or esc).

Note: the touchpad arrow keys are not readable on the Firebird emulator; type
with the on-screen keypad instead.

## Structure

- `tish.c` — the whole shell: input loop, scrollback, render, builtins,
  `find_program()` + `launch()`.
- `Makefile` — Ndless build.
- `apps/hello/` — a tiny `.tns` program used to test the launch pipeline.

## Layout

30 rows × 53 cols (matches the Nspire console). Rows 0–27 are the scrollback
window, row 28 is the prompt, row 29 is the status bar.