# libndls API Reference

`libndls.a` is the high-level convenience layer. It is not a complete libc and
does not hide all hardware details. Many helpers call generated OS stubs;
others access memory-mapped registers directly.

Include it with:

```c
#include <libndls.h>
```

The normal link driver supplies `-lndls`. With a custom link command, link the
SDK startup objects and `libndls.a` together with `libsyscalls.a` and the
required C/C++ runtime libraries.

## Constants and model predicates

```c
SCREEN_WIDTH   320
SCREEN_HEIGHT  240
BLACK          0x0
WHITE          0xF
```

CPU speed constants are raw classic-clock register values:

```c
CPU_SPEED_150MHZ  0x00000002
CPU_SPEED_120MHZ  0x000A1002
CPU_SPEED_90MHZ   0x00141002
```

Convenience predicates are macros over runtime queries:

```c
is_touchpad
is_classic
is_cm
is_cx2
has_colors
```

Do not cache these predicates across a runtime or hardware transition unless
the specific API says the value is immutable. `hwtype()` and touchpad
detection themselves cache values because the calculator model does not change
while a program runs.

## Runtime and safety helpers

### `assert_ndless_rev(unsigned required_rev)`

Displays a message and exits if the installed Ndless revision is too old. It
is a convenience guard, not a replacement for checking hardware, OS ID, or
individual syscall availability.

### `clear_cache(void)`

Cleans/invalidate ARM instruction and data caches using CP15 operations. Use it
after writing executable code or installing an in-memory hook. The helper is
compiled in ARM mode and is deliberately a function rather than an inline
Thumb fragment.

### `hwtype(void)`

Returns the cached Ndless hardware family value. Current code treats `0` as
classic and `1` as color/CX-family hardware. Unknown values should be handled
conservatively.

### `enable_relative_paths(char **argv)`

Extracts the directory from `argv[0]` and asks the OS to make it current. It is
useful for older programs that expect paths relative to their executable.
Return `-1` for malformed input or a failed directory change. It does not
rewrite `argv`.

### `file_each(const char *folder, int (*callback)(const char *, void *), void *context)`

Recursively walks a directory tree. Entries are collected and sorted before
visiting, so traversal is deterministic rather than dependent on filesystem
order. Callback results are:

```text
0  continue
1  abort the entire walk
2  continue but do not descend into this directory
```

The implementation uses a fixed local name arena and a maximum entry budget;
very large directories can be truncated. Paths are passed as calculator
filesystem paths.

### `locate(const char *filename, char *dst_path, size_t dst_path_size)`

Searches the documents tree by basename. It returns `0` when a matching path is
written and `1` when no match is found. A destination too small to hold the
full path is treated as failure rather than silently returning a truncated
result.

### `idle(void)`

Masks interrupts so the timer remains available, executes ARM wait-for-
interrupt, acknowledges the timer source, and restores the mask. It is the
preferred low-power primitive for polling loops. It accesses different timer
and interrupt-controller registers on classic and CX hardware.

### `msleep(unsigned milliseconds)`

Sleeps using the hardware timer and WFI. Classic and CX use different timer
implementations. The CX path intentionally does not call `idle()` because
`idle()` acknowledges an interrupt that the CX sleep loop needs to observe.

### `on_key_pressed(void)`

Reads the physical ON key from hardware registers. CX II uses a different
register from classic/CX1. The result is active-low normalized to `BOOL`.

### `refresh_osscr(void)`

Calls both the home-screen and document-browser refresh services. The order is
intentional; refreshing only one can leave the OS display inconsistent.

### `set_cpu_speed(unsigned speed)`

On classic/CX1 it writes the clock/PLL register and trigger, returning the old
register value. On CX II the current implementation returns zero without
changing the clock. Passing an arbitrary value is unsafe.

## Keyboard and touchpad

### `any_key_pressed(void)`

Scans the keypad matrix and, on touchpad models, reports a touchpad contact as
input. Matrix polarity differs between classic and CX hardware.

### `isKeyPressed(const t_key *key)`

Tests one key. The function-like compatibility macro lets callers write:

```c
if (isKeyPressed(KEY_NSPIRE_ENTER)) {
    /* ... */
}
```

Touchpad arrow keys are resolved through a touchpad scan rather than a normal
matrix bit. See [hardware APIs](hardware.md).

### `wait_key_pressed(void)` and `wait_no_key_pressed(void)`

Poll with `any_key_pressed()` and sleep with `idle()`. `wait_key_pressed()` first
waits for all previous input to be released, which provides a simple debounce
policy.

### `touchpad_getinfo(void)`

