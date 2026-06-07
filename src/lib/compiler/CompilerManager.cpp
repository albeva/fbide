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
#include "workspace/ProjectBase.hpp"
#include "workspace/WorkspaceManager.hpp"
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
    auto* project = getActiveProject();
    if (project == nullptr || !ensureSaved(*project)) {
        return;
    }

    const auto sources = project->getSources();
    if (sources.empty()) {
        return;
    }
    assert(sources.size() == 1 && "currently only one file can be compiled");

    const auto& cfg = project->getCompilerConfig();
    if (!ensureCompilable(cfg)) {
        return;
    }

    m_task = std::make_unique<BuildTask>(m_ctx, *project);
    m_task->compile(toWxString(sources.front()->getFilePath()));
}

void CompilerManager::compileAndRun() {
    auto* project = getActiveProject();
    if (project == nullptr || !ensureSaved(*project)) {
        return;
    }

    const auto sources = project->getSources();
    if (sources.empty()) {
        return;
    }
    assert(sources.size() == 1 && "currently only one file can be compiled");

    const auto& cfg = project->getCompilerConfig();
    if (!ensureCompilable(cfg) || !ensureRunnable(cfg)) {
        return;
    }

    m_task = std::make_unique<BuildTask>(m_ctx, *project);
    m_task->compileAndRun(toWxString(sources.front()->getFilePath()), false);
}

