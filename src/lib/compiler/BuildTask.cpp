//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "BuildTask.hpp"
#include "CompileCommand.hpp"
#include "CompilerManager.hpp"
#include "RunCommand.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Lang.hpp"
#include "lib/editor/Document.hpp"
#include "lib/editor/DocumentManager.hpp"
#include "lib/editor/Editor.hpp"
#include "lib/ui/OutputConsole.hpp"
#include "lib/ui/UIManager.hpp"
#include <cmake/config.hpp>
using namespace fbide;

BuildTask::BuildTask(Context& ctx, Document* doc)
: m_ctx(ctx)
, m_doc(doc) {}

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

    m_running = true;
    m_ctx.getUIManager().setCompilerState(UIState::Running);
    AsyncProcess::exec(buildRunCommand(executablePath), wxPathOnly(executablePath), false, [&](const ProcessResult& result) {
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

    // Validate compiler — getFbcVersion() checks path and caches the result
    const auto& fbcVersion = m_ctx.getCompilerManager().getFbcVersion();
    if (fbcVersion.empty()) {
        wxMessageBox(m_ctx.getLang()[LangId::SettingsCompilerPathError], "FBC", wxICON_ERROR);
        return;
    }

    // Build command
    const auto cmdStr = CompileCommand::makeDefault(sourceFile).build(m_ctx);

    m_compilerLog.Empty();
    m_compilerLog.Add("[bold]Command executed:[/bold]");
    m_compilerLog.Add(cmdStr);
    m_ctx.getCompilerManager().refreshCompilerLog();

    m_running = true;
    m_ctx.getUIManager().setCompilerState(UIState::Compiling);
    setStatus(LangId::StatusCompiling);
    AsyncProcess::exec(cmdStr, m_buildDir, true, [&](const ProcessResult& result) {
        setStatus(LangId::EmptyString);
        m_ctx.getUIManager().setCompilerState(UIState::None);
        m_running = false;
        onCompileFinished(result);
    });
}

void BuildTask::onCompileFinished(const ProcessResult& result) {
    auto& ui = m_ctx.getUIManager();

    // Log and show errors
    if (!result.output.empty()) {
        m_compilerLog.Add("");
        m_compilerLog.Add("[bold]Compiler output:[/bold]");
        showErrors(result.output);
        ui.showConsole();
    } else {
        ui.hideConsole();
    }

    m_compilerLog.Add("");
    m_compilerLog.Add("[bold]Results:[/bold]");

    if (!result) {
        m_compilerLog.Add("Compilation failed");
        appendSystemInfo();
        m_ctx.getCompilerManager().refreshCompilerLog();
        setStatus(LangId::StatusCompileFailed);
        cleanupTempFiles();
        return;
    }

    m_compilerLog.Add("Compilation successful");
    setStatus(LangId::StatusCompileComplete);

    m_compiledFile = deriveExecutablePath(m_sourceFile);
    m_compilerLog.Add("Generated executable: " + m_compiledFile);

    appendSystemInfo();
    m_ctx.getCompilerManager().refreshCompilerLog();

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
        const auto& lang = m_ctx.getLang();
        wxMessageBox(lang[LangId::RunExecError], lang[LangId::RunError], wxICON_ERROR);
    } else if (m_ctx.getConfig().getShowExitCode()) {
        wxString msg;
        msg << result.exitCode;
        wxMessageBox(msg, m_ctx.getLang()[LangId::RunExitCode]);
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
    bool foundError = false;
    bool navigated = false;

    for (const auto& line : output) {
        if (line.empty()) {
            continue;
        }

        m_compilerLog.Add(line);

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
    return RunCommand::makeDefault(executablePath).build(m_ctx);
}

void BuildTask::appendSystemInfo() {
    m_compilerLog.Add("");
    m_compilerLog.Add("[bold]System:[/bold]");
    m_compilerLog.Add("FBIde: " + wxString(cmake::project.version));
    const auto& fbcVersion = m_ctx.getCompilerManager().getFbcVersion();
    if (!fbcVersion.empty()) {
        m_compilerLog.Add("fbc:   " + fbcVersion);
    }
    m_compilerLog.Add("OS:    " + wxGetOsDescription());
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

void BuildTask::setStatus(const LangId id) const {
    m_ctx.getUIManager().getMainFrame()->SetStatusText(m_ctx.getLang()[id]);
}
