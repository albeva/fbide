//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "CompilerManager.hpp"
#include "CompileCommand.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Lang.hpp"
#include "lib/editor/Document.hpp"
#include "lib/editor/DocumentManager.hpp"
#include "lib/editor/Editor.hpp"
#include "lib/ui/OutputConsole.hpp"
#include "lib/ui/UIManager.hpp"

// ---------------------------------------------------------------------------
// Async process wrapper — notifies CompilerManager on termination
// ---------------------------------------------------------------------------

namespace {
class FbProcess final : public wxProcess {
public:
    explicit FbProcess(fbide::CompilerManager* manager)
    : wxProcess(static_cast<wxEvtHandler*>(manager))
    , m_manager(manager) {}

    void OnTerminate(int /*pid*/, const int status) override {
        m_manager->onProcessTerminated(status);
        delete this; // NOLINT(*-owning-memory)
    }

private:
    fbide::CompilerManager* m_manager;
};
} // namespace

// ---------------------------------------------------------------------------

using namespace fbide;

CompilerManager::CompilerManager(Context& ctx)
: m_ctx(ctx) {}

// ---------------------------------------------------------------------------
// Public API — each method is a clear flow of pipeline steps
// ---------------------------------------------------------------------------

void CompilerManager::compile() {
    auto* doc = getActiveDocument();
    if (doc == nullptr || !ensureSaved(*doc)) {
        return;
    }

    if (executeCompiler(resolveCompiler(), doc->getFilePath()) == nullptr) {
        setStatus(LangId::StatusCompileFailed);
        return;
    }

    setStatus(LangId::StatusCompileComplete);
}

void CompilerManager::compileAndRun() {
    auto* doc = getActiveDocument();
    if (doc == nullptr || !ensureSaved(*doc)) {
        return;
    }

    const auto* job = executeCompiler(resolveCompiler(), doc->getFilePath());
    if (job == nullptr) {
        setStatus(LangId::StatusCompileFailed);
        return;
    }

    setStatus(LangId::StatusCompileComplete);
    runAsync(buildRunCommand(job->compiledFile));
}

void CompilerManager::run() {
    const auto* doc = getActiveDocument();
    if (doc == nullptr) {
        return;
    }

    const auto exe = doc->getCompiledFile();
    if (exe.empty() || !wxFileExists(exe)) {
        const auto& lang = m_ctx.getLang();
        if (wxMessageBox(lang[LangId::RunCompileFirst], lang[LangId::RunCompileQuestion],
                wxYES_NO | wxICON_QUESTION)
            == wxNO) {
            return;
        }
        compileAndRun();
        return;
    }

    runAsync(buildRunCommand(exe));
}

void CompilerManager::quickRun() {
    auto* doc = getActiveDocument();
    if (doc == nullptr) {
        return;
    }

    // Determine temp folder from current file or IDE path
    const auto& filePath = doc->getFilePath();
    m_tempFolder = filePath.empty()
                     ? wxPathOnly(m_ctx.getConfig().getFbidePath()) + "/"
                     : wxPathOnly(filePath) + "/";

    // Save content to temp file
    const auto tempFile = m_tempFolder + "FBIDETEMP.bas";
    doc->getEditor()->SaveFile(tempFile);

    const auto* job = executeCompiler(resolveCompiler(), tempFile);
    if (job == nullptr) {
        setStatus(LangId::StatusCompileFailed);
        cleanupTempFiles();
        return;
    }

    setStatus(LangId::StatusCompileComplete);
    m_cleanupOnExit = true;
    runAsync(buildRunCommand(job->compiledFile));
}

// ---------------------------------------------------------------------------
// Compiler log
// ---------------------------------------------------------------------------

