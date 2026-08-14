# Syscall ABI and Generated Interfaces

## Stable numeric registry

`include/syscall-list.h` is the canonical registry. Standard entries occupy
IDs `0` through `342`. The values are ABI, not an implementation detail:

- Never renumber an existing entry.
- Never reuse a deleted value.
- Append new standard entries at the end.
- Keep `__SYSCALLS_LAST` synchronized with the last value.
- Keep extension and emulator ordering synchronized with their runtime tables.

The comments on registry lines are also input to `libsyscalls/mkStubs.php`.
Entries without a parseable signature comment may not get an automatic C
wrapper, even though their numeric value remains reserved.

## SWI encoding

The normal call path emits an ARM software interrupt:

```asm
swi #number
```

The number is an immediate embedded in the instruction. The Ndless handler
decodes it from the instruction at the saved return address. The top three bits
of the 24-bit field are flags:

```c
#define __SYSCALLS_ISEXT 0x200000
#define __SYSCALLS_ISEMU 0x400000
#define __SYSCALLS_ISVAR 0x800000
```

The remaining bits select a standard OS entry, extension entry, emulator entry,
or variable entry. `ISVAR` returns the address of an OS global rather than
calling a function. It is used for such values as `errno`, standard streams,
keypad type, and the global graphics context pointer.

The standard C calling convention is ARM AAPCS-like: arguments 1 through 4
are in `r0` through `r3`, later arguments are on the caller's stack, and the
return value is in `r0`.

## SDK call wrappers

`include/syscall.h` supplies:

```c
wa_syscall(nr)
wa_syscall1(nr, p1)
wa_syscall2(nr, p1, p2)
wa_syscall3(nr, p1, p2, p3)
wa_syscall4(nr, p1, p2, p3, p4)
```

and C++ templates:

```cpp
syscall<nr, return_type>()
syscall<nr, return_type>(p1)
syscall<nr, return_type>(p1, p2)
syscall<nr, return_type>(p1, p2, p3)
syscall<nr, return_type>(p1, p2, p3, p4)
```

The syscall number must be compile-time constant because the inline asm uses an
immediate constraint. The asm declares memory and register clobbers matching
the SWI handler's behavior.

## More than four arguments and varargs

`mkStubs.php` generates ordinary C wrappers for functions with at most four
arguments and no varargs. Functions with more arguments or `...` receive naked
assembly wrappers.

The naked wrapper is necessary because its prologue must not move the caller's
stack arguments. It also saves `lr` in a global ten-entry stack before issuing
the SWI. The OS handler uses `lr` while dispatching, so a naked wrapper cannot
leave the return address only in `lr`.

This is an implementation limit and a reason not to write arbitrary manual
naked stubs. If adding one, preserve the exact caller stack layout, preserve
the return value in `r0`, and ensure the wrapper's assembly matches the target
ARM mode.

## Stage 1 direct-call ABI

When `STAGE1` is defined, `syscall.h` exposes `syscall_addr<nr>()` and
`syscall_local<nr, Ret>()`. These select a direct function address from
`syscall_addrs[os_version_index][nr]` because the resident SWI handler is not
installed yet. This path is for installer code and is not the normal application
ABI.

## Ndless extension entries

The extension table is implemented by Ndless rather than the TI OS:

| Entry | Meaning |
|---|---|
| `e_nl_osvalue` | Select an OS-version-specific value from an array |
| `e_nl_relocdatab` | Historical relocation service |
| `e_nl_hwtype` | Classic versus color hardware family |
| `e_nl_isstartup` | Whether the current launch is startup execution |
| `e_nl_lua_getstate` | Borrowed active Lua state |
| `e_nl_set_resident` | Mark current program resident |
| `e_nl_ndless_rev` | Ndless revision |
| `e_nl_no_scr_redraw` | Control post-program screen redraw |
| `e_nl_loaded_by_3rd_party_loader` | Report third-party loader origin |
| `e_nl_hwsubtype` | CM/CX II subtype information |
| `e_nl_exec` | Execute another native program |
| `e_nl_osid` | OS/model table index |
| `e__nl_hassyscall` | Test whether a syscall is available |
| `e_nl_lcd_blit` | Resolve an LCD blitter for a format |
| `e_nl_lcd_type` | Query native LCD format |
| `e_nl_lcd_init` | Initialize or restore an LCD format |

