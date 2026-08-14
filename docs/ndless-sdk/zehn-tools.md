# Zehn Format and Host Tools

## Why Zehn exists

Ndless programs are loaded at addresses chosen at runtime. A normal linked ELF
cannot simply be copied to an arbitrary address, so the SDK converts the
loadable image and its absolute relocations into a compact self-describing
container called Zehn.

The format is defined by `include/zehn.h`, versioned independently from the
compiler, and currently uses little-endian packed records.

## File layout

The byte layout is:

```text
Zehn_header
Zehn_reloc[reloc_count]
Zehn_flag[flag_count]
extra data, padded as required
executable image
```

The header contains:

| Field | Meaning |
|---|---|
| `signature` | `ZEHN_SIGNATURE`, the bytes for `"Zehn"` in little-endian form |
| `version` | `ZEHN_VERSION`, currently `1` |
| `file_size` | Size of the stored file/image payload represented by the header |
| `reloc_count` | Number of relocation records |
| `flag_count` | Number of metadata records |
| `extra_size` | Extra data size in bytes |
| `alloc_size` | Runtime allocation size including zero-filled tail |
| `entry_offset` | Entry offset within the loaded executable image |

The executable begins after the tables and extra data and is aligned by the
extra-data layout. `.bss` is normally represented as zero-filled allocation
space rather than stored bytes.

## Relocations

Each relocation packs an 8-bit type and a 24-bit offset.

### `ADD_BASE`

Add the runtime image base to the 32-bit word at the offset. `genzehn` emits
this for supported ARM absolute relocations such as `R_ARM_ABS32` and
`R_ARM_TARGET1`.

### `ADD_BASE_GOT`

Add the runtime base to each word in the GOT until the `0xffffffff` sentinel
written by `system/ldscript` is reached.

### `SET_ZERO`

Write zero at the relocation offset. This is used to undo some GOT references
to undefined weak symbols.

### `FILE_COMPRESSED`

This record must precede other relocation records. Its offset contains the
`Zehn_compress_type`; version 1 defines zlib compression (`ZLIB = 0`). The
loader decompresses the stored image before applying ordinary relocations.

### `UNALIGNED_RELOC`

This marker has offset zero and tells the loader that at least one relocation
is not naturally aligned. The loader must use byte-safe reads and writes rather
than assuming a 32-bit aligned pointer.

Unknown relocation types are fatal to a loader. A loader must validate the
header before allocating or jumping to the image.

## Metadata flags

Unknown flags must be ignored by loaders. Current flag types include:

```text
NDLESS_VERSION_MIN / MAX
NDLESS_REVISION_MIN / MAX
RUNS_ON_COLOR
RUNS_ON_CLICKPAD
RUNS_ON_TOUCHPAD
RUNS_ON_32MB
EXECUTABLE_NAME
EXECUTABLE_AUTHOR
EXECUTABLE_VERSION
EXECUTABLE_NOTICE
RUNS_ON_HWW
USES_LCD_BLIT
```

String flags contain offsets into extra data. Version flags encode the Ndless
version as a value such as `31` for 3.1; revision bounds apply with their
corresponding version bounds. Hardware flags let the loader refuse an
incompatible program before execution.

The SDK's LCD APIs emit marker symbols in `.genzehn`. `genzehn` detects those
symbols and sets `USES_LCD_BLIT`; the runtime uses that metadata when deciding
whether compatibility behavior is needed.

## `genzehn`

`tools/genzehn/genzehn.cpp` uses ELFIO, Boost.Program_options, and zlib.

The conversion algorithm is:

1. Open and validate an ARM ELF.
2. Copy all `SHF_ALLOC` sections into one image at their linked offsets,
   zero-filling gaps.
3. Skip `.bss` from stored bytes and account for its allocation tail.
4. Scan symbols for undefined references and LCD marker symbols.
5. Reject strong undefined symbols; weak undefined references can be nulled.
6. Read retained `SHT_REL` sections.
7. Convert supported absolute relocations into Zehn records.
8. Add a GOT relocation when the linker-script sentinel is present.
9. Optionally compress the image with zlib.
10. Write the header, tables, extra data, and image.

`SHT_RELA` is rejected by the current converter. Unsupported relocation types
are errors. `--info --input file` scans the file for a Zehn signature and
reports metadata, which is useful even when the file has a PRG loader prefix.

The source is authoritative for the exact command-line option names and for
which informational fields are printed.

## `zehn_loader`

The legacy loader is itself built as a PRG-compatible native program. Its
linker script places a `PRG` magic value at the beginning and exposes
`zehn_start` at the end. `make-prg` concatenates this loader and the Zehn file.

At runtime it:

1. Finds the appended Zehn header using a PC-relative self-location trick.
2. Validates the header and allocation sizes.
3. Allocates/decompresses a staging image when needed.
4. Applies the Zehn relocation records.
5. Calls the image entry point.
6. Frees temporary storage and returns the result.

Modern Ndless can find the embedded Zehn directly; older compatible runtimes
can execute the loader first. The wrapper is therefore a compatibility layer,
not an additional application process.

## `make-prg`

```sh
make-prg program.zehn program.tns
```

The script validates the input, detects whether it contains a compression
record, chooses `zehn_loader.tns` or `zehn_loader_compressed.tns`, and
concatenates loader plus Zehn payload. It does not compile or relink the
program.

## Other host tools

### Luna

`tools/luna` packages Lua, Python, XML, and resources into TI-Nspire document
containers. It is a host document generator, not a native executable linker.
Its bundled compression/encryption and document encodings are format-specific;
follow the tool's README and source for supported input versions.

### `bmputil`

`tools/bmputil` is a Qt-based converter for TI bitmap resources. The SDK's TI
bitmap forms use RGB565 data plus an alpha representation. It is not built by
the ordinary `tools/Makefile` and requires Qt.

### `nspire-tools`

Besides project scaffolding, this script lazily builds the Zehn loader and
returns paths used by `make-prg`.