void CompilerManager::run() {
    auto* project = getActiveProject();
    if (project == nullptr) {
        return;
    }

    const auto& artefact = project->getArtefact();
    std::error_code ec;
    if (artefact.empty() || !std::filesystem::exists(artefact, ec)) {
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

    const auto& cfg = project->getCompilerConfig();
    if (!ensureRunnable(cfg)) {
        return;
    }

    m_task = std::make_unique<BuildTask>(m_ctx, *project);
    m_task->run(toWxString(artefact), false);
}

void CompilerManager::quickRun() {
    auto* project = getActiveProject();
    if (project == nullptr) {
        return;
    }
    const auto sources = project->getSources();
    if (sources.empty()) {
        return;
    }
    assert(sources.size() == 1 && "Currently only 1 source supported");
    auto* doc = sources.front();

    const auto& cfg = project->getCompilerConfig();
    if (!ensureCompilable(cfg) || !ensureRunnable(cfg)) {
        return;
    }

    // Determine temp folder from current file or IDE path
    const auto filePath = doc->getFilePath();
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

    m_task = std::make_unique<BuildTask>(m_ctx, *project);
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
    path.MakeAbsolute(toWxString(m_ctx.getConfigManager().getAppDir()));
    auto resolved = path.GetFullPath();
    if (resolved.IsEmpty() || !wxIsExecutable(resolved)) {
        return {};
    }
    return resolved;
}

auto CompilerManager::probeCompilerVersion(const std::filesystem::path& compilerPath) const -> wxString {
    wxFileName path(toWxString(compilerPath));
    path.MakeAbsolute(toWxString(m_ctx.getConfigManager().getAppDir()));
    const auto resolved = path.GetFullPath();
    if (resolved.IsEmpty() || !wxIsExecutable(resolved)) {
        return {};
    }
    wxArrayString output;
    wxExecute("\"" + resolved + "\" --version", output);
    return output.empty() ? wxString {} : output[0];
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

auto CompilerManager::promptConfigure(const wxString& titleKey, const wxString& messageKey, const wxString& target) const -> bool {
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
        return true;
    }
    return false;
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
    fbc.MakeAbsolute(toWxString(m_ctx.getConfigManager().getAppDir()));
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
    auto& workspace = m_ctx.getWorkspaceManager();

    auto* doc = [&] -> Document* {
        if (wxFileNameFromPath(fileName) == BuildTask::TEMPNAME) {
            // FBIDETEMP only makes sense while a quick-run is in flight —
            // map it back to the project's primary source so the user
            // navigates into the buffer they typed, not a temp file.
            if (m_task != nullptr && m_task->isQuickRun()) {
                // TODO: project should have this info set properly, with run target.
                if (const auto* project = m_task->getProject()) {
                    const auto sources = project->getSources();
                    if (sources.empty()) {
                        return nullptr;
                    }
                    assert(sources.size() == 1 && "Multi file not supported");
                    return sources.front();
                }
            }
            return nullptr;
        }
        return workspace.openFile(toFsPath(fileName));
    }();
    if (doc == nullptr) {
        return;
    }

    m_ctx.getDocumentManager().setActive(doc);
    doc->getEditor()->navigateToLine(line);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

auto CompilerManager::getActiveProject() -> ProjectBase* {
    if (m_task && m_task->isRunning()) {
        return nullptr;
    }
    return m_ctx.getWorkspaceManager().getActiveProject();
}

auto CompilerManager::ensureSaved(ProjectBase& project) -> bool {
    // Walk every currently-bound document and ensure it's saved.
    // For Ephemeral projects this is a single doc; Persistent projects
    // (future) will iterate over every modified member.
    return std::ranges::all_of(project.getSources(), [this](Document* doc) {
        if (doc == nullptr) {
            return true;
        }
        if (!doc->isModified()) {
            // An unmodified, untitled document hasn't been saved at all
            // — that's still a blocker for compile.
            return !doc->isNew();
        }

        const auto res = wxMessageBox(
            m_ctx.tr("messages.saveFile"),
            m_ctx.tr("messages.saveFileTitle"),
            wxICON_EXCLAMATION | wxYES_NO
        );
        if (res != wxYES) {
            return false;
        }
        return m_ctx.getDocumentManager().saveFile(*doc);
    });
}

void CompilerManager::setProjectConfiguration(ProjectBase& project, const wxString& pickedSlug) const {
    project.setConfigurationSlug(m_catalog->normalizeForStorage(pickedSlug));
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

auto CompilerManager::configurationProject() const -> ProjectBase* {
    // Driven polymorphically by the active document's project: an ephemeral
    // project exposes the active compiler configuration; a persistent project
    // (future) will expose its own targets and report an empty set until then.
    if (m_lastActiveDoc == nullptr) {
        return nullptr;
    }
    auto* project = m_lastActiveDoc->getProject();
    // Every standalone document now belongs to the shared ephemeral project,
    // so a non-FreeBASIC one still reports a (non-null) project. Gate on the
    // document type, not project != null, or the configuration combobox /
    // status-bar selector would leak onto non-FB standalone files.
    if (project != nullptr && project->isEphemeral() && m_lastActiveDoc->getType() != DocumentType::FreeBASIC) {
        return nullptr;
    }
    return project;
}

void CompilerManager::onActiveDocumentChanged(Document* doc) {
    m_lastActiveDoc = doc;
    if (m_configCombo != nullptr) {
        auto* project = configurationProject();
        if (project == nullptr) {
            m_configCombo->Disable();
        } else {
            // Rebuild so a hidden but currently-selected config gets
            // injected into the visible list. populateConfigurationCombo
            // reads the active project to decide which slug to force-include.
            populateConfigurationCombo();
            m_configCombo->Enable();
            const auto& resolved = m_catalog->resolveByPinnedSlug(project->getConfigurationSlug());
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
    // The dropdown contents are driven by the active project: Ephemeral
    // projects pass the catalog's menu-visible compiler configurations
    // through. The project's pinned slug is forwarded so a
    // hidden-but-selected entry still appears.
    auto* project = configurationProject();
    if (project == nullptr) {
        return;
    }
    const auto keepSlug = m_catalog->resolveByPinnedSlug(project->getConfigurationSlug()).slug;
    for (const auto* cfg : project->getMenuConfigurations(keepSlug)) {
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
    auto* project = configurationProject();
    if (project == nullptr || m_configCombo == nullptr) {
        return;
    }
    const auto sel = m_configCombo->GetSelection();
    if (sel < 0) {
        return;
    }
    if (const auto* data = dynamic_cast<wxStringClientData*>(m_configCombo->GetClientObject(static_cast<unsigned>(sel)))) {
        setProjectConfiguration(*project, data->GetData());
    }
}

auto CompilerManager::configurationStatusLabel() const -> wxString {
    auto* project = configurationProject();
    if (project == nullptr) {
        return wxString {};
    }
    return m_catalog->resolveByPinnedSlug(project->getConfigurationSlug()).displayName;
}

auto CompilerManager::buildConfigurationMenu() const -> std::unique_ptr<wxMenu> {
    auto menu = std::make_unique<wxMenu>();
    auto* project = configurationProject();
    if (project == nullptr) {
        return menu;
    }
    const auto currentSlug = m_catalog->resolveByPinnedSlug(project->getConfigurationSlug()).slug;
    // Menu item ID = base + the slug's index within `catalog().all()`,
    // not within the filtered subset — that way the ID still maps back
    // through `catalog().at()` in `applyConfigurationMenuSelection` even
    // though some entries were skipped.
    for (const auto* cfg : project->getMenuConfigurations(currentSlug)) {
        const auto catalogIndex = m_catalog->indexOf(cfg->slug);
        auto* item = menu->AppendRadioItem(kStatusMenuIdBase + catalogIndex, cfg->displayName);
        item->Check(cfg->slug == currentSlug);
    }
    return menu;
}

void CompilerManager::applyConfigurationMenuSelection(const int menuId) const {
    auto* project = configurationProject();
    if (project == nullptr) {
        return;
    }
    if (const auto* cfg = m_catalog->at(menuId - kStatusMenuIdBase)) {
        setProjectConfiguration(*project, cfg->slug);
    }
}
