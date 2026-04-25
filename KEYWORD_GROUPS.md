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
