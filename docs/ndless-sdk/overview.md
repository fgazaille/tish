# Overview and Runtime Model

## What Ndless changes

The TI-Nspire OS is a monolithic ARM/Nucleus image. Ndless installs a resident
extension that changes how native files are recognized and executed, installs a
software-interrupt handler, exposes selected OS functions through a stable
numeric registry, and provides additional runtime services.

The SDK is the host-side development environment for that runtime. It provides:

- ARM cross-compilation wrappers and a linker driver.
- Static syscall and newlib libraries.
- Convenience functions for keys, timing, dialogs, LCDs, files, and hardware.
- A position-independent startup object and linker script.
- The Zehn executable format and packagers.
- Third-party libraries such as Lua headers, zlib, SDL/nSDL, FreeType, and
  Nspire I/O.
- Samples that demonstrate supported and legacy APIs.

## Program execution model

A native program is not an independent process. The loader roughly performs:

1. Opens a file and identifies its executable format.
2. Allocates executable memory and loads the image.
3. Applies the image's relocation records.
4. Builds `argc` and `argv`.
5. Saves selected OS display and interrupt state.
6. Calls the image entry point as `entry(argc, argv)`.
7. Restores OS state and frees the image unless it is resident.

The SDK startup object defines that entry point as `_start`. `_start` saves the
calculator's stack pointer, initializes newlib monitor handles, runs C++ static
constructors, calls `main`, runs destructors, calls `exit`, and finally restores
the original stack and returns to the loader.

The practical application signature is:

```c
int main(int argc, char **argv);
```

Some examples use `int main(void)`, which is valid when the program does not
inspect arguments. `argv[0]` is normally the launched path. Additional values
are loader- or caller-dependent; do not assume desktop Unix conventions.

## Resident applications

An ordinary application is freed after it returns. A resident application
keeps its image allocated after returning so that another component can call
code or data in it later. `nl_set_resident()` marks the current image resident;
the loader also supports a resident pointer for Lua modules and other internal
uses.

Resident code must follow stricter rules:

- It must not use stack or `argv` storage after returning.
- It must not assume that ordinary cleanup will happen later.
- It must call `_exit()` rather than returning from `main` when the SDK's
  resident path requires that behavior.
- Any exported function must remain valid for the entire resident lifetime.
- Lua extensions must be unloaded during the Lua interpreter shutdown path.

The ordinary application path should simply return from `main` and let the
startup/runtime perform cleanup.

## Software-interrupt boundary

The SDK does not link against a conventional OS import table. A syscall wrapper
places a numeric immediate in an ARM `swi` instruction. The Ndless handler reads
that immediate, chooses an OS-version-specific target address, and transfers
control to the OS function. The number, not an OS address, is the application
binary interface used by normal programs.

The exception is the `STAGE1` installer build. Stage 1 runs before the Ndless
SWI handler exists and uses direct calls through the generated address matrix.
See [Syscall ABI](syscalls.md) and [Runtime loader](runtime-loader.md).

## Hardware families

Code commonly distinguishes:

| Predicate | Meaning in the current SDK |
|---|---|
| `is_classic` | `hwtype() < 1`; classic grayscale/clickpad family |
| `has_colors` | Non-classic hardware |
| `is_cm` | `nl_hwsubtype() == 1` |
| `is_cx2` | `nl_hwsubtype() == 2` |
| `is_touchpad` | Keypad type identifies prototype or production touchpad |

These predicates are conveniences, not a complete compatibility matrix. CX II,
HW-W, CM, CAS, clickpad, and touchpad variants can differ in LCD controller,
keypad polarity, timer behavior, and OS addresses. Use the new LCD API rather
than direct framebuffer assumptions for new applications.

## OS versions and address rows

`syscall-addrs.h` contains a generated matrix of 50 OS/model rows and 343
standard syscall columns. Runtime recognition, installer availability, and
application compatibility are separate questions:

- An address row may exist without a current installer artifact.
- A runtime may recognize an OS but an individual syscall can be zero or
  unsafe.
- The same OS version can have different rows for calculator models.
- A newer hardware revision can require compatibility code even when syscall
  names are unchanged.

Never copy an address from one OS row into application code. Use the stable
syscall number or `nl_osvalue()` for an explicitly versioned value.

## Filesystem namespace

The Nucleus filesystem and Ndless's virtual documents root are not identical to
a desktop filesystem. `get_documents_dir()` is the supported way to find the
documents root. `os.h` and `libndls.h` intentionally make the old `NDLESS_DIR`
and `SCREEN_BASE_ADDRESS` names fail at compile time unless a legacy screen API
mode is explicitly selected.

Applications should:

- Use `get_documents_dir()` for the user documents root.
- Treat paths as calculator paths, not POSIX paths.
- Use `nuc_*` or newlib wrappers consistently.
- Avoid relying on `chdir`/`getcwd` behavior when logical path resolution is
  more reliable.
- Close every `NUC_FILE` and `NUC_DIR` handle.

## Licensing and provenance

The repository is not covered by one universal license. The SDK headers include
public-domain and compatibility files; Ndless runtime code includes Mozilla
Public License notices; Nspire I/O is LGPL; USB code is BSD-derived; and Lua,
zlib, FreeType, SDL, and Luna have their own notices. See
[Third-party components](third-party.md).
