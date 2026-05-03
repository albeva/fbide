//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerManager.hpp"
#include <wx/richmsgdlg.h>
#include "BuildTask.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "document/Document.hpp"
#include "document/DocumentIO.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
#include "settings/SettingsDialog.hpp"
#include "ui/CompilerLog.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

CompilerManager::CompilerManager(Context& ctx)
: m_ctx(ctx) {}

CompilerManager::~CompilerManager() = default;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void CompilerManager::compile() {
    auto* doc = getActiveDocument();
    if (doc == nullptr || !ensureSaved(*doc)) {
        return;
    }

    m_task = std::make_unique<BuildTask>(m_ctx, doc);
    m_task->compile(doc->getFilePath());
}

void CompilerManager::compileAndRun() {
    auto* doc = getActiveDocument();
    if (doc == nullptr || !ensureSaved(*doc)) {
        return;
    }

    m_task = std::make_unique<BuildTask>(m_ctx, doc);
    m_task->compileAndRun(doc->getFilePath(), false);
}

void CompilerManager::run() {
    auto* doc = getActiveDocument();
    if (doc == nullptr) {
        return;
    }

    const auto exe = doc->getCompiledFile();
    if (exe.empty() || !wxFileExists(exe)) {
        const auto res = wxMessageBox(
            m_ctx.tr("messages.compileFirst"), m_ctx.tr("messages.compileQuestion"),
            wxYES_NO | wxICON_QUESTION
        );
        if (res == wxNO) {
            return;
        }
        compileAndRun();
        return;
    }

    m_task = std::make_unique<BuildTask>(m_ctx, doc);
    m_task->run(exe, false);
}

void CompilerManager::quickRun() {
    auto* doc = getActiveDocument();
    if (doc == nullptr) {
        return;
    }

    // Determine temp folder from current file or IDE path
    const auto& filePath = doc->getFilePath();
    const auto tempFolder = filePath.empty()
                              ? wxPathOnly(m_ctx.getConfigManager().getAppDir()) + "/"
                              : wxPathOnly(filePath) + "/";

    // Save content to temp file — preserve doc encoding so the compiler
    // sees bytes matching what the user sees.
    const auto tempFile = tempFolder + BuildTask::TEMPNAME;
    if (DocumentIO::save(tempFile, doc->getEditor()->GetText(), doc->getEncoding(), doc->getEolMode()) != DocumentIO::SaveResult::Success) {
        return;
    }

    m_task = std::make_unique<BuildTask>(m_ctx, doc);
    m_task->compileAndRun(tempFile, true);
}

void CompilerManager::killProcess() {
    if (m_task != nullptr && m_task->isRunning()) {
        m_task->kill();
    }
}

// ---------------------------------------------------------------------------
// Compiler log
// ---------------------------------------------------------------------------

void CompilerManager::showCompilerLog() {
    auto& log = m_ctx.getUIManager().getCompilerLog();
    log.Show();
    log.Raise();
}

void CompilerManager::refreshCompilerLog() {
    if (m_task == nullptr) {
        return;
    }
    auto& log = m_ctx.getUIManager().getCompilerLog();
    log.log(m_task->getCompilerLog());
}

// ---------------------------------------------------------------------------
// Compiler version
// ---------------------------------------------------------------------------

auto CompilerManager::resolveCompilerBinary() const -> wxString {
    const wxString configured = m_ctx.getConfigManager().config().get_or("compiler.path", "");
    if (configured.IsEmpty()) {
        return {};
    }
    wxFileName path(configured);
    path.MakeAbsolute(m_ctx.getConfigManager().getAppDir());
    auto resolved = path.GetFullPath();
    if (resolved.IsEmpty() || !wxIsExecutable(resolved)) {
        return {};
    }
    return resolved;
}

auto CompilerManager::getFbcVersion() -> const wxString& {
    if (not m_fbcVersion.empty()) {
        return m_fbcVersion;
    }

    const auto compiler = resolveCompilerBinary();
    if (compiler.IsEmpty()) {
        return m_fbcVersion;
    }

    wxArrayString output;
    wxExecute("\"" + compiler + "\" --version", output);
    if (!output.empty()) {
        m_fbcVersion = output[0];
    }
    return m_fbcVersion;
}

namespace {
/// Open the Settings dialog focused on the Compiler tab.
void openCompilerSettings(Context& ctx) {
    SettingsDialog settings(ctx.getUIManager().getMainFrame(), ctx);
    settings.create(SettingsDialog::Page::Compiler);
    settings.ShowModal();
}
} // namespace

void CompilerManager::checkCompilerOnStartup() {
    auto& configManager = m_ctx.getConfigManager();
    auto& config = configManager.config();

    if (config.get_or("alerts.ignore.missingCompilerBinary", false)) {
        return;
    }
    if (!resolveCompilerBinary().IsEmpty()) {
        return;
    }

    wxRichMessageDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("messages.missingCompilerMessage"),
        m_ctx.tr("messages.missingCompilerTitle"),
        wxYES_NO | wxICON_WARNING
    );
    dlg.SetYesNoLabels(
        m_ctx.tr("messages.missingCompilerOpen"),
        m_ctx.tr("messages.missingCompilerSkip")
    );
    dlg.ShowCheckBox(m_ctx.tr("messages.dontShowAgain"));

    const auto answer = dlg.ShowModal();

    if (dlg.IsCheckBoxChecked()) {
        config["alerts"]["ignore"]["missingCompilerBinary"] = true;
        configManager.save(ConfigManager::Category::Config);
    }

    if (answer == wxID_YES) {
        openCompilerSettings(m_ctx);
    }
}

void CompilerManager::promptMissingCompiler() {
    wxRichMessageDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("messages.missingCompilerMessage"),
        m_ctx.tr("messages.missingCompilerTitle"),
        wxYES_NO | wxICON_WARNING
    );
    dlg.SetYesNoLabels(
        m_ctx.tr("messages.missingCompilerOpen"),
        m_ctx.tr("messages.missingCompilerSkip")
    );
    if (dlg.ShowModal() == wxID_YES) {
        openCompilerSettings(m_ctx);
    }
}

// ---------------------------------------------------------------------------
// Error navigation
// ---------------------------------------------------------------------------

void CompilerManager::goToError(const int line, const wxString& fileName) {
    auto& docManager = m_ctx.getDocumentManager();

    auto* doc = [&] -> Document* {
        const auto isTemp = wxFileNameFromPath(fileName) == BuildTask::TEMPNAME;
        if (m_task == nullptr) {
            return isTemp ? nullptr : docManager.openFile(fileName);
        }
        if (isTemp && m_task->isQuickRun()) {
            return m_task->getDocument();
        }
        return docManager.openFile(fileName);
    }();
    if (doc == nullptr) {
        return;
    }

    docManager.setActive(doc);
    doc->getEditor()->navigateToLine(line);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

auto CompilerManager::getActiveDocument() -> Document* {
    if (m_task && m_task->isRunning()) {
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
        return !doc.isNew();
    }

    const auto res = wxMessageBox(
        m_ctx.tr("messages.saveFile"),
        m_ctx.tr("messages.saveFileTitle"),
        wxICON_EXCLAMATION | wxYES_NO
    );
    if (res != wxYES) {
        return false;
    }

    return m_ctx.getDocumentManager().saveFile(doc);
}

void CompilerManager::setStatus(const wxString& path) const {
    m_ctx.getUIManager().getMainFrame()->SetStatusText(path.empty() ? wxString {} : m_ctx.tr(path));
}
