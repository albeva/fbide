//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "BuildTask.hpp"
#include <cmake/config.hpp>
#include "CompileCommand.hpp"
#include "CompilerConfigCatalog.hpp"
#include "CompilerManager.hpp"
#include "RunCommand.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
#include "ui/CompilerLog.hpp"
#include "ui/OutputConsole.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

auto BuildTask::compilerLog() const -> CompilerLog& {
    return m_ctx.getUIManager().getCompilerLog();
}

BuildTask::BuildTask(Context& ctx, Document* doc)
: m_ctx(ctx)
, m_doc(doc)
, m_config(ctx.getCompilerManager().catalog().resolveByPinnedSlug(
      doc != nullptr ? doc->getConfiguration() : std::nullopt
  )) {}

BuildTask::~BuildTask() {
    if (m_process != nullptr) {
        m_process->detach();
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void BuildTask::compile(const wxString& sourceFile) {
    m_shouldRun = false;
    m_isQuickRun = false;
    startCompiler(sourceFile);
}

void BuildTask::compileAndRun(const wxString& sourceFile, const bool quickRun) {
    m_shouldRun = true;
    m_isQuickRun = quickRun;
    startCompiler(sourceFile);
}

void BuildTask::run(const wxString& executablePath, const bool quickRun) {
    m_compiledFile = executablePath;
    m_isQuickRun = quickRun;

    const auto cmdStr = buildRunCommand(executablePath);

    // A standalone run (not chained from a compile) starts a fresh log.
    if (!m_shouldRun) {
        compilerLog().clear();
    }
    compilerLog().add(CompilerLogSection::RunCommand, cmdStr);

    m_running = true;
    m_ctx.getUIManager().setCompilerState(UIState::Running);
    m_process = AsyncProcess::exec(cmdStr, wxPathOnly(executablePath), false, [&](const ProcessResult& result) {
        m_process = nullptr;
        m_ctx.getUIManager().setCompilerState(UIState::None);
        m_running = false;
        onRunFinished(result);
    });
}

// ---------------------------------------------------------------------------
// Compile pipeline
// ---------------------------------------------------------------------------

void BuildTask::startCompiler(const wxString& sourceFile) {
    m_sourceFile = sourceFile;
    m_buildDir = wxPathOnly(sourceFile);

    // Prepare UI
    auto& ui = m_ctx.getUIManager();
    ui.getOutputConsole().clear();

    // Build command. The resolved config is captured at task construction
    // so a mid-build catalog change (e.g. user editing settings) cannot
    // half-apply across compile and run steps.
    const auto cmdStr = CompileCommand::makeDefault(sourceFile).build(m_config, m_ctx.getConfigManager());

    compilerLog().clear();
    compilerLog().add(CompilerLogSection::CompilerCommand, cmdStr);

    // Arm the running state *before* the synchronous probe below. The probe
    // pumps a nested event loop, so a Compile/Run command dispatched inside it
    // would otherwise see isRunning() == false, pass CompilerManager's re-entry
    // guard, and replace m_task — freeing the BuildTask whose startCompiler is
    // still on the stack. Setting it here makes that re-entry a no-op.
    m_running = true;
    m_ctx.getUIManager().setCompilerState(UIState::Compiling);
    setStatus("status.compiling");

    // Probe the active config's compiler version *before* launching the async
    // compile. probeCompilerVersion runs a synchronous wxExecute (with its own
    // nested event loop); calling it from onCompileFinished would re-enter wx's
    // child-process machinery inside the compile process's OnTerminate handler
    // and deadlock. appendSystemInfo() reads the cached value instead.
    m_fbcVersion = m_ctx.getCompilerManager().probeCompilerVersion(m_config.path);
    m_process = AsyncProcess::exec(cmdStr, m_buildDir, true, [&](const ProcessResult& result) {
        m_process = nullptr;
        setStatus("");
        m_ctx.getUIManager().setCompilerState(UIState::None);
        m_running = false;
        onCompileFinished(result);
    });
}

void BuildTask::onCompileFinished(const ProcessResult& result) {
    // Capture the compiler output verbatim and route it to the console.
    if (!result.output.empty()) {
        wxString outputText;
        for (const auto& line : result.output) {
            if (line.empty()) {
                continue;
            }
            if (!outputText.empty()) {
                outputText << "\n";
            }
            outputText << line;
        }
        compilerLog().add(CompilerLogSection::CompilerOutput, outputText);
        showErrors(result.output);
    }

    if (!result) {
        compilerLog().add(CompilerLogSection::CompileResult, m_ctx.tr("dialogs.log.failureMessage"));
        appendSystemInfo();
        setStatus("status.compileFailed");
        cleanupTempFiles();
        return;
    }

    setStatus("status.compileComplete");

    m_compiledFile = deriveExecutablePath(m_sourceFile);
    auto generatedLine = m_ctx.tr("dialogs.log.generatedExecutable");
    generatedLine.Replace("{path}", m_compiledFile);
    compilerLog().add(
        CompilerLogSection::CompileResult,
        m_ctx.tr("dialogs.log.successMessage") + "\n" + generatedLine
    );

    appendSystemInfo();

    // Update document's compiled file path
    if (auto* doc = getDocument()) {
        doc->setCompiledPath(m_compiledFile);
    }

    if (m_shouldRun) {
        run(m_compiledFile, m_isQuickRun);
    }
}

void BuildTask::onRunFinished(const ProcessResult& result) {
    if (!result.launched) {
        compilerLog().add(CompilerLogSection::RunResult, m_ctx.tr("dialogs.log.executionFailed"));
    } else {
        auto exitLine = m_ctx.tr("dialogs.log.exitCodeLine");
        exitLine.Replace("{code}", wxString::Format("%d", result.exitCode));
        compilerLog().add(CompilerLogSection::RunResult, exitLine);
    }

    if (!result.launched) {
        wxMessageBox(m_ctx.tr("messages.execError"), m_ctx.tr("common.error"), wxICON_ERROR);
    } else if (m_ctx.getConfigManager().config().get_or("commands.showExitCode", false)) {
        wxString msg;
        msg << result.exitCode;
        wxMessageBox(msg, m_ctx.tr("messages.exitCode"));
    }

    cleanupTempFiles();

    auto* frame = m_ctx.getUIManager().getMainFrame();
    frame->Raise();
    frame->SetFocus();

    if (auto* doc = getDocument()) {
        doc->getEditor()->SetFocus();
    }
}

// ---------------------------------------------------------------------------
// Error parsing
// ---------------------------------------------------------------------------

auto BuildTask::showErrors(const wxArrayString& output) -> bool {
    auto& console = m_ctx.getUIManager().getOutputConsole();

    // show console first, then populate it, otherwise glitches happen
    m_ctx.getUIManager().showConsole(true);
    const auto thaw = FreezeLock(&console);

    bool foundError = false;
    bool navigated = false;

    for (const auto& line : output) {
        if (line.empty()) {
            continue;
        }

        // Try to parse "file(line) error NR: message" format
        const auto braceStart = line.Find('(');
        const auto braceEnd = line.Find(')');

#ifdef __WXMSW__
        const bool hasPath = braceStart != wxNOT_FOUND && braceEnd != wxNOT_FOUND
                          && line.length() > 1 && line[1] == ':';
#else
        const bool hasPath = braceStart != wxNOT_FOUND && braceEnd != wxNOT_FOUND
                          && !line.empty() && line[0] == '/';
#endif

        if (!hasPath) {
            auto cleaned = line;
            cleaned.Replace("\t", "  ");
            console.addItem(-1, -1, "", cleaned);
            continue;
        }

        const auto numStr = line.Mid(
            static_cast<size_t>(braceStart) + 1,
            static_cast<size_t>(braceEnd - braceStart) - 1
        );

        long lineNr = -1;
        if (!numStr.IsNumber() || !numStr.ToLong(&lineNr)) {
            console.addItem(-1, -1, "", line);
            continue;
        }

        wxFileName errorFile(line.Left(static_cast<size_t>(braceStart)));
        errorFile.MakeAbsolute();
        if (!errorFile.IsOk() || !errorFile.FileExists()) {
            console.addItem(-1, -1, "", line);
            continue;
        }

        // Parse error number and message: ") error NR: message"
        auto rest = line.Mid(static_cast<size_t>(braceEnd) + 4);
        rest = rest.Mid(static_cast<size_t>(rest.Find(' ')) + 1);
        const auto message = rest.Mid(static_cast<size_t>(rest.Find(':')) + 2);
        long errorNr = -1;
        rest.Left(static_cast<size_t>(rest.Find(':'))).ToLong(&errorNr);
        if (errorNr == 0) {
            errorNr = -1;
        }

        console.addItem(
            static_cast<int>(lineNr),
            static_cast<int>(errorNr),
            errorFile.GetFullPath(),
            message
        );

        if (!navigated) {
            m_ctx.getCompilerManager().goToError(static_cast<int>(lineNr), errorFile.GetFullPath());
            navigated = true;
        }
        foundError = true;
    }

    return foundError;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

auto BuildTask::deriveExecutablePath(const wxString& sourceFile) -> wxString {
    wxFileName exe(sourceFile);
    const auto ext = exe.GetExt().Lower();
    if (ext == "bas" || ext == "bi") {
#ifdef __WXMSW__
        exe.SetExt("exe");
#else
        exe.SetExt("");
#endif
    }
    return exe.GetFullPath();
}

auto BuildTask::buildRunCommand(const wxString& executablePath) const -> wxString {
    return RunCommand::makeDefault(executablePath).build(m_config, m_ctx.getCompilerManager().getParameters());
}

void BuildTask::appendSystemInfo() {
    wxString info = m_ctx.tr("dialogs.log.fbidePrefix") + wxString(cmake::project.version);
    if (!m_fbcVersion.empty()) {
        info << "\n" << m_ctx.tr("dialogs.log.fbcPrefix") + m_fbcVersion;
    }
    info << "\n" << m_ctx.tr("dialogs.log.osPrefix") + wxGetOsDescription();
    compilerLog().add(CompilerLogSection::SystemInfo, info);
}

void BuildTask::cleanupTempFiles() {
    if (not m_isQuickRun || not wxDirExists(m_buildDir)) {
        return;
    }

    wxFileName file { TEMPNAME };
    file.ClearExt();
    const auto base = m_buildDir + wxFileName::GetPathSeparator() + file.GetFullName();
    constexpr std::array exts {
        ".BAS",
        ".exe",
        "",
        ".asm",
        ".o",
        ".c",
        ".ll",
    };
    for (const auto& ext : exts) {
        const auto path = base + ext;
        if (wxFileExists(path)) {
            wxRemoveFile(path);
        }
    }
    m_buildDir.clear();
}

auto BuildTask::getDocument() const -> Document* {
    if (m_doc != nullptr && m_ctx.getDocumentManager().contains(m_doc)) {
        return m_doc;
    }
    return nullptr;
}

void BuildTask::kill() {
    if (m_process != nullptr) {
        m_process->kill();
    }
}

void BuildTask::setStatus(const wxString& path) const {
    m_ctx.getUIManager().getMainFrame()->SetStatusText(path.empty() ? wxString {} : m_ctx.tr(path));
}
