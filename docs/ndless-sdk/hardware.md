# Input, Display, Graphics, USB, and Lua

This volume collects interfaces whose correctness depends strongly on hardware
or on the TI OS's internal data structures.

## Key representation

`include/keys.h` represents a key as:

```c
    int row;
    int col;
    int tpad_row;
    int tpad_col;
    tpad_arrow_t tpad_arrow;
} t_key;
```

The clickpad matrix and touchpad matrix can use different row/column values.
`KEY_`, `KEYTPAD_`, and `KEYTPAD_ARROW_` construct entries. A dummy row/column
marks a key absent on a particular model.

The header defines complete TI-Nspire constants such as:

```text
KEY_NSPIRE_A ... KEY_NSPIRE_Z
KEY_NSPIRE_0 ... KEY_NSPIRE_9
KEY_NSPIRE_ENTER, RET, ESC, DEL, SHIFT, CTRL
KEY_NSPIRE_UP, DOWN, LEFT, RIGHT and diagonal arrows
KEY_NSPIRE_MENU, HOME, CLICK, DOC, TRIG, SCRATCHPAD
```

It also includes a TI-84+ mapping family (`KEY_84_*`). The mapping is a data
contract for `isKeyPressed`, not a promise that every key exists on every
device.

## Key matrix behavior

The current implementation reads memory-mapped keypad rows around
`0x900E0000`. Classic devices use active-low bits; CX-family devices use the
opposite polarity. `isKeyPressed.c` normalizes that difference. This is direct
MMIO and therefore unsafe to copy to unrelated hardware.

The ON key is separate from the normal matrix. `on_key_pressed.c` uses one
register for classic/CX1 and another for CX II.

## Touchpad protocol

The touchpad is accessed through OS I2C helpers:

```c
int touchpad_read(unsigned char page, unsigned char reg, void *data);
int touchpad_write(unsigned char page, unsigned char reg, void *data);
```

The libndls implementation follows the reverse-engineered page protocol:

- Page 4 contains the report expected by the OS interrupt handler.
- Page `0x10` contains size information.
- The page selector is written using register `0xff`.
- Width, height, X, and Y are byte-swapped because the device representation
  is big-endian.
- IRQ and FIQ are masked while selecting the information page and restored
  afterward.

The report is interpreted into a coarse 3x3 arrow grid:

```c
    TPAD_ARROW_NONE,
    TPAD_ARROW_UP, TPAD_ARROW_UPRIGHT,
    TPAD_ARROW_RIGHT, TPAD_ARROW_RIGHTDOWN,
    TPAD_ARROW_DOWN, TPAD_ARROW_DOWNLEFT,
    TPAD_ARROW_LEFT, TPAD_ARROW_LEFTUP,
    TPAD_ARROW_CLICK
} tpad_arrow_t;
```

Do not issue arbitrary I2C/page commands from application code while the OS is
running. Leaving the device on a page other than page 4 can break the OS touch
handler.

## Display formats

`libndls.h` defines:

```c
    SCR_TYPE_INVALID = -1,
    SCR_320x240_565 = 0,
    SCR_320x240_4 = 1,
    SCR_240x320_565 = 2,
    SCR_320x240_16 = 3,
    SCR_320x240_8 = 4,
    SCR_320x240_555 = 5,
    SCR_240x320_555 = 6,
    SCR_TYPE_COUNT = 7
} scr_type_t;
```

The native LCD type may differ from the application buffer type. The new LCD
API handles conversion/blitting on supported revisions; the old API assumes a
native direct framebuffer.

### Old screen API details

The old functions manipulate controller bits in `IO_LCD_CONTROL`, selected
from different register addresses for classic and color hardware. Grayscale is
4 bits per pixel, two pixels per byte; color is normally 16-bit RGB565. The
framebuffer pointer is read from a fixed OS location represented by
`REAL_SCREEN_BASE_ADDRESS`.

This path is marked legacy because it assumes 320x240 and cannot correctly
represent the HW-W rotated panel without runtime compatibility support.

### New screen API details

`lcd_init(type)` selects a format and may allocate or restore a temporary
framebuffer. `lcd_blit(buffer, type)` resolves a format-specific function and
caches it. `lcd_type()` returns the runtime native type, or
`SCR_TYPE_INVALID` when the kernel result is invalid.

For Ndless revisions before the kernel LCD extensions, libndls uses limited
fallback copies and controller writes. Those fallbacks are not equivalent to
the modern HW-W path.

### Framebuffer discipline

- Use the documented buffer type and size.
- Call `lcd_init(SCR_TYPE_INVALID)` when restoring a temporary mode.
- Avoid retaining `REAL_SCREEN_BASE_ADDRESS` across a mode change.
- Invalidate caches after self-modifying or relocated executable data, not as a
  substitute for correct LCD synchronization.
- Do not mix old direct writes and new `lcd_blit` ownership without restoring
  the mode first.

## NGC graphics context API

`include/ngc.h` exposes the OS graphics context interface. The central type is:

```c
```

This is an internal OS object, not a portable standalone graphics context.

### Lifecycle

```c
Gc gui_gc_global_GC(void);
Gc gui_gc_copy(Gc gc, int width, int height);
int gui_gc_begin(Gc gc);
void gui_gc_finish(Gc gc);
void gui_gc_free(Gc gc);
```

Cache the global GC only for the period documented by the surrounding OS
operation. `gui_gc_begin`/`finish` bracket drawing transactions. A copied GC is
owned by the caller only according to the OS implementation; always pair it
with `gui_gc_free` when the API returns a separately allocated context.

### Attributes

