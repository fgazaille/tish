# Build and Toolchain

## Directory layout

```text
bin/                 User-facing wrappers and packaging scripts
include/             Installed headers
lib/                 Target static libraries and imported libraries
libndls/             High-level Ndless helper sources
libsyscalls/         Generated stubs and newlib backend
system/              crt0, C++ init/fini, linker script
thirdparty/          Nspire I/O, zlib, FreeType, imported assets
tools/               genzehn, zehn_loader, luna, bmputil
samples/             Example applications
toolchain/           Cross-toolchain source/build/install tree
NavNet/              Separate NavNet import-library component
```

## Target toolchain

The checked-in bootstrap script targets `arm-none-eabi` and pins the following
versions in its source:

```text
binutils 2.44
GCC      14.2.0
newlib   4.5.0.20241231
GDB      16.2 (optional/host-dependent build step)
```

The wrappers add `-mcpu=arm926ej-s`, define `_TINSPIRE`, and select the Gold
linker driver. The target is little-endian ARM with soft floating point. Newlib
is built without its supplied system-call implementations because
`libsyscalls` supplies them.

Important toolchain assumptions:

- Static linking is normal.
- Threads and TLS are not configured as a normal hosted environment.
- C++ SDK libraries use `-fno-rtti` and `-fno-exceptions`.
- ARM mode is required for some CP15 instructions and Ndless extension SWIs.
- The calculator has no desktop-style process, virtual-memory, or dynamic
  loader environment.

## Host prerequisites

The build scripts assume a Unix-like host with GNU Make and shell utilities.
Depending on the target you build, you may also need:

- PHP for `libsyscalls/mkStubs.php`.
- Boost.Program_options and zlib development files for `genzehn`.
- GMP, MPFR, and MPC for GCC.
- Python development support for an optional GDB build.
- Qt for `bmputil`.
- `xxd` for the FreeType sample's embedded-font step.
- Docker for the Docker workflow.

The scripts are not a promise of portability to every shell or host OS.

## Wrapper commands

### `nspire-gcc` and `nspire-g++`

Both wrappers:

1. Ask `nspire-tools path` for the SDK root.
2. Prepend `toolchain/install/bin` to `PATH`.
3. Create `~/.ndless/include` if needed.
4. Execute the ARM compiler with:

```text
-mcpu=arm926ej-s -D _TINSPIRE -fuse-ld=gold
-I ~/.ndless/include
-I <sdk>/include
-I <sdk>/include/freetype2
```

The user include directory is intentionally searched before the SDK include
directory so local replacement headers can be supplied without editing the
SDK.

### `nspire-as`

This is a compiler-based assembler wrapper rather than a bare assembler. It
enables preprocessing and defines `GNU_AS`, allowing assembly sources to
include SDK headers. Use it for `.S` files.

### `nspire-ld`

This is a backwards-compatible alias that executes `nspire-gcc`. Existing
Makefiles often invoke it as their linker variable, but the actual driver
ultimately reaches the custom `arm-none-eabi-ld.gold` wrapper.

### `nspire-tools`

```text
nspire-tools new <name>       Create a Makefile from Makefile.tpl
nspire-tools main             Copy main.c.tpl
nspire-tools path             Print the SDK root
nspire-tools _toolchainpath   Print the cross-toolchain bin directory
nspire-tools _zehn_loader_path [suffix]
```

The last command lazily builds and locates `zehn_loader.tns` or
`zehn_loader_compressed.tns`.

## Linker behavior

The SDK ships a linker driver named `arm-none-eabi-ld.gold`. It preprocesses
arguments, removes standard CRT and library arguments that do not fit the
Ndless runtime, builds startup objects if necessary, and invokes the real ARM
linker with:

```text
--pic-veneer --emit-relocs -T system/ldscript -static
```

The default target library group includes the available third-party libraries,
`libstdc++`, `libndls`, `libsyscalls`, `libm`, `libc`, and `libgcc`. The custom
`--nspireio` option selects `libsyscalls_nspireio.a`.

`--emit-relocs` is essential: `genzehn` reads the retained ELF relocation
sections to create runtime relocation entries. `--pic-veneer` supplies suitable
long-branch veneers for a position-independent loaded image.

## Startup objects

### `crt0.S`

The entry sequence is:

```text
push r4-r12,lr
save original SP
save argc/argv
initialise_monitor_handles()
__cpp_init()
restore argc/argv
main(argc, argv)
save return value
__cpp_fini()
restore return value
exit()
restore original SP and return to loader
```

`initialise_monitor_handles` has a weak no-op fallback so a deliberately
minimal `-nostdlib` program can link without the full newlib backend.

### `crti.S` and `crtn.S`

The SDK uses hand-written sentinel-terminated constructor/destructor lists.
`crti.S` supplies the loop and labels; `crtn.S` supplies `-1` terminators and a
weak `exit` fallback that jumps to `__crt0_exit`.

The linker script's init/fini ordering is part of this ABI. Do not replace the
startup objects with ordinary hosted CRT objects.

## Linker script layout

`system/ldscript` links the image at address zero because the loader later adds
the actual load base:

```text
.text       entry, startup, init/fini arrays, code
.got        GOT entries followed by 0xffffffff sentinel
.data       read-only data, writable data, .genzehn markers
.ARM.extab  exception tables
.ARM.exidx  unwind index
.eh_frame   unwind data
.bss        zero-filled data
```

The GOT sentinel is consumed by the `ADD_BASE_GOT` Zehn relocation. The
`.genzehn` section carries weak marker symbols that tell `genzehn` whether the
old or new LCD API was linked.

## Library build flags

### libsyscalls

The library uses the ARM926 target, `-nostdlib`, `-fPIE`, `-mlong-calls`,
function/data sections, and strict warnings. `stubs.cpp` is regenerated from
`syscall-list.h` by `mkStubs.php`; the same generator writes
`include/syscall-decls.h`.

It produces:

```text
lib/libsyscalls.a
lib/libsyscalls_nspireio.a
```

The second archive differs in standard stream handling, routing input/output
through Nspire I/O.

### libndls

The library uses optimization, function/data sections, strict warnings, and
`-nostdlib`. C++ sources use C++11 with RTTI and exceptions disabled. `idle.o`
and `clear_cache.o` are forced to ARM mode because they contain ARM CP15
instructions.

## Build outputs

A typical application build has these stages:

```text
program.o
program.elf       Linked image with retained relocations
program.zehn      Relocatable Zehn container
program.tns       Legacy PRG wrapper containing loader + Zehn
```

`make-prg` validates the Zehn input with `genzehn --info`, detects compression,
and concatenates the matching loader before the Zehn payload.

## Debug and reproducibility

Use `DEBUG=TRUE` in the generated Makefile for `-O0 -g`. Use `genzehn --info`
to inspect the output container. Generated binaries are not source-of-truth;
rebuild them with the exact SDK/toolchain revision when investigating a
problem.