void CompilerManager::showCompilerLog() {
    const auto& lang = m_ctx.getLang();
    auto* frame = m_ctx.getUIManager().getMainFrame();
    const auto dlg = make_unowned<wxDialog>(
        frame, wxID_ANY,
        lang[LangId::CompilerLogTitle],
        wxDefaultPosition, wxSize(400, 200),
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX
    );

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    const auto output = make_unowned<wxTextCtrl>(
        dlg, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_BESTWRAP | wxTE_RICH2
    );
    output->SetFont(wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    sizer->Add(output, 1, wxEXPAND);

    const wxTextAttr normal = output->GetDefaultStyle();
    wxTextAttr bold = normal;
    bold.SetFont(bold.GetFont().Bold());

    // Parse [bold] tags in compiler log
    for (const auto& line : m_compilerLog) {
        bool inTag = false;
        wxString tag;
        for (const auto ch : line) {
            if (ch == '[' && !inTag) {
                inTag = true;
            } else if (ch == ']' && inTag) {
                inTag = false;
                if (tag.Lower() == "bold") {
                    output->SetDefaultStyle(bold);
                } else if (tag.Lower() == "/bold") {
                    output->SetDefaultStyle(normal);
                }
                tag.clear();
            } else if (inTag) {
                tag += ch;
            } else {
                output->WriteText(wxString(ch));
            }
        }
        output->WriteText("\r\n");
    }
    output->SetInsertionPoint(0);

    dlg->SetSizer(sizer);
    dlg->Show();
}

// ---------------------------------------------------------------------------
// Error navigation
// ---------------------------------------------------------------------------

void CompilerManager::goToError(const int line, const wxString& fileName) {
    // Skip temp files
    if (wxFileNameFromPath(fileName).Lower() == "fbidetemp.bas") {
        return;
    }

    auto& docManager = m_ctx.getDocumentManager();
    auto* doc = docManager.findByPath(fileName);
    if (doc == nullptr) {
        doc = docManager.openFile(fileName);
    }
    if (doc == nullptr) {
        return;
    }

    auto* editor = doc->getEditor();
    const int targetLine = line - 1;
    if (editor->GetCurrentLine() != targetLine) {
        editor->ScrollToLine(targetLine - editor->LinesOnScreen() / 2);
        editor->GotoLine(targetLine);
    }
    editor->SetFocus();
    editor->EnsureCaretVisible();
}

// ---------------------------------------------------------------------------
// Async process lifecycle
// ---------------------------------------------------------------------------

void CompilerManager::onProcessTerminated(const int exitCode) {
    m_processRunning = false;

    if (m_ctx.getConfig().getShowExitCode()) {
        wxString msg;
        msg << exitCode;
        wxMessageBox(msg, m_ctx.getLang()[LangId::RunExitCode]);
    }

    if (m_cleanupOnExit) {
        cleanupTempFiles();
        m_cleanupOnExit = false;
    }

    auto* frame = m_ctx.getUIManager().getMainFrame();
    frame->Raise();
    frame->SetFocus();

    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->SetFocus();
    }

    m_ctx.getUIManager().enableRunMenus(true);
}

// ---------------------------------------------------------------------------
// Pipeline steps
// ---------------------------------------------------------------------------

auto CompilerManager::getActiveDocument() -> Document* {
    if (m_processRunning) {
        return nullptr;
    }

    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr || doc->getType() != DocumentType::FreeBASIC) {
        return nullptr;
    }
    return doc;
}

auto CompilerManager::ensureSaved(Document& doc) -> bool {
    if (!doc.isModified()) {
        return true;
    }

    const auto& lang = m_ctx.getLang();
    const auto res = wxMessageBox(
        lang[LangId::RunFileModified],
        lang[LangId::RunSaveFile],
        wxICON_EXCLAMATION | wxYES_NO
    );
    if (res != wxYES) {
        return false;
    }

    return m_ctx.getDocumentManager().saveFile(doc);
}

auto CompilerManager::resolveCompiler() const -> wxString {
#ifdef __WXMSW__
    wxFileName path(m_ctx.getConfig().getCompilerPath());
    path.MakeAbsolute();
    if (!path.FileExists()) {
        const auto& lang = m_ctx.getLang();
        wxMessageBox(lang[LangId::SettingsCompilerPathError], "FBC", wxICON_ERROR);
        return {};
    }
    return path.GetFullPath();
#else
    return m_ctx.getConfig().getCompilerPath();
#endif
}

