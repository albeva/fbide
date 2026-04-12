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
#include "lib/ui/UIManager.hpp"

// ---------------------------------------------------------------------------
// Async process wrapper
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------

using namespace fbide;

CompilerManager::CompilerManager(Context& ctx)
: m_ctx(ctx) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

auto CompilerManager::compile() -> bool {
    if (m_processRunning) {
        return false;
    }

    // 1. Prep: save document
    if (!prepareDocument()) {
        return false;
    }

    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr || doc->getType() != DocumentType::FreeBASIC) {
        return false;
    }

    auto result = compile(doc, { .temporary = false });
    return true;
}

void CompilerManager::compileAndRun() {
    if (m_processRunning) {
        return;
    }

    if (!compile()) {
        return;
    }

    const auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }

    const wxFileName file(doc->getCompiledFile());
    runAsync(buildRunCommand(file));
}

void CompilerManager::run() {
    if (m_processRunning) {
        return;
    }

    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }

    const wxFileName file(doc->getCompiledFile());
    if (!file.IsOk() || !file.FileExists()) {
        const auto& lang = m_ctx.getLang();
        if (wxMessageBox(lang[LangId::RunCompileFirst], lang[LangId::RunCompileQuestion],
                wxYES_NO | wxICON_QUESTION) == wxNO) {
            return;
        }
        compileAndRun();
        return;
    }

    runAsync(buildRunCommand(file));
}

void CompilerManager::quickRun() {
    if (m_processRunning) {
        return;
    }

    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }

    // Determine temp folder
    const auto& filePath = doc->getFilePath();
    if (filePath.empty()) {
        m_tempFolder = wxPathOnly(m_ctx.getConfig().getFbidePath()) + "/";
    } else {
        m_tempFolder = wxPathOnly(filePath) + "/";
    }

    // Save editor content to temp file
    const auto tempFile = m_tempFolder + "FBIDETEMP.bas";
    doc->getEditor()->SaveFile(tempFile);

    // Temporarily set document to temp file
    const auto oldPath = doc->getFilePath();
    const auto oldCompiled = doc->getCompiledFile();
    doc->setFilePath(tempFile);

    if (compile()) {
        m_isTemp = true;
        const wxFileName file(doc->getCompiledFile());
        doc->setCompiledFile(oldCompiled);
        doc->setFilePath(oldPath);
        runAsync(buildRunCommand(file));
    } else {
        doc->setCompiledFile(oldCompiled);
        doc->setFilePath(oldPath);
        cleanupTempFiles();
    }
}

void CompilerManager::openCmdPrompt() {
#ifdef __WXMSW__
    wxExecute("cmd.exe");
#else
    const auto& terminal = m_ctx.getConfig().getTerminal();
    if (!terminal.empty()) {
        wxExecute(terminal);
    }
#endif
}

void CompilerManager::showParametersDialog() {
    const auto& lang = m_ctx.getLang();
    wxTextEntryDialog dialog(
        m_ctx.getUIManager().getMainFrame(),
        lang[LangId::RunParamsPrompt],
        lang[LangId::ThemeParametersTitle],
        m_parameters,
        wxOK | wxCANCEL
    );

    if (dialog.ShowModal() == wxID_OK) {
        m_parameters = dialog.GetValue();
    }
}

void CompilerManager::toggleShowExitCode() {
    auto& config = m_ctx.getConfig();
    config.setShowExitCode(!config.getShowExitCode());
}

void CompilerManager::toggleActivePath() {
    auto& config = m_ctx.getConfig();
    config.setActivePath(!config.getActivePath());
}

