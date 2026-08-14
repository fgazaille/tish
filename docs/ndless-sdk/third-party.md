# Third-Party Components and Samples

The SDK installs several upstream or separately maintained components. This
chapter documents how Ndless integrates them; it is not a replacement for each
project's complete manual or license notice.

## zlib

The SDK builds a static target zlib library and installs `zlib.h` and `zconf.h`.
The TI OS also exposes a subset of zlib functions through syscall IDs 77-87.

Use the normal zlib API when linking `libz.a`. Use the syscall subset only when
the exact declaration and OS support are known. Zehn compression uses zlib and
requires a loader variant containing decompression support.

## Lua 5.1

The installed Lua headers are Lua 5.1 headers. The SDK exposes a subset of the
Lua core and auxiliary library through generated syscall wrappers and adds
`nl_lua_getstate` for access to the active calculator interpreter. See
[hardware APIs](hardware.md).

Do not mix Lua 5.2+ headers or binaries with the SDK's Lua ABI. The calculator
owns the embedded state and its lifetime.

## SDL/nSDL

The include tree contains SDL 1.2-compatible headers plus nSDL extensions. The
libraries are commonly:

```text
libSDL.a
libSDL_gfx.a
libSDL_image.a
```

`sdl-config` reports an SDL 1.2-compatible version and emits the SDK include
and `-lSDL` flags. The nSDL libraries are target-specific static libraries,
not a desktop SDL runtime. Consult the installed README and SDL/nSDL source for
the supported video, event, image, and font behavior.

## Nspire I/O

Nspire I/O provides a console abstraction and optional C++ stream support. The
compatibility header `nspireio2.h` includes the main Nspire I/O header with
compatibility enabled.

The main functions used by terminal-style programs include:

```c
nio_init
nio_free
nio_set_default
nio_get_default
nio_csl_savechar
nio_vram_csl_drawchar
nio_vram_draw
nio_putchar
nio_puts
nio_getchar
nio_fgets
nio_printf
```

Console objects own screen/grid state. Initialize before drawing, set a default
when using default-console functions, and free the console before an ordinary
exit. The SDK's `libsyscalls_nspireio.a` uses this path for standard streams.

There are multiple README/version descriptions in the bundled tree. Verify
the source actually built by the current Makefile rather than assuming every
documented feature is present in every archive.

## FreeType 2

The SDK installs FreeType headers under `include/freetype2` and a target
`libfreetype.a`, built against target zlib when configured. The sample embeds a
font as a C array and renders text. Use upstream FreeType lifetime rules:

```text
FT_Init_FreeType
FT_New_Memory_Face or FT_New_Face
FT_Set_Char_Size / FT_Set_Pixel_Sizes
FT_Load_Char / FT_Render_Glyph
FT_Done_Face
FT_Done_FreeType
```

The calculator display format and memory budget remain application concerns.

## BSD-derived USB

`usb.h`, `usbdi.h`, and the HID helper implementations retain BSD-derived
structures, constants, and notices. The Ndless-specific part is the syscall
bridge and the calculator-side availability of those operations. Preserve the
upstream license text when redistributing modified headers or binaries.

## NavNet

`NavNet/include/navnet.h` and its import-library Makefile describe a separate
TI connectivity/DLL surface. It provides opaque node, operation, and channel
handles plus functions such as `TI_NN_Connect`, `TI_NN_Read`, `TI_NN_Write`,
service registration, and file transfer. It is not part of the ordinary
`SUBDIRS` build and should be treated as a separate platform integration.

## Luna and bitmap tooling

Luna converts scripts/resources into TI-Nspire document containers. `bmputil`
is a Qt-based bitmap converter. Neither is a native application API.

## Samples

| Sample | Main lesson |
|---|---|
| `colors` | Old direct framebuffer/color mode API |
| `helloworld-sdl` | Basic nSDL initialization and text |
| `helloworld-cpp` | C++ classes and target runtime |
| `link-sdl` | SDL sprites, images, and game loop |
| `luaext` | Native Lua extension and resident module behavior |
| `particles` | Input polling and real-time rendering |
| `ngc` | OS graphics context drawing |
| `newlib` | printf, files, heap, arguments, dirent, errno |
| `newlib-c++` | C++ newlib integration |
| `zehn` | Constructors, destructors, metadata, and Zehn flags |
| `freetype` | Font embedding and FreeType rendering |

Samples may intentionally demonstrate legacy or unsafe APIs. Read their
Makefiles and source before copying a technique into new software.

## Licensing

At minimum, preserve notices for:

- Ndless runtime files carrying MPL notices.
- Public-domain SDK files.
- Nspire I/O LGPL files.
- BSD-derived USB code.
- Lua 5.1.
- zlib.
- FreeType.
- SDL/nSDL.
- Luna's MPL and embedded dependencies.
- Sample-specific licenses.

The SDK tree should not be redistributed as though it had one license.
