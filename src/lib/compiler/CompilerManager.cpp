//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerManager.hpp"
#include <wx/richmsgdlg.h>
#include "BuildTask.hpp"
#include "CompilerConfigCatalog.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "document/Document.hpp"
#include "document/DocumentIO.hpp"
#include "document/DocumentManager.hpp"
#include "document/DocumentPath.hpp"
#include "editor/Editor.hpp"
#include "settings/SettingsDialog.hpp"
#include "ui/CompilerLog.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

CompilerManager::CompilerManager(Context& ctx)
: m_ctx(ctx)
, m_catalog(std::make_unique<CompilerConfigCatalog>(ctx.getConfigManager())) {
    m_catalog->reload();
}

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
    m_task->compile(toWxString(doc->getFilePath()));
}

void CompilerManager::compileAndRun() {
    auto* doc = getActiveDocument();
    if (doc == nullptr || !ensureSaved(*doc)) {
        return;
    }

    m_task = std::make_unique<BuildTask>(m_ctx, doc);
    m_task->compileAndRun(toWxString(doc->getFilePath()), false);
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
    std::filesystem::path tempFolder;
    if (filePath.empty()) {
        std::error_code ec;
        tempFolder = std::filesystem::current_path(ec);
    } else {
        tempFolder = filePath.parent_path();
    }

    // Save content to temp file — preserve doc encoding so the compiler
    // sees bytes matching what the user sees.
    const auto tempFile = tempFolder / BuildTask::TEMPNAME;
    if (DocumentIO::save(tempFile, doc->getEditor()->GetText(), doc->getEncoding(), doc->getEolMode()) != DocumentIO::SaveResult::Success) {
        return;
    }

    m_task = std::make_unique<BuildTask>(m_ctx, doc);
    m_task->compileAndRun(toWxString(tempFile), true);
}

void CompilerManager::killProcess() const {
    if (m_task != nullptr && m_task->isRunning()) {
        m_task->kill();
    }
}

// ---------------------------------------------------------------------------
// Compiler log
// ---------------------------------------------------------------------------

void CompilerManager::showCompilerLog() const {
    auto& log = m_ctx.getUIManager().getCompilerLog();
    log.Show();
    log.Raise();
}

