# Nucleus and Compatibility Headers

## `nucleus.h`

`include/nucleus.h` is the low-level type and extension header used by the
generated syscall declarations. It does not expose a complete Nucleus RTOS
manual; it provides the subset of types and entry points needed by Ndless.

## Basic types

```c
```

`NU_TASK`, `NUC_FILE`, and `NUC_DIR` are opaque. Do not allocate or inspect
them as though they were complete public structs.

`OS_BASE_ADDRESS` is defined as `0x10000000` for runtime/loader code that must
refer to the OS image base. Application code should not assume that every
address relative to this value is mapped or safe.

## `String`

The OS string object is represented by a pointer to an incomplete layout:

```c
    char *str;
    int len;
    int chunk_size;
    int unknown_field;
} *String;
```

Use the OS string functions rather than changing fields directly:

```c
String string_new(void);
void string_free(String value);
char *string_to_ascii(String value);
int string_set_ascii(String value, const char *text);
int string_set_utf16(String value, const char *text);
int string_concat_utf16(String value, const char *text);
void string_lower(String value);
char string_charAt(String value, int index);
void string_erase(String value, int index);
void string_truncate(String value, int length);
```

The UTF-16 functions use the TI OS's string representation. The C declarations
use `char *` for historical reasons; this does not mean the data is UTF-8.
Returned character buffers may be OS-owned or temporary. Copy them when they
must outlive the operation.

## File and directory records

The native directory API uses:

```c
struct nuc_dirent {
    char d_name[1];
};
```

The record is effectively variable-sized. The name field must be consumed while
the directory entry remains valid and must not be treated as storage that the
caller owns.

The partial `nuc_stat` layout is:

```c
struct nuc_stat {
    unsigned short st_dev;
    unsigned int st_ino;
    unsigned int st_mode;
    unsigned short st_nlink;
    unsigned short st_uid;
    unsigned short st_gid;
    unsigned short st_rdev;
    unsigned int st_size;
    unsigned int st_atime;
    unsigned int st_mtime;
    unsigned int st_ctime;
};
```

Several fields have fixed or placeholder semantics in the current OS bridge.
Use `S_ISDIR(st_mode)` and the documented native wrappers rather than relying
on desktop POSIX ownership/time semantics.

`DSTAT` is a separate native enumeration record used by `NU_Get_First`,
`NU_Get_Next`, and `NU_Done`. Its path and attribute fields are partially
reverse-engineered; unknown fields must be preserved if a caller passes the
record back to the OS.

## OS event record

```c
struct s_ns_event {
    unsigned int timestamp;
    unsigned short type;
    unsigned short ascii;
    unsigned int key;
    unsigned int cursor_x;
    unsigned int cursor_y;
    unsigned int unknown;
    unsigned short modifiers;
    unsigned char click;
};
```

The current comments identify key-down as type `0x8`, key-up as `0x10`, and
possibly APD as `0x20`. Modifier values include Shift `3`, Ctrl `4`, and Caps
`0x10`; click is `8`. Treat unlisted values as OS-specific.

The event functions are:

```c
int get_event(struct s_ns_event *event);
void send_key_event(struct s_ns_event *event, unsigned short value,
                    BOOL a, BOOL b);
void send_click_event(struct s_ns_event *event, unsigned short value,
                      BOOL a, BOOL b);
void send_pad_event(struct s_ns_event *event, unsigned short value,
                    BOOL a, BOOL b);
```

These inject or consume OS events and should not be used to simulate arbitrary
input without understanding the current OS event loop.

## Resource identifiers

`e_resourceID` stores four-character resource IDs as ASCII packed integers:

```text
RES_CLNK RES_CTLG RES_DCOL RES_DLOG RES_DTST
RES_GEOG RES_MATH RES_MWIZ RES_NTPD RES_PGED
RES_QCKP RES_QUES RES_SCPD RES_SYST RES_TBLT
```

They are used by graphics icon lookup. Resource availability depends on the OS
resource bundle.

## Ndless extension functions

The public extension declarations are:

```c
unsigned int nl_ndless_rev(void);
unsigned int nl_hwtype(void);
unsigned int nl_hwsubtype(void);
BOOL nl_loaded_by_3rd_party_loader(void);
BOOL nl_isstartup(void);
BOOL _nl_hassyscall(int nr);
void nl_set_resident(void);
unsigned int nl_osvalue(const unsigned int *values, unsigned int size);
int nl_exec(const char *program, int argc, char **argv);
lua_State *nl_lua_getstate(void);
```

The values are provided by the resident runtime, not by the TI OS's ordinary
syscall table. Their meaning can change only with the matching Ndless runtime;
the C names alone do not make them available on a calculator without Ndless.

`_nl_hassyscall` checks a numeric standard syscall entry and its selected OS
address. It does not validate a caller's function signature or memory arguments.

## `SYSCALL_CUSTOM`

The macro binds a version-indexed address array to a function pointer:

```c
static const unsigned foo_addresses[] = {
    /* values in the runtime OS row order */
};

#define foo SYSCALL_CUSTOM(foo_addresses, int, const char *)
```

The resulting expression calls `nl_osvalue` and casts the selected address.
Use this only for deliberately reverse-engineered, version-specific functions.
The array must be long enough for every OS ID the program allows, and the
function signature must be correct for every row.

## NavNet opaque handles

The compatibility header forward-declares:

```c
typedef struct _nn_ch *nn_ch_t;
typedef struct _nn_nh *nn_nh_t;
typedef struct _nn_oh *nn_oh_t;
```

These are borrowed/owned according to the NavNet operation being performed.
Use `TI_NN_CreateOperationHandle`/`TI_NN_DestroyOperationHandle`, connect and
disconnect channel handles, and consult the separate NavNet documentation for
callback lifetime. Do not dereference the handles.

## `os.h`

`include/os.h` is an umbrella compatibility header. It includes standard C,
filesystem, math, Lua, Ndless, graphics, hook, and syscall headers. Its own
comment says it should no longer be used. New code should include the narrow
header it needs, especially `libndls.h`, `nucleus.h`, `syscall-decls.h`, or
`ngc.h`.

## `dirent.h`

`dirent.h` aliases `DIR` to `NUC_DIR`, maps `struct dirent` to
`struct nuc_dirent`, and supplies static wrappers:

```c
opendir(path)  -> nuc_opendir(path)
readdir(dir)  -> nuc_readdir(dir)
closedir(dir) -> nuc_closedir(dir)
```

It is a compatibility convenience, not a complete host dirent implementation.

## `memory.h`

This header only includes `<string.h>`. It exists for source compatibility with
code that expects a memory header; it does not introduce a separate allocator.

## `bsdcompat.h`

This header provides BSD-compatible aliases such as `u_int8_t`, `u_int16_t`,
`u_int32_t`, `u_int`, `u_char`, `u_long`, and `device_t`, plus a minimal
`struct clist`, `__packed`, and `ENXIO`. It exists primarily for the imported
USB stack. It is not a general BSD compatibility layer.
