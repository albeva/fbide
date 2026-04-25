# Keyword Group Re-categorisation

Working document for re-defining FreeBASIC keyword groups in `keywords.ini`.
Categorisation deferred — this only records the group definitions and what
belongs in each. Actual keyword lists assembled later.

## Groups

### `Keywords`

Main language built-in keywords. Control flow, declarations, scoping.

- Control flow: `if`, `then`, `else`, `elseif`, `endif`, `for`, `next`, `while`,
  `wend`, `do`, `loop`, `until`, `select`, `case`, `return`, `goto`, `gosub`,
  `exit`, `continue`
- Declaration: `declare`, `function`, `sub`, `class`, `type`, `enum`, `union`,
  `namespace`, `import`, `from`, `as`
- Variables: `dim`, `var`, `const`, `static`, `shared`, `redim`, `extern`,
  `common`
- Visibility / linkage: `private`, `public`, `protected`, `export`
- Other built-in language constructs.

### `KeywordTypes`

Built-in types and type casts/conversions.

- Types: `integer`, `long`, `longint`, `short`, `byte`, `ubyte`, `single`,
  `double`, `string`, `zstring`, `wstring`, `boolean`, `any`, `pointer`,
  `ptr`, etc.
- Casts / conversions: `cast`, `cbool`, `cint`, `clng`, `cdbl`, `csng`,
  `cbyte`, `cubyte`, `cstr`, `cptr`, etc.

### `KeywordOperators`

Keywords that act as operators (binary or unary).

- `and`, `or`, `not`, `xor`, `eqv`, `imp`, `andalso`, `orelse`
- `mod`, `shl`, `shr`
- `is`, `new`, `delete` (when used as operators)

### `KeywordConstants`

Built-in compiler defines / constants.

- Compiler-provided macros: `__LINE__`, `__FILE__`, `__FUNCTION__`, `__DATE__`,
  `__TIME__`, `__FB_VERSION__`, `__FB_MAIN__`, etc.
- Built-in literals: `TRUE`, `FALSE`, `NULL`

### `KeywordLibrary`

Standard library keywords that don't fit other categories. Built-in runtime
functions and statements.

- I/O: `print`, `input`, `cls`, `tab`, `spc`, `lprint`, `open`, `close`,
  `write`, `read`
- Graphics: `screen`, `pset`, `line`, `circle`, `paint`, `color`, `palette`,
  `draw`, `point`
- Time / system: `sleep`, `timer`, `time`, `date`, `shell`, `chdir`, `kill`
- Math: `abs`, `sqr`, `sin`, `cos`, `tan`, `atn`, `log`, `exp`, `int`, `fix`,
  `sgn`, `rnd`, `randomize`
- String: `len`, `mid`, `left`, `right`, `chr`, `asc`, `instr`, `lcase`,
  `ucase`, `ltrim`, `rtrim`, `trim`, `str`, `val`, `hex`, `oct`, `bin`

### `KeywordCustom`

User-defined keywords. Empty by default. Both slots
(`KeywordCustom1`, `KeywordCustom2`) for the user to populate.

### `KeywordPP`

Preprocessor directives. Word forms only — the leading `#` is matched by the
lexer separately.

- `include`, `define`, `undef`
- `ifdef`, `ifndef`, `if`, `else`, `elseif`, `elseifdef`, `elseifndef`, `endif`
- `macro`, `endmacro`
- `pragma`, `print`, `error`, `assert`
- `lang`, `inclib`, `libpath`, `line`, `cmdline`, `once`, `defined`

### `KeywordAsm1`

Valid only inside `asm ... end asm` blocks. Instruction mnemonics.

- Data movement: `mov`, `movzx`, `movsx`, `lea`, `xchg`, `push`, `pop`
- Arithmetic: `add`, `sub`, `mul`, `imul`, `div`, `idiv`, `inc`, `dec`, `neg`
- Logic: `and`, `or`, `xor`, `not`, `test`
- Shift / rotate: `shl`, `shr`, `sar`, `rol`, `ror`
- Control flow: `jmp`, `call`, `ret`, `je`, `jne`, `jg`, `jl`, `loop`, `int`
- Comparisons: `cmp`
- SSE / AVX, FPU, system instructions, etc.

### `KeywordLibrary` (new group)

Adding this group brings the total to 10 (9 currently). Decision needed:
- Add a 10th slot in `ThemeCategory::DEFINE_THEME_KEYWORD_GROUPS`, OR
- Collapse `KeywordCustom1`/`KeywordCustom2` into a single `KeywordCustom`
  to keep the count at 9.

### `KeywordAsm2`

Valid only inside `asm ... end asm` blocks. CPU registers.

- General purpose 64-bit: `rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi`, `rbp`,
  `rsp`, `r8`–`r15`
- General purpose 32/16/8-bit aliases: `eax`/`ax`/`al`/`ah`, etc.
- Segment: `cs`, `ds`, `es`, `fs`, `gs`, `ss`
- Special: `rip`/`eip`/`ip`, `rflags`/`eflags`/`flags`
- FPU stack: `st0`–`st7`
- SIMD: `mm0`–`mm7`, `xmm0`–`xmm31`, `ymm0`–`ymm31`, `zmm0`–`zmm31`, `k0`–`k7`
- Control / debug: `cr0`–`cr8`, `dr0`–`dr7`
- System: `gdtr`, `idtr`, `ldtr`, `tr`