Extension SWIs have values too large for the Thumb SWI immediate form. Code
that calls them must be built in ARM mode.

## Emulator entries

The emulator integration table contains:

```c
NDLSEMU_DEBUG_ALLOC
NDLSEMU_DEBUG_FREE
```

These are for emulator/debug allocation coordination, not ordinary application
code.

## Complete standard registry index

The following is a name index, not a replacement for the generated declarations
or upstream API documentation. Signatures are authoritative in
`syscall-list.h` and `syscall-decls.h`.

### IDs 0-97: C, filesystem, task, input, zlib, and dialogs

```text
0  fopen                 1  fread                 2  fwrite
3  fclose                4  fgets                 5  malloc
6  free                  7  memset                8  memcpy
9  memcmp               10  printf               11  sprintf
12 fprintf              13  ascii2utf16          14  TCT_Local_Control_Interrupts
15 mkdir                16  rmdir                 17  chdir
18 stat                 19  unlink                20  rename
21 TCC_Terminate_Task   22  puts                 23  NU_Get_First
24 NU_Get_Next          25  NU_Done               26  strcmp
27 strcpy               28  strncat               29  strlen
30 show_dialog_box2_    31  strrchr               32  _vsprintf
33 fseek                34  NU_Current_Dir       35  read_unaligned_longword
36 read_unaligned_word  37  strncpy               38  isalpha
39 isascii              40  isdigit               41  islower
42 isprint              43  isspace               44  isupper
45 isxdigit             46  tolower               47  atoi
48 atof                 49  calloc                50  realloc
51 strpbrk              52  fgetc                 53  NU_Set_Current_Dir
54 fputc                55  memmove               56  memrev
57 strchr                58  strncmp                59  keypad_type
60 freopen              61  errno_addr             62  toupper
63 strtod               64  strtol                65  ungetc
66 strerror              67  strcat                68  strstr
69 fflush               70  remove                71  stdin
72 stdout               73  stderr                74  ferror
75 touchpad_read        76  touchpad_write         77  adler32
78 crc32                79  crc32_combine          80  zlibVersion
81 zlibCompileFlags     82  deflateInit2_          83  deflate
84 deflateEnd           85  inflateInit2_          86  inflate
87 inflateEnd            88  TCC_Current_Task_Pointer 89 ftell
90 NU_Open              91  NU_Close               92  NU_Truncate
93 _show_msgbox_2b      94  _show_msgbox_3b        95  opendir
96 readdir              97  closedir
```

### IDs 98-197: Lua 5.1 auxiliary and core APIs

