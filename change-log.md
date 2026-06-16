# Changes since 0.5.0-rc.6

- Added a Windows installer with FreeBASIC for x86
- Added an arm64 (aarch64) Linux AppImage.
- Added an arm64 Windows build.
- Added file associations (.bas, .bi .fbs) for Linux and Windows

- Added auto-reload of externally modified documents.
- Added auto-refresh to the file browser which monitors for filesystem changes.
- Added a context menus to the file browser.
- Added a folder focus mode to the file browse.
- Changed sessions are now loaded and stay active until fbide quits or session is closed.
- Changed sessions now auto saves when quitting fbide or closing the session.
- Added file browser state to sessions: selection, expanded folders, focused folder and active sidebar tab are restored on load.
- Added a `fbide format <file>` command that formats a file from the command line (re-indent, re-format, case and HTML options) to stdout or an output file.
- Added a new app icon, splash and distinct file icons for .bas, .bi and .fbs files.
- Redesigned the About dialog.
- Statically linked the CRT into the x86/x64 Windows builds so they run without the VC++ redistributable.
- Fixed an operator (e.g. `,`) before a `&h`/`&o`/`&b` number swallowing its prefix and mis-highlighting the number (#111).
- Fixed a crash on startup when the file browser's tree fired a selection change during its own construction.
- Fixed a filesystem-watcher assertion on startup when opening a `.fbs` session from the command line.
- Fixed `_` in a `##_##` preprocessor token-paste being mis-lexed as a line continuation (#115).
- Added opening common extensionless files (Makefile, README, LICENSE, …) directly in fbide from the file browser (#114).
- Changed the Open dialog's default filter to FBIde (`*.bas`, `*.bi`, `*.fbs`) so session files load from the standard Open dialog.
- Removed "Load Session" from the File menu — open a `.fbs` via the normal Open dialog instead.
- Added native file/folder icons to the macOS file browser, replacing the generic monotone icons.
- Fixed Comment/Uncomment changing the text selection — the selection (or caret) is now preserved relative to the edited text (#113).
- Fixed a keyword right after `.`/`->` followed by a non-identifier (e.g. `->(byref`) losing its highlighting (#112).
- Fixed reopening the already-active session file reloading it from disk; it's now a no-op.
- Added an editor notification bar when a file fails to save (e.g. a read-only file), showing the OS reason when available.
- Fixed "Show in Browser" not revealing a file located outside the file browser's focused folder; it now unfocuses first.
- Added highlighting of every occurrence of the identifier under the caret or selection (toggle in Settings → General; colours editable in the theme editor).
- Added highlighting of matching scope keywords under the caret — opener/closer pairs (For/Next, Sub/End Sub, Do/Loop, …), If…ElseIf…Else…End If and Select Case…End Select groups, and Return with its enclosing Sub/Function.

# Changes since 0.5.0-rc.5

- Fixed status bar getting stuck on a stale compile message (#106).
- Fixed crash when quitting from the macOS dock menu (#107).
- Fixed minimap not using the theme's change-marker colours until the Settings dialogue was opened.
- Fixed BOM-marked files with invalid bytes being reinterpreted as Latin-1 and losing their BOM on save.
- Fixed on-type case conversion, occasionally altering a keyword just past a pasted block.
- Optimised single-line lexing (e.g. auto-indent on Enter) to not allocate whole-document capacity in large files.
- Fixed compiler log mangling output that contains square-bracket markup (e.g. `[b]`).
- Fixed potential crash when quitting during an in-flight update check.
- Fixed Find/Replace dialogues stacking up when reopened; the open one is now raised instead.
- Fixed crash when a second instance forwards a file during the splash screen.
- Fixed Settings partially applying (and restarting for a language change) when another tab had an invalid value.
- Fixed crash when re-selecting a newly saved theme in Settings.
- Fixed potential crash on multi-line edits.
- Fixed properly to clean up async tasks when fbide quits.
- Optimised multi-line edits to avoid re-lexing the document from the beginning.

# Changes since 0.5.0-rc.4

- Fixed crash on linux and macOS after compiling a file (#104)

# Changes since 0.5.0-rc.3

- Added multiple compiler configurations.
- Added configuration selector to toolbar or statusbar (configurable).
- Added compiler auto detection.
- Added include file resolution to include "-i" from compile command.
- Added run command to the log.
- Added version checker, which shows a message box if there is a new version of fbide.
- Fixed indent issue with indenting when "operator" and "property" are used as expressions (#94).
- Fixed console min height (#95).

# Changes since 0.5.0-rc.2

- Added change tracking (#1)
- Added a confirm-and-close prompt when Save As targets a file already open in another tab
- Added basic Markdown syntax highlighting for `.md` / `.markdown` files
- Added basic Windows batch syntax highlighting for `.bat` / `.cmd` files
- Added basic shell script syntax highlighting for `.sh` / `.bash` files
- Added basic Makefile syntax highlighting (`Makefile`, `GNUmakefile`, `.mk`, `.make`)
- Added basic JSON syntax highlighting for `.json` / `.json5` files
- Added basic CSS syntax highlighting for `.css` files
- Added a document type indicator to the status bar
- Added option to override document type (right click on type indicator in the status bar)
- Changed fbide now stores config changes in `.local.`, keeping original base files immutable.
- Fixed `#include` opening the same file in multiple tabs on case-insensitive filesystems (#87)
- Fixed issue with folding where the opener was followed by empty lines.

# Changes since 0.5.0-rc.1

- Added editor minimap — toggle via View → Minimap (F6)
- Fixed perfomance issues with multiline edits (undo/redo/index, etc.)
- Fixed unsaved FBIDETEMP location to be fbide cwd (#70)
- Fixed "Cmd prompt" command, which messed with fbide cwd
- Fixed symbol browser not listing Sub/Function declarations with `Private`/`Public`/`Protected` modifiers (#74)
- Changed symbol browser to show the qualified name for methods (`Type.Method`)
- Added constructors, destructors, operators and properties to the symbol browser
- Changed symbol browser to group UDT methods (subs, functions, constructors, …) under their owning type
- Added a search box to the symbol browser to filter results live by name, symbol type or UDT
- Added auto-indent and closer insertion for preprocessor blocks (#72)
- Changed symbol browser to list symbols declared inside `#if`/`#ifdef`/`#ifndef` blocks (#73)
- Fixed several issues around folding (#55)

# Changes since 0.5.0-beta.3

- Added new cobalt theme (Gothon)
- New styles for preprocessor identifier, number, operator and strings
- Changed theme settings to use a tree layout for an easier overview
- Fixed wstring and zstring classification, moved them to type keywords (#47)
- Fixed detection when a compound statement closer is not needed (#50)
- Fixed avoid de-indenting compound statement closers beyond opener level (#48)
- Fixed single-line `asm` statements no longer treated as multi-line block openers (#46)
- Fixed auto-indent and code formatter no longer add `End Asm` closer / indent body for single-line `asm` (#46)
- Fixed preprocessor lexing of `"_"` which would cause `_` to be treated as a line continuation (#54)
- Changed IDE resource directory carrying a `READONLY` sentinel file is now mirrored to `<user-data-dir>/ide` on launch and loaded/saved from there. (#39)
- Added Linux AppImage to the release pipeline; `-DBUILD_APPIMAGE=ON` switches the install layout to FHS and points the resource resolver at `share/fbide/ide/` (#66)
- Changed log now writes to the per-user data directory by default and flushes after each record, so crash diagnostics survive; added `--log-path` to override the location
- Changed `<$terminal>` placeholder is now read from `compiler.terminal` config(#45)
- Added run and compile command placeholder table.

# Changes since 0.5.0-beta.2

- Fixed issue with auto-indent not working when keyword case transform was disabled (#24)
- Fixed issue with REM comment followed by newline, which caused next line to be a comment (#28)
- Fixed various spelling errors (#26)
- Fixed bug with console not being properly updated after compile-generated output (#32)
- Fixed bug with indenting #defines containing IF keyword, which was counted as ppIF (#34)
- Fixed toolbar tooltip to show long help string (#36)
- Fixed case transform behaviour when moving cursor
- Changed auto indenter actions are now undoable (#37)
- Added show current document path in the title bar (#38)
- Renamed "Sub/Function browser" to "Symbol browser" across all locales
- Added Spanish translation (Joseba Epalza)

# Change since 0.5.0-beta.1

- Improved dark mode support on Windows. Can be enabled manually by editing the config file
- Toolbar now uses a new method to render, allowing it to be moved around, docked to the sides, etc. (#11)
- Remember the configured layout between FBIDE restarts
- Encode correct platform values in Windows manifest files
- The font now falls back to the system default monospace when the configured face isn't installed (#12)
- Pressing `F2` now toggles the Browser pane; `Shift+F2` opens the Sub/Function browser (#9)
- Removed old version from splash image (pauldoe)
- Session files are now persisted in the "Recent Files" menu, allowing quick access. (#16)
- Sessions now persist code folding state (#10)
- Fix issue with compile log not populating properly (#6, #7)
- Right-clicking on the console now opens the compile log (#5)
- Fixed issue with default background colour for margins, and fixed classic theme (#4)
- Support drag & drop files, only files supported by fbide are opened (#21)

# Changes since fbide 0.4.5.

### Project changes
- FBIde is completely rewritten from scratch, to match the feature set of fbide 0.4.6.
- Updated to wxWidgets 3.3.
- Adopted C++23 as baseline.
- Adopted the CMake build system.
- Relicensed under MIT

### General
- Added option to start the editor with a custom config file via `--config` option, or setting ide path with `--ide`
- Line numbers margin now zooms with font size changes
- FBIde validates chm files, if they are locked, show a dialogue with instructions and a link for more info
- If no local CHM file is found, or when on a non-Windows platform, open the FreeBASIC wiki page instead
- Added More keyword groups in settings
- Added ASM syntax highlighting
- Simplified Theme settings
- Added config file for shortcuts
- Added Config file for menu and toolbar layouts
- Added useful links in the Help menu
- Added support for INI (and other fbide config files) files
- Added support for different file encodings and line endings
- Updated session files to store some file properties like encoding and line endings
- Changed session to store relative paths when possible
- Added support for case transform of keywords
- Improved auto-indent by automatically adding closing keywords. After DO, add LOOP automatically.
- Reorganised keyword groups.
- Sub/Function browser now also shows included files, clicking on which opens the file
- Sub/Function browser now also shows macros
- Added dropdown menu option for reloading file from disk

### Removed options
- `ActivePath` option has been removed. FBIde now always sets the working directory to the source file location when compiling or running.
- `floatbars` option has been removed. FBIde now always uses wxAUI for layout.

### Layout
- FBIde uses wxAUI layout for tabs and panels. They can be moved around, docked in various positions, etc.

### Compiler
- Compilation is now asynchronous in a separate process.
- Compiler log is now a single persistent window that updates automatically after each compilation.
- Compile command paths are quoted by default: `"<$fbc>" "<$file>"`.
- Run command paths are quoted by default: `<$terminal> "<$file>" <$param>`.
- Added toolbar button to kill running process
