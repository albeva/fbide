# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FBIde rewrite — clean modern C++/wxWidgets reimplementation of [the original FBIde](https://github.com/albeva/fbide/tree/classic_0.4) from the `classic_0.4` branch. FBIde is an open-source, cross-platform IDE for the FreeBASIC compiler. Goal: feature parity with modern C++ architecture.

## Build

FBIde builds on Windows, macOS and Linux. Keep instructions, scripts and code
platform-neutral; when something is genuinely platform-specific, say which
platforms it applies to.

Requirements: C++23 compiler (MSVC 19.40+, recent Clang or GCC), CMake 4.0+,
Ninja, and a static wxWidgets 3.3.2+ install. Point `WXWIN` at the wxWidgets
install prefix if CMake cannot find it on its own. Use whatever compiler is the
default on the host; pass `-DCMAKE_CXX_COMPILER=...` only if the local setup
needs it (e.g. a non-default Clang on macOS).

Debug:
```bash
cmake -B build/claude/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/claude/debug
```

Release:
```bash
cmake -B build/claude/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/claude/release
```

On Windows, configure from a Visual Studio developer shell so Ninja picks up MSVC.

### Run tests
Unit tests (`tests`) and UI tests (`ui-tests`) are registered with CTest:
```bash
ctest --test-dir build/claude/debug --output-on-failure
```
The binaries can also be run directly from `build/claude/debug/tests/`
(`tests`/`ui-tests`, with `.exe` on Windows).

## Reference Implementation

The original FBIde 0.4 source is not part of this checkout (see the link above). Key characteristics of the original:
- ~3500 LOC across ~20 source files, C++17, wxWidgets 3.3, CMake, Scintilla (wxSTC) editor
- Everything lives in one monolithic `FBIdeMainFrame` class (~100 public members, all state as public fields)
- Language files (`.fbl`) use numeric indexes (`0=Welcome`, `234=Active paths`) — very fragile
- Theme files (`.fbt`) and settings (`.ini`) use wxFileConfig INI format
- Single-instance handling via `wxSingleInstanceChecker` + IPC

## Features to Reimplement

### Editor Core
- Scintilla-based editor (wxSTC) with tabbed interface (custom `wxMyNotebook`)
- Buffer management: multiple open files with document pointer swapping (single STC instance, multiple documents)
- FreeBASIC syntax highlighting (VB lexer), HTML highlighting, plain text mode
- 4 keyword groups for syntax highlighting loaded from `.lng` files
- Smart auto-indent for FreeBASIC (sub/function/if/for/while/do/select/type/enum/union/scope/asm blocks)
- Code folding, brace matching, line numbers, indent guides, edge column, whitespace display

### File Operations
- New, open (multi-select), save, save-as, save-all, close, close-all
- Session save/load (`.fbs` files — stores open files, cursor positions, scroll positions)
- File history (recent files)
- Modified file tracking with `[*]` prefix in tab titles

### Compiler Integration
- Compile, run, compile-and-run, quick-run (temp file compile+execute)
- Configurable compiler command prototype with meta-tags: `<$fbc>`, `<$file>`, `<$file_path>`, `<$file_name>`, `<$file_ext>`, `<$param>`, `<$terminal>`
- Error parsing from fbc output (file/line/error extraction) displayed in console list
- Click-to-navigate errors, compiler log viewer with `[bold]` markup
- Active path: set working directory to source file location
- Async process execution with toolbar/menu state management

### Search
- Find, replace, find next/previous, find current word forward/backward
- Replace all (with undo grouping)
- Goto line (supports `line:col` format and `e` for end)

### Edit Operations
- Undo/redo, cut/copy/paste, select all, select line
- Indent/unindent blocks
- Comment/uncomment blocks (FreeBASIC `'` and `REM` style)

### Dialogs
- Settings: general, themes, keywords, compiler tabs
- Sub/Function browser — parses current document for subs/functions
- Format dialog (code formatting)
- About dialog

### Other
- Single instance: second launch sends filename to running instance via IPC
- Splash screen
- CHM help integration (Windows)
- Ctrl+1-9 tab switching, Ctrl+Tab/Shift+Tab cycling
- Hotspot click on `#include` preprocessor lines to open referenced files

## Legacy File Formats (must support import)

### Theme files (`.fbt`)
INI format with sections: `[default]`, `[linenumber]`, `[select]`, `[brace]`, `[badbrace]`, `[comment]`, `[number]`, `[keyword]`, `[string]`, `[preprocessor]`, `[operator]`, `[identifier]`, `[date]`, `[stringeol]`, `[keyword2-4]`, `[constant]`, `[asm]`. Colors stored as decimal integers (e.g. `16777215` = white). Bundled themes: classic, borland, matrix, obsidian, ocean, original, twilight.

### Language files (`.fbl`)
INI format under `[FBIde]` section. Keys are numeric indexes (`0` through `244`), values are UI strings. 15+ languages exist. Format is terrible — rewrite should use named keys but provide import from indexed format.

### Settings (`.ini`)
wxFileConfig INI with sections: `[general]`, `[paths]`, `[compiler]`, `[editor]`. Platform-specific files: `prefs_win32.ini`, `prefs_linux.ini`.

### Keyword files (`.lng`)
INI with `[keywords]` section containing `kw1`-`kw4` (space-separated keyword lists).

### Session files (`.fbs`)
Plain text. Version 0.2 format: header line, selected tab index, then repeating (filepath, scroll-line, caret-pos) triples.

## Architecture Goals for Rewrite

- Proper separation of concerns (no god-class)
- Modern C++23
- Use JSON or TOML for new config format (with legacy importers)
- Named keys for translations instead of numeric indexes
- Proper event handling (no raw macros)
- Resource management with RAII throughout