auto CompilerManager::executeCompiler(const wxString& compiler, const wxString& sourceFile) -> CompileJob* {
    if (compiler.empty()) {
        return nullptr;
    }

    // Prepare UI
    auto& ui = m_ctx.getUIManager();
    ui.getOutputConsole().clear();
    setStatus(LangId::StatusCompiling);

    // Set working directory if active path is enabled
    if (m_ctx.getConfig().getActivePath()) {
        wxSetWorkingDirectory(wxPathOnly(sourceFile));
    }

    // Build and execute command
    const auto cmd = CompileCommand::makeDefault(compiler, sourceFile);
    const auto cmdStr = cmd.build();

    m_compilerLog.Empty();
    m_compilerLog.Add("[bold]Command executed:[/bold]");
    m_compilerLog.Add(cmdStr);

    wxArrayString stdOutput;
    wxArrayString errOutput;
    const auto exitCode = wxExecute(cmdStr, stdOutput, errOutput);
    WX_APPEND_ARRAY(stdOutput, errOutput);

    // Log and show errors
    if (!stdOutput.empty()) {
        m_compilerLog.Add("");
        m_compilerLog.Add("[bold]Compiler output:[/bold]");
        showErrors(stdOutput);
        ui.showConsole();
    } else {
        ui.hideConsole();
    }

    m_compilerLog.Add("");
    m_compilerLog.Add("[bold]Results:[/bold]");

    if (exitCode != 0) {
        return nullptr;
    }

    // Store the job for later use (run command, etc.)
    auto* doc = m_ctx.getDocumentManager().getActive();
    m_job = CompileJob {
        .doc = doc,
        .sourceFile = sourceFile,
        .compiledFile = deriveExecutablePath(sourceFile),
    };

    // Update document's compiled file path
    if (doc != nullptr) {
        doc->setCompiledPath(m_job->compiledFile);
    }
    return &*m_job;
}

auto CompilerManager::showErrors(const wxArrayString& output) -> bool {
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
            goToError(static_cast<int>(lineNr), errorFile.GetFullPath());
            navigated = true;
        }
        foundError = true;
    }

    return foundError;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

auto CompilerManager::deriveExecutablePath(const wxString& sourceFile) -> wxString {
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

auto CompilerManager::buildRunCommand(const wxString& executablePath) const -> wxString {
    const wxFileName file(executablePath);
    auto cmd = m_ctx.getConfig().getRunCommand();

    cmd.Replace("<$param>", m_parameters);
    cmd.Replace("<$file>", file.GetFullPath());
    cmd.Replace("<$file_path>", file.GetPath());
    cmd.Replace("<$file_name>", file.GetName());
    cmd.Replace("<$file_ext>", file.GetExt());
#ifndef __WXMSW__
    cmd.Replace("<$terminal>", m_ctx.getConfig().getTerminal());
#endif

    return cmd;
}

void CompilerManager::runAsync(const wxString& command) {
    auto* process = new FbProcess(this); // NOLINT(*-owning-memory)
    const long result = wxExecute(command, wxEXEC_ASYNC, process);

    if (result == 0) {
        delete process; // NOLINT(*-owning-memory)
        m_processRunning = false;
        const auto& lang = m_ctx.getLang();
        wxMessageBox(lang[LangId::RunExecError] + command + "\"", lang[LangId::RunError], wxICON_ERROR);
        return;
    }

    m_processRunning = true;
    m_ctx.getUIManager().enableRunMenus(false);
}

void CompilerManager::cleanupTempFiles() {
    constexpr std::array files {
        "FBIDETEMP.bas",
        "FBIDETEMP.exe",
        "FBIDETEMP",
        "FBIDETEMP.asm",
        "FBIDETEMP.o",
        "FBIDETEMP.c",
        "FBIDETEMP.ll",
    };
    for (const auto& file : files) {
        const auto path = m_tempFolder + file;
        if (wxFileExists(path)) {
            wxRemoveFile(path);
        }
    }
}

void CompilerManager::setStatus(const LangId id) const {
    m_ctx.getUIManager().getMainFrame()->SetStatusText(m_ctx.getLang()[id]);
}