void CompilerManager::showCompilerLog() {
    const auto& lang = m_ctx.getLang();
    auto* frame = m_ctx.getUIManager().getMainFrame();
    const auto dlg = make_unowned<wxDialog>(
        frame,
        wxID_ANY,
        lang[LangId::CompilerLogTitle],
        wxDefaultPosition,
        wxSize(400, 200),
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
        bool nesting = false;
        wxString tag;
        for (const auto ch : line) {
            if (ch == '[' && !nesting) {
                nesting = true;
            } else if (ch == ']' && nesting) {
                nesting = false;
                if (tag.Lower() == "bold") {
                    output->SetDefaultStyle(bold);
                } else if (tag.Lower() == "/bold") {
                    output->SetDefaultStyle(normal);
                }
                tag.clear();
            } else if (nesting) {
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

void CompilerManager::goToError(int line, const wxString& fileName) {
    auto& docManager = m_ctx.getDocumentManager();

    // Skip temp files
    if (wxFileNameFromPath(fileName).Lower() == "fbidetemp.bas") {
        return;
    }

    // Open the file if not already open
    auto* doc = docManager.findByPath(fileName);
    if (doc == nullptr) {
        doc = docManager.openFile(fileName);
    }
    if (doc == nullptr) {
        return;
    }

    auto* editor = doc->getEditor();
    const int targetLine = line - 1; // Convert 1-based to 0-based
    if (editor->GetCurrentLine() != targetLine) {
        editor->ScrollToLine(targetLine - editor->LinesOnScreen() / 2);
        editor->GotoLine(targetLine);
    }
    editor->SetFocus();
    editor->EnsureCaretVisible();
}

// ---------------------------------------------------------------------------
// Compile pipeline steps
// ---------------------------------------------------------------------------

auto CompilerManager::prepareDocument() -> bool {
    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return false;
    }

    if (!doc->isModified()) {
        return true;
    }

    const auto& lang = m_ctx.getLang();
    if (wxMessageBox(lang[LangId::RunFileModified], lang[LangId::RunSaveFile],
            wxICON_EXCLAMATION | wxYES_NO) != wxYES) {
        return false;
    }

    return m_ctx.getDocumentManager().saveFile(*doc);
}

auto CompilerManager::buildCompileCommand(const wxString& sourceFile) const -> CompileCommand {
    const auto compiler = resolveCompilerPath();
    if (compiler.empty()) {
        return {};
    }

    // Only compile FreeBASIC source files
    const wxFileName file(sourceFile);
    const auto ext = file.GetExt().Lower();
    if (ext != "bas" && ext != "bi" && ext != "rc") {
        return {};
    }

    auto cmd = CompileCommand::makeDefault(compiler, file.GetFullPath());
    return cmd;
}

auto CompilerManager::executeCompiler(const CompileCommand& cmd) -> CompileResult {
    CompileResult result;

    const auto cmdStr = cmd.build();
    if (cmdStr.empty()) {
        return result;
    }

    m_compilerLog.Empty();
    m_compilerLog.Add("[bold]Command executed:[/bold]");
    m_compilerLog.Add(cmdStr);

    wxArrayString arrOutput;
    wxArrayString arrErrOutput;
    result.exitCode = wxExecute(cmdStr, arrOutput, arrErrOutput);

    // Merge stdout and stderr
    WX_APPEND_ARRAY(arrOutput, arrErrOutput);
    result.output = std::move(arrOutput);

    return result;
}

auto CompilerManager::processResult(const CompileResult& result) -> bool {
    auto& ui = m_ctx.getUIManager();

    if (!result.output.empty()) {
        m_compilerLog.Add("");
        m_compilerLog.Add("[bold]Compiler output:[/bold]");
        const bool hasErrors = parseCompilerOutput(result.output);

        ui.showConsole();

        if (hasErrors && m_firstErrorLine >= 0) {
            goToError(m_firstErrorLine, m_firstErrorFile);
        }
    } else {
        ui.hideConsole();
    }

    m_compilerLog.Add("");
    m_compilerLog.Add("[bold]Results:[/bold]");

    return result.exitCode != 0;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

auto CompilerManager::resolveCompilerPath() const -> wxString {
#ifdef __WXMSW__
    wxFileName compilerPath(m_ctx.getConfig().getCompilerPath());
    compilerPath.MakeAbsolute();
    if (!compilerPath.FileExists()) {
        const auto& lang = m_ctx.getLang();
        wxMessageBox(lang[LangId::SettingsCompilerPathError], "FBC", wxICON_ERROR);
        return {};
    }
    return compilerPath.GetFullPath();
#else
    return m_ctx.getConfig().getCompilerPath();
#endif
}

auto CompilerManager::buildRunCommand(const wxFileName& file) const -> wxString {
    auto cmd = m_ctx.getConfig().getRunCommand().Lower().Trim(true).Trim(false);

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

auto CompilerManager::parseCompilerOutput(const wxArrayString& output) -> bool {
    m_firstErrorFile.clear();
    m_firstErrorLine = -1;
    bool foundError = false;

    for (const auto& line : output) {
        if (line.empty()) {
            continue;
        }

        m_compilerLog.Add(line);

        const auto braceStart = line.Find('(');
        const auto braceEnd = line.Find(')');

        bool isErrorLine = false;
        long lineNr = -1;
        long errorNr = -1;
        wxString errorFile;
        wxString errorMessage;

#ifdef __WXMSW__
        const bool hasFilePath = braceStart != wxNOT_FOUND && braceEnd != wxNOT_FOUND
            && line.length() > 1 && line[1] == ':';
#else
        const bool hasFilePath = braceStart != wxNOT_FOUND && braceEnd != wxNOT_FOUND
            && !line.empty() && line[0] == '/';
#endif

        if (hasFilePath) {
            const auto numStr = line.Mid(
                static_cast<size_t>(braceStart) + 1,
                static_cast<size_t>(braceEnd - braceStart) - 1
            );

            if (numStr.IsNumber()) {
                numStr.ToLong(&lineNr);

                wxFileName outputFile(line.Left(static_cast<size_t>(braceStart)));
                outputFile.MakeAbsolute();

                if (outputFile.IsOk() && outputFile.FileExists()) {
                    errorFile = outputFile.GetFullPath();
                    auto rest = line.Mid(static_cast<size_t>(braceEnd) + 4);
                    rest = rest.Mid(static_cast<size_t>(rest.Find(' ')) + 1);
                    errorMessage = rest.Mid(static_cast<size_t>(rest.Find(':')) + 2);
                    auto errStr = rest.Left(static_cast<size_t>(rest.Find(':')));
                    errStr.ToLong(&errorNr);
                    isErrorLine = true;
                }
            }
        }

        if (isErrorLine) {
            if (errorNr == 0) {
                errorNr = -1;
            }
            if (!foundError) {
                m_firstErrorFile = errorFile;
                m_firstErrorLine = static_cast<int>(lineNr);
                foundError = true;
            }
            m_ctx.getUIManager().getOutputConsole().addItem(static_cast<int>(lineNr), static_cast<int>(errorNr), errorFile, errorMessage);
        } else {
            auto cleaned = line;
            cleaned.Replace("\t", "  ");
            m_ctx.getUIManager().getOutputConsole().addItem(-1, -1, "", cleaned);
        }
    }

    return foundError;
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

void CompilerManager::onProcessTerminated(const int exitCode) {
    m_processRunning = false;

    if (m_ctx.getConfig().getShowExitCode()) {
        wxString temp;
        temp << exitCode;
        wxMessageBox(temp, m_ctx.getLang()[LangId::RunExitCode]);
    }

    if (m_isTemp) {
        cleanupTempFiles();
        m_isTemp = false;
    }

    auto* frame = m_ctx.getUIManager().getMainFrame();
    frame->Raise();
    frame->SetFocus();

    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc != nullptr) {
        doc->getEditor()->SetFocus();
    }

    m_ctx.getUIManager().enableRunMenus(true);
}

auto CompilerManager::compile(Document* doc)-> CompileResult {
    //     // 2. Prep UI
    //     const auto& lang = m_ctx.getLang();
    //     auto& ui = m_ctx.getUIManager();
    //     ui.getOutputConsole().clear();
    //     ui.getMainFrame()->SetStatusText(lang[LangId::StatusCompiling]);
    //
    //     // 3. Build compile command
    //     const auto cmd = buildCompileCommand(doc->getFilePath());
    //
    //     // 4. Execute compiler
    //     if (m_ctx.getConfig().getActivePath()) {
    //         const wxFileName file(doc->getFilePath());
    //         wxSetWorkingDirectory(file.GetPath());
    //     }
    //
    //     auto result = executeCompiler(cmd);
    //
    //     // 5. Update UI with results
    //     if (processResult(result)) {
    //         ui.getMainFrame()->SetStatusText(lang[LangId::StatusCompileFailed]);
    //         return false;
    //     }
    //
    //     // 6. Set compiled file path
    //     wxFileName exeFile(doc->getFilePath());
    //     const auto ext = exeFile.GetExt().Lower();
    //     if (ext == "bas" || ext == "bi") {
    // #ifdef __WXMSW__
    //         exeFile.SetExt("exe");
    // #else
    //         exeFile.SetExt("");
    // #endif
    //         doc->setCompiledFile(exeFile.GetFullPath());
    //         result.compiledFile = exeFile.GetFullPath();
    //     }
    //
    //     ui.getMainFrame()->SetStatusText(lang[LangId::StatusCompileComplete]);
    //     return true;
}

void CompilerManager::cleanupTempFiles() {
    wxRemoveFile(m_tempFolder + "FBIDETEMP.bas");
#ifdef __WXMSW__
    wxRemoveFile(m_tempFolder + "FBIDETEMP.exe");
#endif
    wxRemoveFile(m_tempFolder + "fbidetemp.asm");
    wxRemoveFile(m_tempFolder + "fbidetemp.o");
}
