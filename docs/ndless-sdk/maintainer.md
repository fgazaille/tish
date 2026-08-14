# Maintainer and Reverse-Engineering Notes

This chapter is for people extending Ndless or adding an OS/model. It is not a
normal application-development guide.

## Adding a syscall

For a new standard OS syscall:

1. Append it after the current last standard ID in `include/syscall-list.h`.
2. Never alter an existing numeric value.
3. Add a parseable signature comment if a generated wrapper is intended.
4. Increment `__SYSCALLS_LAST`.
5. Add the function address to every relevant OS row generated for the runtime.
6. Regenerate `syscall-decls.h` and `stubs.cpp` through `mkStubs.php`.
7. Build and inspect the generated declaration and wrapper.
8. Add availability checks and a sample/test where practical.

Do not reorder the list to make it attractive. Its historical order is the
ABI.

## Updating address tables

`include/syscall-addrs.h` is generated from OS analysis data, not handwritten
application code. The matching runtime table must use the same row ordering.
For every new OS/model row record:

- Exact OS version/build.
- Calculator model and CAS status.
- Hardware family/subtype.
- Source OS image or IDA database.
- Function/data address provenance.
- Whether an address is confirmed on hardware.
- Whether a zero address means unsupported or simply unmeasured.

One stale address can crash the OS or corrupt a program. A nonzero address does
not prove that a function's reconstructed signature is correct.

## Generated declaration caveats

`mkStubs.php` parses comments with a regular expression. It emits:

- Plain C wrappers for up to four fixed arguments.
- Naked wrappers for more than four arguments or varargs.

Malformed comments, omitted signatures, historical typos, and varargs affect
what gets generated. Always inspect both `stubs.cpp` and `syscall-decls.h` after
changing the registry.

## Adding an OS version

A complete runtime addition can involve all of the following:

```text
syscall-addrs.h generation inputs
OS identity detection
syscall table selection
program-loader hook address
startup hook address
task-stack address
LCD controller/mirror addresses
touchpad/keypad compatibility
Lua interpreter hooks
watchdog/reboot behavior
installer exploit or bootstrap
persistent-mode patches
```

Runtime recognition, installer support, and application execution support must
be tested independently. Update the compatibility matrix only from source,
not from a filename or an assumed version pattern.

## Hook maintenance

The `hook.h` macros overwrite eight bytes at a target. Before installing a hook:

- Confirm the target address for the exact OS row.
- Disassemble at least the overwritten instructions.
- Confirm they are ARM instructions when the hook requires ARM.
- Confirm they contain no PC-relative branch or load that cannot be replayed.
- Provide a non-reentrant hook body.
- Preserve and restore registers using the supplied macros.
- Call `clear_cache()` after patching.
- Test uninstall and abnormal exit paths.

The hook macros are not a substitute for an ABI-stable callback registration
system.

## Direct hardware maintenance

For each direct register access record:

- Model/hardware family.
- Register address and bit meaning.
- Whether the register is read/write or write-only.
- Required interrupt masking.
- Required cache/TLB synchronization.
- State that must be restored on exit.
- Failure behavior if the value is wrong.

The current libndls sources are the best reference for classic/CX keypad,
timer, LCD, interrupt, and CPU-clock differences. Keep comments next to the
access; do not centralize undocumented addresses into a generic abstraction
that hides their provenance.

## LCD API markers

The linker script retains `.genzehn`. The old API emits an old marker and the
new `lcd_blit.cpp` implementation emits a new marker. `genzehn` uses these
symbols to set metadata. If a new display path is added, update:

- The public format enum.
- `lcd_init`/`lcd_blit` behavior.
- Runtime extension implementation.
- Zehn marker detection.
- Metadata documentation.
- HW-W and old-program compatibility.

## Testing strategy

At minimum, test each change at three levels:

1. **Host/build**: regenerate stubs, compile affected libraries, run
   `genzehn --info`, and verify no generated diff is accidental.
2. **Emulator/runtime**: exercise syscall dispatch and memory/format behavior
   under the available emulator/debug path.
3. **Hardware**: test the exact OS/model rows affected, including abnormal
   exit, screen restoration, key release, and loader cleanup.

Useful diagnostics include:

```c
nl_ndless_rev();
nl_osid();
nl_hwtype();
nl_hwsubtype();
nl_hassyscall(e_some_syscall);
nl_loaded_by_3rd_party_loader();
```

`genzehn --info --input program.zehn` verifies the packaging layer but cannot
prove that a runtime address, hook, or hardware path is correct.

## Documentation policy

Every new low-level interface should document:

- Header and library.
- Ownership and lifetime.
- Return and error conventions.
- OS/model/revision availability.
- Interrupt/thread/reentrancy restrictions.
- Whether it is public, compatibility, reverse-engineered, unsafe, or
  third-party.
- A source path and a sample where possible.

Never describe a reverse-engineered address as a stable TI API. Never merge a
third-party API's documentation into Ndless's own API contract.