```text
98 luaL_register         99 luaL_checklstring      100 luaL_error
101 luaI_openlib         102 luaL_getmetafield      103 luaL_callmeta
104 luaL_typerror        105 luaL_argerror         106 luaL_optlstring
107 luaL_checknumber     108 luaL_optnumber        109 luaL_checkinteger
110 luaL_optinteger      111 luaL_checkstack       112 luaL_checktype
113 luaL_checkany        114 luaL_newmetatable     115 luaL_checkudata
116 luaL_where            117 luaL_checkoption      118 luaL_ref
119 luaL_unref            120 luaL_loadfile         121 luaL_loadbuffer
122 luaL_loadstring       123 luaL_newstate         124 luaL_gsub
125 luaL_findtable        126 luaL_buffinit         127 luaL_prepbuffer
128 luaL_addlstring       129 luaL_addstring        130 luaL_addvalue
131 luaL_pushresult       132 lua_newstate           133 lua_close
134 lua_newthread         135 lua_atpanic           136 lua_gettop
137 lua_settop             138 lua_pushvalue         139 lua_remove
140 lua_insert             141 lua_replace            142 lua_checkstack
143 lua_xmove              144 lua_isnumber           145 lua_isstring
146 lua_iscfunction        147 lua_isuserdata         148 lua_type
149 lua_typename           150 lua_equal              151 lua_rawequal
152 lua_lessthan           153 lua_tonumber           154 lua_tointeger
155 lua_toboolean          156 lua_tolstring          157 lua_objlen
158 lua_tocfunction        159 lua_touserdata         160 lua_tothread
161 lua_topointer          162 lua_pushnil             163 lua_pushnumber
164 lua_pushinteger        165 lua_pushlstring         166 lua_pushstring
167 lua_pushvfstring       168 lua_pushfstring         169 lua_pushcclosure
170 lua_pushboolean        171 lua_gettable            172 lua_getfield
173 lua_rawget              174 lua_rawgeti             175 lua_createtable
176 lua_newuserdata        177 lua_getmetatable        178 lua_getfenv
179 lua_settable            180 lua_setfield            181 lua_rawset
182 lua_rawseti             183 lua_setmetatable        184 lua_setfenv
185 lua_call                186 lua_pcall               187 lua_cpcall
188 lua_load                189 lua_dump                190 lua_yield
191 lua_resume              192 lua_status               193 lua_gc
194 lua_error               195 lua_next                 196 lua_concat
197 lua_getstack
```

`lua_pushvfstring` is reserved but explicitly omitted from generated wrappers
because a `va_list` is not treated as safely portable across this boundary.

### IDs 198-226: screen, text, dialogs, and OS strings

```text
198 refresh_homescr       199 refresh_docbrowser       200 strtok
201 utf162ascii           202 utf16_strlen             203 _show_1NumericInput
204 _show_2NumericInput   205 _show_msgUserInput       206 rand
207 srand                 208 strtoul                  209 string_new
210 string_free           211 string_to_ascii          212 string_lower
213 string_charAt         214 string_concat_utf16      215 string_set_ascii
216 string_set_utf16      217 string_indexOf_utf16     218 string_last_indexOf_utf16
219 string_compareTo_utf16 220 string_substring         221 string_erase
222 string_truncate       223 string_substring_utf16   224 string_insert_replace_utf16
225 string_insert_utf16   226 string_sprintf_utf16
```

The source comment for ID 223 contains the historical spelling
`string_subtrsing_utf16`; the registry symbol and generated declaration are
`string_substring_utf16`.

### IDs 227-260: USB and device services

```text
227 usbd_open_pipe        228 usbd_close_pipe           229 usbd_transfer
230 usbd_alloc_xfer       231 usbd_free_xfer            232 usbd_setup_xfer
233 usbd_setup_isoc_xfer  234 usbd_get_xfer_status      235 usbd_interface2endpoint_descriptor
236 usbd_abort_pipe       237 usbd_clear_endpoint_stall 238 usbd_endpoint_count
239 usbd_interface_count  240 usbd_interface2device_handle
241 usbd_device2interface_handle 242 usbd_pipe2device_handle 243 usbd_sync_transfer
244 usbd_open_pipe_intr   245 usbd_do_request           246 usbd_do_request_flags
247 usbd_do_request_flags_pipe 248 usbd_get_interface_descriptor
249 usbd_get_config_descriptor 250 usbd_get_device_descriptor
251 usbd_set_interface    252 usbd_get_interface         253 usbd_find_idesc
254 usbd_errstr           255 usbd_devinfo               256 usbd_get_quirks
257 usbd_get_endpoint_descriptor 258 usb_register_driver
259 device_get_softc      260 device_get_ivars
```

Several comments in this historical registry have incomplete or swapped
signatures. Use the generated declarations and exact header types when writing
code; do not infer a missing argument from a comment alone.