```c
gui_gc_clipRect
gui_gc_setColorRGB
gui_gc_setColor
gui_gc_setAlpha
gui_gc_setFont
gui_gc_getFont
gui_gc_setPen
gui_gc_setRegin
```

The header preserves the historical spelling `setRegin`; the generated syscall
declaration uses `gui_gc_setRegion`. Verify the symbol used by your SDK build.

String modes combine sizing, baseline/vertical alignment, and rotation:

```c
GC_SM_NORMAL, GC_SM_SHRINK, GC_SM_OVERLAP
GC_SM_BASELINE, GC_SM_BOTTOM, GC_SM_MIDDLE, GC_SM_TOP
GC_SM_RIGHT, GC_SM_DOWN, GC_SM_LEFT
```

`gui_gc_drawString` takes a `char *` that represents the OS's UTF-16 string
convention, despite the C type not spelling `uint16_t *`. Construct strings in
the format expected by the OS and do not pass ordinary UTF-8 without conversion.

### Drawing and metrics

Primitives include arcs, icons, sprites, lines, rectangles, polygons, filled
variants, gradients, and TI image resources. Arc angles are multiplied by 10.
The font enum encodes family, style, and size.

Metrics include character/string width and height, font height, and icon size.
Use the same font and UTF-16 representation for measurement and drawing.

### Blitting

```c
gui_gc_blit_gc
gui_gc_blit_buffer
gui_gc_blit_to_screen
gui_gc_blit_to_screen_region
```

The implementation in `libndls/ngc.c` reaches into reverse-engineered GC
fields to find an off-screen buffer. The source explicitly warns that this is
not portable and is broken on HW-W. Region coordinates are converted between
pixels and bytes based on current color/grayscale mode. It is legacy code; use
the modern LCD API for new software.

`gui_gc_fillGradient` also carries an OS stability warning in the header. Do
not draw beyond the documented screen height, and test color combinations on
the target hardware.

## USB interface

The SDK's `usb.h` and `usbdi.h` are NetBSD/FreeBSD-derived USB interfaces
adapted to the TI OS. The handles are opaque:

```c
```

### Wire structures

USB wire values use byte arrays to allow unaligned little-endian fields:

```c
```

Use `UGETW`, `USETW`, `UGETDW`, `USETDW`, and `USETW2`. Do not cast packed wire
fields to native integer pointers.

The central request is:

```c
    uByte bmRequestType;
    uByte bRequest;
    uWord wValue;
    uWord wIndex;
    uWord wLength;
} usb_device_request_t;
```

Descriptors include device, configuration, interface, endpoint, string, hub,
qualifier, and OTG forms. Descriptor pointers returned by the OS are borrowed;
do not free them.

### Transfer lifecycle

The normal workflow is:

1. Obtain an interface/device handle from the driver environment.
2. Inspect endpoint descriptors.
3. Open a pipe with `usbd_open_pipe`.
4. Allocate a transfer with `usbd_alloc_xfer`.
5. Configure it with `usbd_setup_xfer` or `usbd_setup_isoc_xfer`.
6. Submit it with `usbd_transfer`, or use synchronous transfer behavior.
7. Read status with `usbd_get_xfer_status`.
8. Abort outstanding transfers before closing a pipe.
9. Close the pipe and free the transfer on detach.

Callback type:

```c
                              usbd_private_handle,
                              usbd_status);
```

Buffers, callback code, private context, and frame-length arrays must remain
valid until the operation completes or is aborted. The headers do not provide
general thread-safety guarantees.

Important status values include `USBD_NORMAL_COMPLETION`, `USBD_IN_PROGRESS`,
`USBD_CANCELLED`, `USBD_TIMEOUT`, `USBD_SHORT_XFER`, and `USBD_STALLED`.
`usbd_errstr` returns a diagnostic string owned by the USB stack.

### HID helpers

`usbd_set_idle` and `usbd_set_protocol` construct standard HID class/interface
requests and submit them through `usbd_do_request`. They are SDK helpers ported
from BSD utility code, not OS-native functions. `duration` and `id` are packed
as the HID request requires; `report` is the protocol value.

The presence of USB declarations does not prove that every OS build supports
every operation. Check `nl_hassyscall` and test on the exact target.

## Lua 5.1

The installed Lua headers describe Lua 5.1:

```text
LUA_VERSION     "Lua 5.1"
LUA_VERSION_NUM 501
lua_Number      double
lua_Integer     int
```

The Lua state is opaque. A native C function receives `lua_State *` and
returns the number of result values left on the stack.

### Lifetime rules

- The calculator's active `lua_State *` is owned by the OS interpreter.
- `nl_lua_getstate()` returns a borrowed pointer; do not call `lua_close()` on
  it.
- Lua strings and userdata are managed by Lua's garbage collector.
- `lua_tolstring` returns a borrowed pointer invalidated when the value is
  removed or changed.
- Registry references from `luaL_ref` keep values alive until `luaL_unref`.
- A `luaL_Reg` array is normally static and ends with `{NULL, NULL}`.

### Ndless Lua integration

Ndless hooks the interpreter to capture its state and registers native extension
support. The public extension function is:

```c
lua_State *nl_lua_getstate(void);
```

The runtime also supplies the Ndless-specific `nrequire` mechanism and an
`ndless` table with uninstall behavior. Native modules use `.luax.tns` files,
are searched relative to the documents area, and are kept resident until Lua
shutdown. The current implementation limits loaded modules to 30 and module
names to fewer than 30 characters.

Lua hooks, module lifetime, startup register conventions, and the shared state
are reverse-engineered and OS-version-specific. Do not assume ordinary Lua
thread safety or that an extension can outlive the interpreter.
