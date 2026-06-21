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
#include "FbcAutoDetect.hpp"
#include "FbcDefines.hpp"
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
#include "utils/PathConversions.hpp"
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

    const auto& cfg = m_catalog->resolveByPinnedSlug(doc->getConfiguration());
    if (!ensureCompilable(cfg)) {
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

    const auto& cfg = m_catalog->resolveByPinnedSlug(doc->getConfiguration());
    if (!ensureCompilable(cfg) || !ensureRunnable(cfg)) {
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

    const auto& cfg = m_catalog->resolveByPinnedSlug(doc->getConfiguration());
    if (!ensureRunnable(cfg)) {
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

    const auto& cfg = m_catalog->resolveByPinnedSlug(doc->getConfiguration());
    if (!ensureCompilable(cfg) || !ensureRunnable(cfg)) {
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

auto CompilerManager::probeCompilerVersion(const std::filesystem::path& compilerPath) const -> wxString {
    wxFileName path(toWxString(compilerPath));
    path.MakeAbsolute(m_ctx.getConfigManager().getAppDir());
    const auto resolved = path.GetFullPath();
    if (resolved.IsEmpty() || !wxIsExecutable(resolved)) {
        return {};
    }
    wxArrayString output;
    wxExecute("\"" + resolved + "\" --version", output);
    return output.empty() ? wxString {} : output[0];
}

auto CompilerManager::builtinDefines(const std::filesystem::path& compilerPath) const
    -> const std::unordered_set<std::string>& {
    static const std::unordered_set<std::string> empty;

    wxFileName path(toWxString(compilerPath));
    path.MakeAbsolute(m_ctx.getConfigManager().getAppDir());
    const auto resolved = path.GetFullPath();
    if (resolved.IsEmpty() || !wxIsExecutable(resolved)) {
        return empty;
    }

    const std::string key = resolved.utf8_string();
    if (const auto it = m_builtinDefinesCache.find(key); it != m_builtinDefinesCache.end()) {
        return it->second;
    }

    std::unordered_set<std::string> defines;
    const wxFileName stub(m_ctx.getConfigManager().getIdeDir(), "fbc-defines.bas");
    if (stub.FileExists()) {
        const wxString stubPath = stub.GetFullPath();
        const wxString tempObj = wxFileName::CreateTempFileName("fbide-fbdefs");
        wxArrayString stdOut;
        wxArrayString stdErr;
        wxExecute("\"" + resolved + "\" -c \"" + stubPath + "\" -o \"" + tempObj + "\"", stdOut, stdErr, wxEXEC_SYNC);
        defines = parseFbcDefines(stdOut);
        defines.merge(parseFbcDefines(stdErr));
        if (!tempObj.IsEmpty()) {
            wxRemoveFile(tempObj);
        }
    }
    return m_builtinDefinesCache.emplace(key, std::move(defines)).first->second;
}

void CompilerManager::warmBuiltinDefines() const {
    std::ignore = builtinDefines(m_catalog->resolveByPinnedSlug(std::nullopt).path);
}

namespace {
/// Open the Settings dialog at a compiler deep-link target (page +
/// optional "<slug>/<field>").
void openCompilerSettings(Context& ctx, const wxString& target) {
    SettingsDialog settings(ctx.getUIManager().getMainFrame(), ctx);
    settings.create(target);
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
        openCompilerSettings(m_ctx, "compiler");
    }
}

auto CompilerManager::detectCompilerOnFirstRun() -> bool {
#ifdef __WXMSW__
    auto& configManager = m_ctx.getConfigManager();
    auto detected = FbcAutoDetect::detectSilently(toFsPath(configManager.getAppDir()), wxIsPlatform64Bit());
    if (!detected.has_value()) {
        return false;
    }
    // Install + persist exactly like the Settings-dialog auto-detect path:
    // replace the [compiler] subtree wholesale, refresh the catalog/UI, and
    // flush the config so the choice survives the next launch.
    configManager.config()["compiler"] = std::move(*detected);
    linkBundledHelpFile();
    m_catalog->reload();
    refreshConfigurationCombo();
    configManager.save(ConfigManager::Category::Config);
    return true;
#else
    return false;
#endif
}

void CompilerManager::linkBundledHelpFile() const {
#ifdef __WXMSW__
    auto& configManager = m_ctx.getConfigManager();
    auto& config = configManager.config();

    // Respect an existing help file — only fill an empty one, never clobber
    // a path the user (or a previous detect) already set.
    if (!config.get_or("paths.helpFile", wxString {}).IsEmpty()) {
        return;
    }

    // The canonical compiler just written by auto-detect. Its folder is where
    // the installer drops the bundled compiler and the matching manual.
    const wxString binary = resolveCompilerBinary();
    if (binary.IsEmpty()) {
        return;
    }

    // FreeBASIC ships its manual as FB-manual-<version>.chm. Pin the lookup to
    // the compiler's reported version (reuse the build-flow --version probe)
    // so a future fbc bump never wires up a stale manual.
    const auto version = FbcAutoDetect::parseVersion(probeCompilerVersion(toFsPath(binary)));
    if (!version.has_value()) {
        return;
    }

    const wxFileName chm(wxFileName(binary).GetPath(), "FB-manual-" + *version + ".chm");
    if (!chm.FileExists()) {
        return;
    }

    config["paths"]["helpFile"] = configManager.relative(chm.GetFullPath());
#endif
}

void CompilerManager::promptConfigure(const wxString& titleKey, const wxString& messageKey, const wxString& target) const {
    wxRichMessageDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr(messageKey),
        m_ctx.tr(titleKey),
        wxYES_NO | wxICON_WARNING
    );
    dlg.SetYesNoLabels(
        m_ctx.tr("messages.missingCompilerOpen"),
        m_ctx.tr("messages.missingCompilerSkip")
    );
    if (dlg.ShowModal() == wxID_YES) {
        openCompilerSettings(m_ctx, target);
    }
}

void CompilerManager::promptMissingCompiler() const {
    promptConfigure("messages.missingCompilerTitle", "messages.missingCompilerMessage", "compiler");
}

auto CompilerManager::ensureCompilable(const ResolvedCompilerConfig& cfg) const -> bool {
    // 1. fbc binary must be set and reachable. Resolve the (possibly
    //    relative) configured path against the IDE app dir the same way
    //    CompileCommand does before invoking it.
    const auto rawPath = toWxString(cfg.path);
    wxFileName fbc { rawPath };
    fbc.MakeAbsolute(m_ctx.getConfigManager().getAppDir());
    if (rawPath.IsEmpty() || !wxIsExecutable(fbc.GetFullPath())) {
        promptConfigure("messages.missingCompilerTitle", "messages.missingCompilerMessage", "compiler/" + cfg.slug + "/path");
        return false;
    }
    // 2. Compile-command template must be present.
    if (cfg.compileCommand.Strip(wxString::both).IsEmpty()) {
        promptConfigure("messages.missingCompileCommandTitle", "messages.missingCompileCommand", "compiler/" + cfg.slug + "/compileCommand");
        return false;
    }
    return true;
}

auto CompilerManager::ensureRunnable(const ResolvedCompilerConfig& cfg) const -> bool {
    if (cfg.runCommand.Strip(wxString::both).IsEmpty()) {
        promptConfigure("messages.missingRunCommandTitle", "messages.missingRunCommand", "compiler/" + cfg.slug + "/runCommand");
        return false;
    }
    return true;
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
    // The configuration also supplies the intellisense `#include` search dirs
    // and preprocessor defines; re-evaluate them so the new config takes effect
    // now, not on the next edit.
    m_ctx.getDocumentManager().refreshIntellisenseConfig();
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
    )
                        .get();
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
    }
    // Delegate the create/destroy decision to UIManager: combobox or
    // status-bar field are both surfaces it owns and may need to add or
    // drop depending on the new catalog state.
    m_ctx.getUIManager().refreshConfigurationDisplay();
}

void CompilerManager::onActiveDocumentChanged(Document* doc) {
    m_lastActiveDoc = doc;
    if (m_configCombo != nullptr) {
        if (doc == nullptr || doc->getType() != DocumentType::FreeBASIC) {
            m_configCombo->Disable();
        } else {
            // Rebuild so a hidden but currently-selected config gets
            // injected into the visible list. populateConfigurationCombo
            // reads the active doc to decide which slug to force-include.
            populateConfigurationCombo();
            m_configCombo->Enable();
            const auto& resolved = m_catalog->resolveByPinnedSlug(doc->getConfiguration());
            if (const auto index = comboIndexForSlug(resolved.slug); index >= 0) {
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
    // Catalog owns the visibility logic — manager just renders what it
    // hands back. The pinned slug of the active doc is forwarded so a
    // hidden-but-pinned config still appears in the combo (the catalog
    // honours it via the alwaysInclude path).
    const auto keepSlug = m_lastActiveDoc != nullptr && m_lastActiveDoc->getType() == DocumentType::FreeBASIC
                            ? m_catalog->resolveByPinnedSlug(m_lastActiveDoc->getConfiguration()).slug
                            : wxString {};
    for (const auto* cfg : m_catalog->menuConfigs(keepSlug)) {
        m_configCombo->Append(cfg->displayName, new wxStringClientData(cfg->slug));
    }
}

auto CompilerManager::comboIndexForSlug(const wxString& slug) const -> int {
    if (m_configCombo == nullptr) {
        return -1;
    }
    for (unsigned i = 0; i < m_configCombo->GetCount(); ++i) {
        if (const auto* data = dynamic_cast<wxStringClientData*>(m_configCombo->GetClientObject(i));
            data != nullptr && data->GetData() == slug) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CompilerManager::onConfigurationComboSelected() const {
    if (m_lastActiveDoc == nullptr || m_configCombo == nullptr) {
        return;
    }
    const auto sel = m_configCombo->GetSelection();
    if (sel < 0) {
        return;
    }
    if (const auto* data = dynamic_cast<wxStringClientData*>(m_configCombo->GetClientObject(static_cast<unsigned>(sel)))) {
        setDocumentConfiguration(*m_lastActiveDoc, data->GetData());
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
    // Menu item ID = base + the slug's index within `catalog().all()`,
    // not within the filtered subset — that way the ID still maps back
    // through `catalog().at()` in `applyConfigurationMenuSelection` even
    // though some entries were skipped.
    for (const auto* cfg : m_catalog->menuConfigs(currentSlug)) {
        const auto catalogIndex = m_catalog->indexOf(cfg->slug);
        auto* item = menu->AppendRadioItem(kStatusMenuIdBase + catalogIndex, cfg->displayName);
        item->Check(cfg->slug == currentSlug);
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
