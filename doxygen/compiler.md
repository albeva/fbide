# Compiler {#compiler}

The compiler subsystem owns FBIde's relationship with `fbc`: probing
the binary, building command lines, spawning the process, parsing
errors, and feeding the result back into the editor as clickable
output. It is asynchronous end-to-end — the UI never blocks on
`fbc`.

## Players

| Type                | Lifetime              | Job                                                                |
|---------------------|-----------------------|--------------------------------------------------------------------|
| `CompilerManager`   | Stable, owned by `Context`. | Public surface (compile / run / quickRun / kill / log / probe). |
| `BuildTask`         | One-shot, owned by `CompilerManager::m_task`. | Per-operation state: source, log, exit handling. |
| `CompileCommand`    | Stack-local helper.   | Substitutes `<$fbc>`, `<$file>` etc. into the config template.     |
| `RunCommand`        | Stack-local helper.   | Substitutes `<$exe>`, `<$param>` for the run step.                 |
| `AsyncProcess`      | Self-deleting (wxProcess). | wraps `wxExecute`; redirects stdout/stderr; one-shot callback. |

`CompilerManager` is the only piece kept long-term. Everything else is
owned by the in-flight task.

## Single in-flight invariant

`CompilerManager::m_task` is a `unique_ptr<BuildTask>`. Replacing it
deletes the previous task, which cascades through `BuildTask::kill` to
abort any process the task spawned. The compile / run handlers always
call `m_task = std::make_unique<BuildTask>(...)` to enforce this — you
cannot have two builds racing.

## Compile flow

```
compile() / compileAndRun() / quickRun()
        │
        ▼
getActiveDocument() — reject if missing or wrong type
        │
        ▼
ensureSaved() — autosave or prompt; returns false on cancel
        │
        ▼
m_task = make_unique<BuildTask>(...)
        │
        ▼
BuildTask::compile(sourceFile)
        │   CompileCommand::build → "<$fbc> -lang ... <$file>"
        ▼
AsyncProcess::exec(cmd, workdir, /*redirect=*/true, onCompileFinished)
        │
        ▼  (later, on UI thread when fbc exits)
BuildTask::onCompileFinished
        │  populate compiler log, parse errors → console rows
        │  if errors and not compileAndRun:  setStatus, return
        │  if compileAndRun and ok:           BuildTask::run(...)
```

`AsyncProcess` always invokes its callback exactly once — either with
`launched=true` and an exit code, or with `launched=false` if
`wxExecute` failed to start the process. The handler distinguishes
"compiler crashed" from "compiler ran and reported errors".

## QuickRun temp file

`BuildTask::TEMPNAME = "FBIDETEMP.BAS"` is the canonical name for
quickrun's temp source file. The flow:

1. Save the active editor's contents to `<workDir>/FBIDETEMP.BAS`.
2. Compile the temp file as if it were a normal source.
3. Run the resulting executable.
4. Clean up both the temp source and its compiled output via
   `cleanupTempFiles()` once the run process terminates.

If the user is editing an untitled buffer, quickrun is the only path
that produces a runnable binary without first prompting Save As.

## Command templates

Compile and run command lines are templates with meta-tags:

| Tag        | Substitution                                                |
|------------|-------------------------------------------------------------|
| `<$fbc>`   | Resolved compiler binary path.                              |
| `<$file>`  | Source file to compile.                                     |
| `<$exe>`   | Compiled executable to run (run step only).                 |
| `<$param>` | Runtime parameters (from the Parameters dialog).            |

`CompileCommand::build` and `RunCommand::build` perform the
substitutions; the templates themselves live in
`config_<plat>.ini` under `compiler.compile` and `compiler.run`.

## Working directory rule

The working directory is *always* the directory of the source file
being compiled or run. There is no per-build override and no fallback
to the IDE directory — the old `ActivePath` config option from
fbide-old was removed because it was a footgun (relative include
resolution behaved differently than the source file's neighbours).

## Error parsing & navigation

`BuildTask::showErrors` walks the compiler output, recognises
`<file>(<line>) error:` patterns, and emits clickable rows into the
Output Console (`UIManager`'s output pane). Activating a row calls
`CompilerManager::goToError(line, fileName)`, which opens the file
(if not already open) and navigates the editor to the offending line.

## Compiler probe

| Member                         | Job                                                              |
|--------------------------------|------------------------------------------------------------------|
| `resolveCompilerBinary()`      | Resolve `compiler.path` against `appDir`; verify file exists.    |
| `getFbcVersion()`              | Run `fbc --version`, cache the line. Empty on failure.           |
| `resetFbcVersion()`            | Drop the cache (call from settings dialog when path edited).     |
| `checkCompilerOnStartup()`     | Once-on-launch probe. Surfaces the silenced "missing compiler" prompt. |
| `promptMissingCompiler()`      | Build-time prompt — never silenced, the user just asked for it.  |

The startup prompt uses `wxRichMessageDialog` with a "Don't show
again" checkbox; the checkbox flips
`alerts.ignore.missingCompilerBinary`. The build-time prompt
deliberately omits the checkbox — if the user just clicked Compile
and the binary is missing, suppressing the alert would be hostile.

Both prompts route to "Yes" → `SettingsDialog::create(Page::Compiler)`
so the user lands directly on the Compiler tab.

## Compiler log dialog

`showCompilerLog` opens a modeless dialog rendering
`BuildTask::getCompilerLog()`. The log uses a tiny `[bold]...[/bold]`
markup convention to highlight section headers (system info, command
line, errors) — `appendSystemInfo` writes the FBIde + fbc + OS
banner. `refreshCompilerLog` updates the dialog if it's already open.

## Recipe: add a new compile-time toggle

1. Add the config key under `compiler.*` in `config_<plat>.ini`.
2. Add a UI control on the Compiler settings tab and wire its
   apply() back into `ConfigManager`.
3. Extend the compile template with the new meta-tag if it influences
   the command line, and expand it in `CompileCommand::build`.
4. If the toggle changes the compiler binary or its capabilities,
   call `resetFbcVersion()` on apply so the next probe re-reads.

## Cross-links

- @ref commands — Compile / Run / QuickRun / Kill / Parameters menu
  entries and the `UIState` flips that mask them.
- @ref documents — `ensureSaved` interacts with the document save
  flow.
- @ref settings — Compiler tab + the config template strings live
  there.
- @ref ui — Output Console and status-bar text are written by the
  compile pipeline.