### IDs 261-296: events and NavNet

```text
261 get_event              262 send_key_event           263 send_click_event
264 send_pad_event         265 getcwd                   266 sscanf
267 TI_NN_SendKeyPress     268 TI_NN_IsNodeResponsive   269 TI_NN_NodeEnumDone
270 TI_NN_NodeEnumNext     271 TI_NN_GetConnMaxPktSize  272 TI_NN_Read
273 TI_NN_Write            274 TI_NN_StartService       275 TI_NN_StopService
276 TI_NN_Connect          277 TI_NN_Disconnect         278 TI_NN_NodeEnumInit
279 TI_NN_UnregisterNotifyCallback 280 TI_NN_RegisterNotifyCallback
281 TI_NN_InstallOS        282 TI_NN_GetNodeInfo        283 TI_NN_DestroyOperationHandle
284 TI_NN_CreateOperationHandle 285 TI_NN_GetNodeScreen    286 TI_NN_CopyFile
287 TI_NN_Rename           288 TI_NN_RmDir               289 TI_NN_MkDir
290 TI_NN_DeleteFile      291 TI_NN_GetFileAttributes  292 TI_NN_PutFile
293 TI_NN_DirEnumDone      294 TI_NN_DirEnumNext         295 TI_NN_DirEnumInit
296 TI_NN_GetFile
```

### IDs 297-335: documents, graphics, and formatted output

```text
297 get_documents_dir     298 gui_gc_global_GC_ptr     299 gui_gc_free
300 gui_gc_copy            301 gui_gc_begin              302 gui_gc_finish
303 gui_gc_clipRect        304 gui_gc_setColorRGB        305 gui_gc_setColor
306 gui_gc_setAlpha        307 gui_gc_setFont            308 gui_gc_getFont
309 gui_gc_setPen          310 gui_gc_setRegion          311 gui_gc_drawArc
312 gui_gc_drawIcon        313 gui_gc_drawSprite         314 gui_gc_drawLine
315 gui_gc_drawRect        316 gui_gc_drawString         317 gui_gc_drawPoly
318 gui_gc_fillArc         319 gui_gc_fillPoly           320 gui_gc_fillRect
321 gui_gc_fillGradient    322 gui_gc_drawImage          323 gui_gc_getStringWidth
324 gui_gc_getCharWidth    325 gui_gc_getStringSmallHeight 326 gui_gc_getCharHeight
327 gui_gc_getStringHeight 328 gui_gc_getFontHeight      329 gui_gc_getIconSize
330 gui_gc_blit_gc         331 gui_gc_blit_buffer         332 snprintf
333 _vprintf               334 _vfprintf                  335 _vsnprintf
```

### IDs 336-342: NAND, calculator expressions, and display

```text
336 read_nand              337 write_nand                 338 nand_erase_range
339 calc_cmd                340 get_res_string              341 disp_str
342 TI_MS_MathExprToStr
```

## OS-variable access

An `ISVAR` call returns an address. The caller must know whether that address
points directly to the object or to another pointer-valued object. Examples:

```cpp
unsigned char *keypad_type();
// The returned pointer addresses the OS keypad-type byte.

Gc gui_gc_global_GC();
// The implementation dereferences the ISVAR result once to get the GC.
```

`stdlib.cpp` uses the same pattern for `errno`, `stdin`, `stdout`, and
`stderr`. Treat the returned storage as OS-owned; do not free it or change it
unless the API explicitly documents writable access.

## OS-version-specific values

`nl_osvalue(values, size)` returns the array element for the current runtime OS
index. The `SYSCALL_CUSTOM` macro builds a function pointer from that result:

```c
static const unsigned addresses[] = { /* one value per OS row */ };
#define my_os_function SYSCALL_CUSTOM(addresses, int, const char *)
```

This is inherently OS-specific. The array ordering must match the runtime's
`syscall_addrs` ordering and must contain enough entries for every supported
index you intend to run on.
