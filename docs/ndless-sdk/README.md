# Ndless SDK Manual

This manual documents the Ndless SDK and the runtime contracts used by native
TI-Nspire programs. It is written against the SDK tree installed at
`/home/felix/Ndless/ndless-sdk` and the matching runtime tree at
`/home/felix/Ndless/ndless`.

The SDK is not a normal hosted C library. A native program runs inside the
Ndless loader, calls into the TI OS through a fixed software-interrupt ABI, and
uses static libraries linked into a position-independent image. The runtime,
the SDK, the calculator model, and the exact OS build all matter.

The documentation was checked against SDK/runtime revision `9484d8d`. The
working SDK checkout also contained unrelated local changes when this manual
was written; generated binaries and modified samples are not treated as
documentation sources.

## Volumes

- [Overview and runtime model](overview.md)
- [Build and toolchain](build.md)
- [Syscall ABI and generated interfaces](syscalls.md)
- [libndls API reference](libndls.md)
- [Input, display, graphics, USB, and Lua](hardware.md)
- [Nucleus types and compatibility headers](nucleus.md)
- [Newlib, filesystem, and C/C++ runtime](runtime.md)
- [Zehn format and host tools](zehn-tools.md)
- [Ndless runtime and compatibility](runtime-loader.md)
- [Third-party components and samples](third-party.md)
- [Maintainer and reverse-engineering notes](maintainer.md)

## Scope and terminology

This manual distinguishes the following categories:

- **Public SDK API**: an interface intentionally exposed by an installed
  header and library.
- **Implemented behavior**: behavior verified by reading the current source,
  even when the interface is not formally specified.
- **Compatibility API**: an old name or behavior retained for existing code.
- **Reverse-engineered**: an interface or structure inferred from OS binaries,
  IDA data, hardware behavior, or hardcoded addresses.
- **OS-specific**: behavior that depends on an exact OS image, calculator
  family, hardware revision, or syscall address row.
- **Unsafe**: code that can corrupt the OS, framebuffer, interrupt state,
  filesystem, or calculator boot state when used incorrectly.
- **Third-party**: a bundled dependency whose complete API and license are
  owned by another project.

The SDK contains many headers that are not complete specifications. In
particular, `syscall-addrs.h`, `nucleus.h`, `ngc.h`, `usb.h`, and the Lua/SDL
headers expose reconstructed or inherited interfaces. Treat source comments and
the exact SDK revision as authoritative over this manual when they disagree.

## Source of truth

The most important files are:

```text
include/libndls.h          High-level Ndless helpers
include/nucleus.h          Ndless extensions and OS-facing types
include/keys.h             Key and touchpad mappings
include/hook.h             ARM hook macros
include/syscall.h          SWI calling convention
include/syscall-list.h     Stable syscall numbers and flags
include/syscall-decls.h    Generated C/C++ declarations
include/syscall-addrs.h    Generated OS-version address matrix
include/zehn.h             Zehn container structures
libsyscalls/*.cpp          Syscall stubs and newlib integration
libndls/*                  High-level helper implementations
system/*                   Startup objects and linker script
tools/genzehn/*            ELF to Zehn conversion
tools/zehn_loader/*        Legacy PRG compatibility loader
```

The separate `ndless` repository supplies the calculator-side implementation:
the resident SWI handler, loader, OS hooks, LCD compatibility, Lua hooks,
installer code, and OS-version-specific address data.

## Quick start

With the SDK's `bin/` directory and cross-toolchain on `PATH`:

```sh
nspire-tools new hello
nspire-tools main
make
```

The generated pipeline is:

```text
hello.c / hello.cpp / hello.S
    -> nspire-gcc, nspire-g++, or nspire-as
    -> hello.o
    -> nspire-ld / arm-none-eabi-ld.gold
    -> hello.elf
    -> genzehn
    -> hello.zehn
    -> make-prg
    -> hello.tns
```

The resulting file requires a compatible Ndless runtime on the calculator.
Building a `.tns` file does not install Ndless and does not guarantee that the
program is compatible with every calculator or OS version.

## Safety

Back up calculator documents before installing or testing Ndless runtime
components. Application code that uses direct MMIO, hooks, NAND access,
persistent mode, or undocumented OS structures can crash or permanently alter
calculator state. Test on hardware appropriate to the exact OS/model pair.

## What "everything" means here

The manual covers every Ndless-owned header, runtime library, build component,
host tool, standard syscall name, extension entry, executable-format record,
and bundled-library integration point found in the checked-out trees. It does
not duplicate the complete upstream manuals for Lua, zlib, SDL, FreeType,
Nspire I/O, Qt, Boost, ELFIO, or BSD USB; those APIs are catalogued with their
Ndless-specific constraints and provenance. Their upstream headers remain the
authoritative complete references.
