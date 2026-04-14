# Rewrite TODO

## Project
- [ ] Upgrade app licence in the app to MIT
- [ ] Set windows binary details (description, name, version, copyright, etc.)

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

## Sub/Function Browser

- [ ] Sub/Function browser. Parse SUB/FUNCTION declarations, list with jump-to-definition.

## Dialogs

- [ ] Format dialog. Keyword case conversion tool.
- [ ] About dialog. Version and license info.

## Help

- [ ] Help. Context-sensitive .chm keyword lookup.
- [x] QuickKeys viewer. Display quickkeys.txt.
- [x] ReadMe viewer. Display readme.txt.

## Application

- [ ] New Window. Launch another fbide.exe instance.
- [ ] Single-instance handling. Reuse existing window when launched again.