Returns cached touchpad dimensions or `NULL` on a non-touchpad device. The
implementation temporarily masks IRQ/FIQ while switching I2C pages because the
OS interrupt handler expects the device to remain on page 4.

### `touchpad_scan(touchpad_report_t *report)`

Reads the touchpad report, byte-swaps device-endian coordinates, and computes a
coarse arrow region. The report includes contact/proximity, coordinates,
velocity, pressed state, and `tpad_arrow_t`. The current implementation returns
zero for success and a nonzero value for an I2C/read error.

### `touchpad_arrow_pressed(tpad_arrow_t arrow)`

Scans and compares the computed arrow region. It is intended as an internal
helper; application code normally calls `isKeyPressed` with a touchpad-aware
key constant.

## Dialogs and text input

The TI dialog APIs use UTF-16 strings. libndls converts ASCII input and manages
temporary buffers. On color-capable hardware currently in grayscale, the old
message-box implementation saves/restores the framebuffer and temporarily
switches modes to avoid OS redraw corruption.

### `show_msgbox` macros and `_show_msgbox`

```c
show_msgbox(title, message)
show_msgbox_2b(title, message, button1, button2)
show_msgbox_3b(title, message, button1, button2, button3)
```

The underlying `_show_msgbox` is variadic and returns the selected button ID.
The OS uses a `"DLG"` button-list convention. Interrupts are masked around the
dialog because the dialog's key handling must not race with the program's
state.

### `show_msg_user_input`

```c
int show_msg_user_input(
    const char *title,
    const char *message,
    const char *default_value,
    char **value_ref
);
```

Returns the entered string length or `-1` for cancellation/empty input/error.
The returned string is heap allocated when `value_ref` is non-NULL and must be
freed by the caller.

### `show_1numeric_input` and `show_2numeric_input`

Display bounded integer input dialogs. Return `1` for the OS's OK result and
`0` for cancellation. The wrappers convert strings, pass output pointers, and
work around an OS behavior where minimum bounds `0` and `-1` act as special
values; the result is clamped back to the requested minimum.

## Configuration

```c
cfg_open(void)
cfg_open_file(const char *filepath)
cfg_close(void)
char *cfg_get(const char *key)
cfg_register_fileext(const char *ext, const char *program)
cfg_register_fileext_file(const char *filepath, const char *ext,
                          const char *program)
```

The parser reads a small `key=value` file into memory, strips comments and
line endings, and stores offsets rather than duplicated pointers. `cfg_get`
returns a pointer into the active buffer; it becomes invalid after
`cfg_close()`. The default configuration is `ndless.cfg.tns`, located through
the documents tree.

`cfg_register_fileext` adds an `ext.<extension>=<program>` mapping without
duplicating an existing key. Configuration files are limited by the current
implementation's fixed maximum; this is not a general INI parser.

## Filesystem and execution helpers

libndls exposes no separate `nuc_*` declarations; those are generated syscall
stubs. It composes them through `file_each`, `locate`, configuration, and the
Ndless extension:

```c
int nl_exec(const char *program, int argc, char **argv);
```

`nl_exec` asks the resident loader to execute another native file. It runs in
the same loader context and may replace the screen. Restore or redraw your
application display after it returns.

## Legacy screen API

Define `OLD_SCREEN_API` before including `libndls.h` to expose:

```c
void clrscr(void);
BOOL lcd_isincolor(void);
void lcd_incolor(void);
void lcd_ingray(void);
unsigned _scrsize(void);
```

The old API assumes 320x240 and direct access through the OS framebuffer
pointer. It is retained for compatibility and is not suitable for HW-W rotated
LCDs. New applications should use the new API.

## New LCD API

Without `OLD_SCREEN_API`:

```c
bool lcd_init(scr_type_t type);
void lcd_blit(void *buffer, scr_type_t buffer_type);
```

The Ndless revision determines whether these call kernel LCD extensions or use
legacy fallbacks. On supported revisions, the kernel handles display rotation,
controller configuration, and hardware-specific buffers. Applications must
provide a buffer matching the selected `scr_type_t`; do not assume every type
has the same byte size.

## USB helper functions

```c
usbd_status usbd_set_idle(usbd_interface_handle iface,
                          int duration, int id);
usbd_status usbd_set_protocol(usbd_interface_handle iface, int report);
```

These are SDK-side HID request helpers built on the OS USB syscall layer, not
new kernel primitives. See [hardware APIs](hardware.md) for ownership and
transfer rules.

## Machine-control helpers

```c
halt();
bkpt();
```

`halt()` loops forever. `bkpt()` emits an ARM or Thumb breakpoint instruction.
They are debugging/control primitives, not portable application functions.
