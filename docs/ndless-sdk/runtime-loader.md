# Ndless Runtime and Loader

This document covers the calculator-side half of the system in the separate
`ndless` repository. It is included because SDK behavior depends on it.

## Runtime components

The resident resources include:

- The SWI handler and syscall extension table.
- OS-version selection and address matrix use.
- Program loading and format dispatch.
- OS document-open and startup hooks.
- LCD compatibility for new hardware.
- Lua interpreter hooks and native module loading.
- Emulator allocation hooks.
- Optional persistent-mode hooks.

Installer directories contain OS-generation-specific bootstrap payloads. They
are not part of the ordinary application build and must not be treated as a
portable exploit framework.

## Installing the resident runtime

The exact installer depends on calculator family, CAS status, hardware, and OS
build. The general path is:

1. Identify the exact OS version and model.
2. Select the matching installer artifact.
3. Place the required resources and configuration files in the expected
   documents location.
4. Execute the installer document.
5. Let the installer load `ndless_resources.tns`.
6. Verify the runtime revision and hardware queries from a native program.

The installers use OS-specific vulnerabilities or boot/persistence mechanisms.
Installation can alter OS hooks, startup behavior, and document-save behavior.
Back up documents first.

## SWI handler setup

The resident resources install an ARM SWI handler into the OS's exception-vector
copy and internal handler pointer. The handler:

1. Saves the caller's registers and saved processor state.
2. Detects ARM versus Thumb caller state.
3. Reads the SWI immediate from the caller instruction.
4. Separates extension/emulator/variable flags from the syscall index.
5. Selects an extension table or OS-version address row.
6. Returns an OS-variable address for `ISVAR`, or calls the selected function.
7. Restores processor state and returns to the caller.

The handler has limited reentrancy support. Extension functions should not
assume they can call arbitrary other syscalls from inside the handler.

The runtime also exposes an N-ext descriptor beginning with a `NEXT` signature
so programs and tools can detect the installed extension.

## OS identification

The runtime reads an OS identity value and selects one of the ordered rows used
by `syscall-addrs.h`. The rows cover OS/model combinations from the 3.x era
through current 6.x entries in the repository, but recognition does not imply
that an installer or every API is available.

Public queries include:

```c
unsigned nl_ndless_rev(void);
unsigned nl_hwtype(void);
unsigned nl_hwsubtype(void);
unsigned nl_osvalue(const unsigned *values, unsigned size);
unsigned nl_osid(void);
BOOL _nl_hassyscall(int nr);
BOOL nl_isstartup(void);
BOOL nl_loaded_by_3rd_party_loader(void);
```

Use `nl_osid` for diagnostics and `nl_osvalue` only with arrays deliberately
ordered for the runtime's OS table. Do not use the numeric ID as a stable model
identifier outside the matching runtime revision.

## Program loading

The loader's `ld_exec_with_args` path:

- Expands the current task stack when the OS row supports it.
- Applies file association rules from `ndless.cfg.tns`.
- Reads the file magic.
- Loads PRG, Zehn, or legacy bFLT formats.
- Applies hardware/version metadata restrictions.
- Masks appropriate interrupts and waits for key release.
- Saves the screen and configures the display.
- Calls the image entry point.
- Restores display and interrupt state.
- Frees the image unless resident.

The loader recognizes:

```text
PRG\0   Legacy wrapper; may contain an embedded Zehn file
bFLT    Historical flat binary format
Zehn    Current relocatable container
```

The PRG path scans only the first 20 KiB for embedded Zehn content. This is why
`make-prg` places the small compatibility loader before the Zehn payload.

## Metadata enforcement

The Zehn loader checks minimum/maximum Ndless version and revision and hardware
flags. It can reject programs that require color, clickpad, touchpad, 32 MB
compatibility, HW-W support, or a specific LCD API. Unknown flags are ignored;
unknown relocation types are fatal.

## Display compatibility

The new LCD extensions are implemented by the runtime. For HW-W and related
hardware, the runtime can rotate/copy application buffers into the physical
panel format. It also contains an old-program compatibility path that remaps
the LCD controller and emulates certain access patterns through a data-abort
handler.

This compatibility code is OS/hardware-specific and depends on controller,
translation-table, SPI, and mirror-buffer addresses. Application code should
use `lcd_blit`, not reproduce the compatibility layer.

## Hooks

The loader hooks OS document-open paths to recognize native files and startup
paths to execute files under the Ndless startup directory. The SDK hook macros
are lower-level and are used by runtime code to patch exact OS functions.

Hook installation overwrites two ARM instructions with an indirect branch. The
original instructions are saved in generated storage and replayed on return.
Overwritten instructions must not contain unsafe relative accesses, and hooks
must use the required ARM calling/return macros.

## Lua integration

The runtime hooks Lua interpreter startup and shutdown to capture the active
`lua_State *`, register Ndless functions, and keep native `.luax.tns` modules
resident. Modules are released when the interpreter shuts down. See
[hardware APIs](hardware.md) for the application-facing rules.

## Persistent mode

Persistent mode uses the OS's saved-current-document mechanism to reload a
small Ndless loader on boot. It installs hooks that suppress or alter snapshot
save/clear behavior. This can change normal document-save behavior and is
intended for specific CX II-era OS builds.

Treat persistent mode as opt-in and potentially destructive. Uninstall and
recovery paths are OS-specific; preserve backups and test the ordinary runtime
before attempting persistence.

## Resident memory and emulator allocation

The runtime normally uses heap/executable-memory allocation for program images.
Emulator mode can use a fixed debug allocation area so a debugger sees stable
addresses. The `NDLSEMU_DEBUG_ALLOC` and `NDLSEMU_DEBUG_FREE` entries coordinate
that behavior. Applications should not call these directly.
