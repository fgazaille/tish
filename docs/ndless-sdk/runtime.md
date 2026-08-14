# Newlib, Filesystem, and C/C++ Runtime

## Two file APIs

Ndless exposes the underlying Nucleus file API and a newlib/POSIX-like bridge.

The Nucleus API uses opaque handles:

```c
NUC_FILE *nuc_fopen(const char *path, const char *mode);
size_t nuc_fread(void *ptr, size_t size, size_t count, NUC_FILE *file);
size_t nuc_fwrite(void *ptr, size_t size, size_t count, NUC_FILE *file);
int nuc_fclose(NUC_FILE *file);
NUC_DIR *nuc_opendir(const char *path);
struct nuc_dirent *nuc_readdir(NUC_DIR *dir);
int nuc_closedir(NUC_DIR *dir);
int nuc_stat(const char *path, struct nuc_stat *st);
```

`dirent.h` supplies thin `DIR`, `opendir`, `readdir`, and `closedir` aliases.
The `nuc_*` functions are generated syscall stubs and use the TI OS's native
path/structure conventions.

The newlib bridge maps normal C functions such as `fopen`, `read`, `write`,
`open`, `close`, `stat`, `unlink`, `rename`, `mkdir`, `rmdir`, `chdir`, and
`getcwd` onto the Nucleus API. Use one abstraction consistently in a given
operation; do not cast a newlib file descriptor to a `NUC_FILE *`.

## Newlib monitor initialization

`crt0.S` calls `initialise_monitor_handles()` before C++ constructors. The
`libsyscalls` implementation:

- Saves the original screen buffer pointer for abnormal-exit restoration.
- Obtains OS-owned `stdin`, `stdout`, and `stderr` through `ISVAR` entries.
- Initializes the fd table for descriptors 0, 1, and 2.
- Registers cleanup state with `atexit`.

The `crt0.S` weak fallback is a no-op for minimal programs that do not link the
full backend.

## File descriptor table

`stdlib.cpp` uses a fixed table of 20 entries. Descriptors 0-2 are the OS
standard streams. New descriptors are allocated from 3 upward. A full table
returns `EMFILE`; invalid descriptors return `EBADF`.

Standard streams must not be closed through `_close`; the backend rejects that
operation to avoid destroying the calculator's own handles.

`_open` translates flags into native mode strings. It handles `O_CREAT` and
`O_EXCL` with prechecks and always uses binary mode because the ARM newlib
configuration does not provide the desktop `O_BINARY` behavior automatically.

## Read/write behavior

`_read` uses `nuc_fread`. A zero-byte read is distinguished between EOF and an
error by probing `fgetc`, inspecting the native FILE flags, and putting a
non-EOF character back when needed. This is dependent on the current TI FILE
layout and is not portable.

`_write` uses `nuc_fwrite`. `_lseek` calls native seek and tell. `_fstat` and
`_link` are not implemented in the current backend. `_getpid` returns a fixed
value, while `_kill`, `_getentropy`, and `_times` are unsupported or stubbed.

`_gettimeofday` reads the calculator RTC at a fixed memory-mapped address and
reports zero microseconds. This is not a general POSIX clock implementation.

## `errno`

The OS owns its errno storage. `stdlib.cpp` accesses the OS errno address using
`e_errno_addr | __SYSCALLS_ISVAR`, then copies it into newlib's errno on error.
The implementation undefines newlib's errno macro locally to avoid recursion.

Do not assume that an arbitrary native OS syscall updates newlib errno unless
the wrapper explicitly calls the synchronization helper.

## Heap alignment

The OS allocator may not return the 8-byte alignment required by the C ABI. The
newlib backend allocates eight extra bytes, rounds the pointer up, and stores
the padding immediately before the user pointer:

```text
raw allocation ... padding byte | aligned user pointer ...
```

`free` recovers the raw pointer from that byte. `realloc` may move data when
the alignment padding changes. `calloc` checks multiplication overflow before
zeroing memory through the OS memset service.

Do not pass a pointer returned by the aligned `malloc` directly to a native OS
free function. Use the matching newlib `free`. Likewise, do not free a native
OS allocation with newlib `free`.

## Paths and `realpath`

`realpath.c` is a BSD-derived implementation added because the target newlib
does not supply one. It:

- Handles absolute and current-directory-relative paths.
- Removes duplicate separators and `.` components.
- Resolves `..` without climbing above the root.
- Stats intermediate components.
- Enforces `PATH_MAX`.
- Allocates a result when the caller passes `NULL`.

The result describes the Nucleus/virtual filesystem namespace, not a host path.
`get_documents_dir()` is preferred when the application needs the user
documents area.

## Directory structures

`nucleus.h` defines a partial `struct nuc_stat` and a one-character
`struct nuc_dirent` name field because the OS uses variable-sized directory
records. The exact data layout is OS-facing and partially reverse-engineered.
Use the native syscall functions rather than constructing these structures by
hand.

## C++ initialization and termination

The SDK's startup sequence runs global constructors from a linker-ordered
`.init_array` list and global destructors from `.fini_array`. `exit()` executes
registered `atexit` functions, then the backend reclaims newlib state and weakly
linked libstdc++ cleanup where available.

The abnormal path uses `abort()` to restore the display and show a message box.
The implementation optionally demangles an active C++ exception if the weak
runtime symbols are linked.

SDK library builds disable RTTI and exceptions, but an application can link
additional C++ runtime features only if the resulting code fits the target and
the startup/backend assumptions remain valid.

## Nspire I/O variant

`libsyscalls_nspireio.a` replaces standard stream behavior with the Nspire I/O
console. It initializes a default `nio_console` early, routes writes through
console functions, and frees the console during exit. Select it through the
linker's `--nspireio` option.

This variant is not a transparent desktop terminal. Its screen dimensions,
color support, input behavior, and cleanup are those of the bundled Nspire I/O
implementation.

## Unsupported hosted assumptions

Do not assume the target provides:

- `fork`, processes, signals, or a conventional PID model.
- A normal filesystem mount table or symlink implementation.
- POSIX permissions or ownership semantics.
- Dynamic loading or shared libraries.
- Thread-local errno or thread-safe global libc state.
- Desktop terminal behavior.
- Arbitrary stack growth from the application itself.

The Ndless loader may expand a task stack internally on supported OS rows, but
applications must still avoid unbounded stack use.