void CompilerManager::refreshCompilerLog() const {
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

void CompilerManager::checkCompilerOnStartup() const {
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

void CompilerManager::promptMissingCompiler() const {
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

void CompilerManager::goToError(const int line, const wxString& fileName) const {
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

auto CompilerManager::getActiveDocument() const -> Document* {
    if (m_task && m_task->isRunning()) {
        return nullptr;
    }

    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr || doc->getType() != DocumentType::FreeBASIC) {
        return nullptr;
    }
    return doc;
}

auto CompilerManager::ensureSaved(Document& doc) const -> bool {
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

void CompilerManager::setDocumentConfiguration(Document& doc, const wxString& pickedSlug) const {
    doc.setConfiguration(m_catalog->normalizeForStorage(pickedSlug));
    // Both the toolbar combobox and the status-bar field need to
    // reflect the new selection. The combobox already shows the picked
    // entry (it's the source of the event when picked from there); for
    // the status-bar popup path the click closes the menu and nothing
    // else would otherwise push the new label.
    pushStatusBarLabel();
}

// ---------------------------------------------------------------------------
// Toolbar combobox
// ---------------------------------------------------------------------------

auto CompilerManager::createConfigurationCombo(wxAuiToolBar* parent) -> wxComboBox* {
    // wxCB_READONLY: user can only pick from the list, never type.
    // Fixed width keeps the toolbar layout predictable.
    constexpr int kWidth = 160;
    m_configCombo = make_unowned<wxComboBox>(
        parent, wxID_ANY, wxString {},
        wxDefaultPosition, wxSize(kWidth, -1),
        wxArrayString {}, wxCB_READONLY
    ).get();
    m_configCombo->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent&) {
        onConfigurationComboSelected();
    });
    populateConfigurationCombo();
    // Sync to the remembered active document: disabled with no selection
    // when there's no FreeBASIC document (e.g. at first build), enabled
    // and selected when the toolbar is rebuilt while one is open.
    onActiveDocumentChanged(m_lastActiveDoc);
    return m_configCombo;
}

void CompilerManager::destroyConfigurationCombo() {
    if (m_configCombo == nullptr) {
        return;
    }
    // Drop the hosting toolbar's item first so it isn't left pointing at
    // a dead widget, then destroy the combobox (a child of the toolbar).
    if (auto* tb = wxDynamicCast(m_configCombo->GetParent(), wxAuiToolBar)) {
        tb->DeleteTool(m_configCombo->GetId());
        tb->Fit();
    }
    m_configCombo->Destroy();
    m_configCombo = nullptr;
}

void CompilerManager::refreshConfigurationCombo() {
    if (m_configCombo != nullptr) {
        populateConfigurationCombo();
        // Restore selection / enabled state for whichever doc is
        // currently active — population wiped both.
        onActiveDocumentChanged(m_lastActiveDoc);
        return;
    }
    // Even when the combobox doesn't exist (e.g. the user never wired
    // a `configuration` entry into `layout.toolbar`), the status-bar
    // selector still needs its label refreshed after the catalog
    // mutates.
    pushStatusBarLabel();
}

void CompilerManager::onActiveDocumentChanged(Document* doc) {
    m_lastActiveDoc = doc;
    if (m_configCombo != nullptr) {
        if (doc == nullptr || doc->getType() != DocumentType::FreeBASIC) {
            m_configCombo->Disable();
        } else {
            m_configCombo->Enable();
            const auto& resolved = m_catalog->resolveByPinnedSlug(doc->getConfiguration());
            if (const auto index = m_catalog->indexOf(resolved.slug); index >= 0) {
                m_configCombo->SetSelection(index);
            }
        }
    }
    pushStatusBarLabel();
}

void CompilerManager::pushStatusBarLabel() const {
    m_ctx.getUIManager().getStatusBar().refreshConfigurationField();
}

void CompilerManager::populateConfigurationCombo() const {
    if (m_configCombo == nullptr) {
        return;
    }
    m_configCombo->Clear();
    for (const auto& cfg : m_catalog->all()) {
        m_configCombo->Append(cfg.displayName);
    }
}

void CompilerManager::onConfigurationComboSelected() const {
    if (m_lastActiveDoc == nullptr || m_configCombo == nullptr) {
        return;
    }
    if (const auto* cfg = m_catalog->at(m_configCombo->GetSelection())) {
        setDocumentConfiguration(*m_lastActiveDoc, cfg->slug);
    }
}

auto CompilerManager::configurationStatusLabel() const -> wxString {
    if (m_lastActiveDoc == nullptr || m_lastActiveDoc->getType() != DocumentType::FreeBASIC) {
        return wxString {};
    }
    return m_catalog->resolveByPinnedSlug(m_lastActiveDoc->getConfiguration()).displayName;
}

auto CompilerManager::buildConfigurationMenu() const -> std::unique_ptr<wxMenu> {
    auto menu = std::make_unique<wxMenu>();
    const auto currentSlug = m_lastActiveDoc != nullptr
                               ? m_catalog->resolveByPinnedSlug(m_lastActiveDoc->getConfiguration()).slug
                               : wxString {};
    const auto configs = m_catalog->all();
    for (std::size_t i = 0; i < configs.size(); ++i) {
        auto* item = menu->AppendRadioItem(kStatusMenuIdBase + static_cast<int>(i), configs[i].displayName);
        item->Check(configs[i].slug == currentSlug);
    }
    return menu;
}

void CompilerManager::applyConfigurationMenuSelection(const int menuId) const {
    if (m_lastActiveDoc == nullptr) {
        return;
    }
    if (const auto* cfg = m_catalog->at(menuId - kStatusMenuIdBase)) {
        setDocumentConfiguration(*m_lastActiveDoc, cfg->slug);
    }
}
