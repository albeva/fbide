# Rewrite TODO

## Project
- [x] Upgrade app licence in the app to MIT
- [x] Set windows binary details (description, name, version, copyright, etc.)
- [x] Remove lexilla git submodule, instead copy only the headers we need directly into the project
- [x] Remove all unused langauge keys from both FBIde as well as language files
- [x] Add all new string literals in FBIde to Trans and language files
- [ ] Update translations

## Compiler Integration

- [x] Compile. Run fbc with command template, capture output.
- [x] Compile & Run. Compile then execute if successful.
- [x] Run. Execute previously compiled program.
- [x] Quick Run. Run without recompiling.
- [x] Error parsing. Regex extract file/line/errnum/message, populate console.
- [x] Clickable errors. Double-click console row to jump to source location.
- [x] Compiler log viewer. Rich text dialog showing full compiler output.
- [x] Compiler log: append system info (FBIde version, fbc version via `fbc --version`, OS description).
- [x] Compiler log: append "Generated executable: <path>" on successful compilation.
- [x] Parameters dialog. Text entry for runtime arguments.
- [x] Command prompt. Open cmd.exe or configured terminal.
- [x] ShowExitCode toggle. Display program exit code after run.
- [x] ActivePath toggle. Use source file directory as working directory.

## Dialogs

- [x] Format dialog. Keyword case conversion tool.
- [x] About dialog. Version and license info.
- [ ] Sub/Function browser. Parse SUB/FUNCTION declarations, list with jump-to-definition.

## Help

- [x] Help. Context-sensitive .chm keyword lookup.
- [x] QuickKeys viewer. Display quickkeys.txt.
- [x] ReadMe viewer. Display readme.txt.

## Application

- [x] New Window. Launch another fbide.exe instance.
- [x] Single-instance handling. Reuse existing window when launched again.

## Bugs

- [x] Open/save dialog file patterns that currently throw na error
- [x] "assert "GetEventHandler() == this" failed in ~wxWindowBase(): any pushed event handlers must have been removed" on find/replace dialog
- [x] Syntax highlighter matches identifiers as keywords after "."
- [x] Formatter doesn't handle multi line comments. /' /' '/ '/
- [x] Auto indent fails on single line IFs
- [x] In the formatter, "Open in the browser" button sometimes does not appear
- [x] Formatter: indent namespace ... end namespace
- [x] Formatter: fails to handle "EXIT DO" when nested in a block properly.
- [x] Recent files seem to jump around in the menu, when new one is added.
- [x] Resized dialog causes broken layout in theme panel.
- [x] Opening non-existing file seems to break document loading, leaving with unattached editor hanging.
- [x] When opening a file and then forwarding it via single instance, relative file path is not resolved.
- [ ] On linux, the wxStaticBoxSizer does not pad its content (wxGTK issue?)
- [ ] Saving new or old .fbt theme is broken.
- [x] When changing font size from large to small, seems editor line-height remains old size.
- [x] When file fails to load, Document creates Editor, which leaves hanging editor.

## Formatter

- [ ] Indent initializer lists { ... } when spread over multiple lines with line continuations
- [x] Add support for `' format off` and `' format on` regions to disale/enable formatting (or with `rem format`)
- [x] Support `EndIf` as a closing keyword, same as `end if`

# Editor

- [ ] Auto indent code as user types
- [ ] Open #include directives
- [x] Implement new FB scintilla lexer
- [x] Make editor fully utf-8 compatible (config/langauge files, the editor, save/load, etc.)
- [ ] Review all themes, update them for new settings.
- [x] Implement code folding

# General

- [x] Add stop/kill button that terminates running app/compile process
- [x] Do not clear clipboard when quitting FBIde
- [x] Use file pattern allowing all supported files to be open in file open dialog
- [x] Review & update all defined keywords to match latest version of FreeBASIC
- [x] Resolve compiler and helpfile to relative paths, if located in the same or sub folder as fbide binary OR the IDE/
- [ ] Use sensible, default strings and config options as defaults, in case underlying value is missing