---

## Final Categorisation

Source: 608 entries from FB-manual-1.10.1.chm Functional Keyword List,
classified per priority (keyword > type > operator > define > library);
PP / Asm contextual lists kept separate (overlap with code allowed).

### Keywords (98)

```
abstract alias as asm base byref byval call case cdecl class common const constructor continue data declare defbyte defdbl defint deflng deflongint defshort defsng defstr defubyte defuint defulongint defushort destructor dim do else elseif end endif enum erase error exit export extends extern fastcall for function gosub goto if iif implements import is let lib loop naked namespace next on operator option overload override pascal private property protected public read redim rem restore resume return scope select shared static stdcall step stop sub then this thiscall to type union until using var virtual wend while with wstring zstring
```

### KeywordTypes (37)

```
any boolean byte cast cbool cbyte cdbl cint clng clngint cptr cshort csign csng cubyte cuint culng culngint cunsg cushort double fbarray integer long longint object pointer ptr short single string ubyte uinteger ulong ulongint unsigned ushort
```

### KeywordOperators (28)

```
and and= andalso delete eqv eqv= imp imp= mod mod= new not offsetof or or= orelse procptr sadd shl shl= shr shr= sizeof strptr typeof varptr xor xor=
```

### KeywordConstants (75)

```
__date__ __date_iso__ __fb_64bit__ __fb_arg_count__ __fb_arg_extract__ __fb_arg_leftof__ __fb_arg_rightof__ __fb_argc__ __fb_argv__ __fb_arm__ __fb_asm__ __fb_backend__ __fb_bigendian__ __fb_build_date__ __fb_build_date_iso__ __fb_build_sha1__ __fb_cygwin__ __fb_darwin__ __fb_debug__ __fb_dos__ __fb_err__ __fb_eval__ __fb_fpmode__ __fb_fpu__ __fb_freebsd__ __fb_gcc__ __fb_gui__ __fb_iif__ __fb_join__ __fb_lang__ __fb_linux__ __fb_main__ __fb_min_version__ __fb_mt__ __fb_netbsd__ __fb_openbsd__ __fb_optimize__ __fb_option_byval__ __fb_option_dynamic__ __fb_option_escape__ __fb_option_explicit__ __fb_option_gosub__ __fb_option_private__ __fb_out_dll__ __fb_out_exe__ __fb_out_lib__ __fb_out_obj__ __fb_pcos__ __fb_ppc__ __fb_query_symbol__ __fb_quote__ __fb_signature__ __fb_sse__ __fb_uniqueid__ __fb_uniqueid_pop__ __fb_uniqueid_push__ __fb_unix__ __fb_unquote__ __fb_vectorize__ __fb_ver_major__ __fb_ver_minor__ __fb_ver_patch__ __fb_version__ __fb_win32__ __fb_x86__ __fb_xbox__ __file__ __file_nq__ __function__ __function_nq__ __line__ __path__ __time__ false true
```

### KeywordLibrary (258)

```
abs access acos add allocate alpha append arraylen arraysize asc asin assert assertwarn atan2 atn beep bin binary bit bitreset bitset bload bsave callocate chain chdir chr circle clear close cls color com command condbroadcast condcreate conddestroy condsignal condwait cos csrlin curdir custom cva_arg cva_copy cva_end cva_list cva_start cvd cvi cvl cvlongint cvs cvshort date dateadd datediff datepart dateserial datevalue day deallocate dir draw dylibfree dylibload dylibsymbol dynamic encoding environ eof erfn erl ermn err escape event exec exepath exp explicit fb_memcopy fb_memcopyclear fb_memmove field fileattr filecopy filedatetime fileexists fileflush filelen fileseteof fix flip format frac fre freefile get getjoystick getkey getmouse hex hibyte hiword hour imageconvertrow imagecreate imagedestroy imageinfo inkey inp input instr instrrev int isdate isredirected kill lbound lcase left len line lobyte loc local locate lock lof log loword lpos lprint lpt lset ltrim mid minute mkd mkdir mki mkl mklongint mks mkshort month monthname multikey mutexcreate mutexdestroy mutexlock mutexunlock name nogosub nokeyword now oct open out output paint palette pcopy peek pmap point pointcoord poke pos preserve preset print pset put random randomize reallocate reset rgb rgba right rmdir rnd rset rtrim run screen screencontrol screencopy screenevent screenglproc screeninfo screenlist screenlock screenptr screenres screenset screensync screenunlock second seek setdate setenviron setmouse settime sgn shell sin sleep space spc sqr stick str strig swap system tab tan threadcall threadcreate threaddetach threadself threadwait time timer timeserial timevalue trans trim ubound ucase unlock va_arg va_first va_next val valint vallng valuint valulng view wait wbin wchr weekday weekdayname whex width window windowtitle winput woct write wspace wstr year
```

### KeywordPP (25)

```
assert cmdline define defined else elseif elseifdef elseifndef endif endmacro error if ifdef ifndef inclib include lang libpath line macro once pragma print reserve undef
```
