//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "WorkspaceManager.hpp"
#include "EphemeralProject.hpp"
#include "Project.hpp"
#include "ProjectSession.hpp"
#include "analyses/intellisense/IntellisenseService.hpp"
#include "app/Context.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "document/DocumentPath.hpp"
#include "document/FileSession.hpp"
#include "sidebar/SideBarManager.hpp"
#include "ui/UIManager.hpp"

using namespace fbide;

WorkspaceManager::WorkspaceManager(Context& ctx)
: m_ctx(ctx)
, m_intellisense(std::make_unique<IntellisenseService>(ctx, &ctx.getDocumentManager())) {}

WorkspaceManager::~WorkspaceManager() = default;

auto WorkspaceManager::openFile(const std::filesystem::path& path) -> Document* {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return nullptr;
    }
    const auto canonical = canonicalizePath(path);

    // Session files load through the session manager, not as a tab.
    if (canonical.extension() == ".fbs") {
        m_ctx.getFileSession().load(canonical);
        return nullptr;
    }
    // Project files load a persistent project, not a document tab.
    if (canonical.extension() == ".fbp") {
        loadProject(canonical);
        return nullptr;
    }

    auto& docManager = m_ctx.getDocumentManager();

    // Known document — already open, or a member of the open persistent
    // project pre-created at load. Focus it, creating its editor if needed.
    if (auto* existing = docManager.findByPath(canonical)) {
        docManager.openEditorFor(*existing);
        return existing;
    }

    // Otherwise open it as a standalone document in the shared ephemeral.
    return docManager.openDocument(canonical);
}

void WorkspaceManager::openFile() {
    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("files.loadTitle"),
        "",
        ".bas",
        m_ctx.getConfigManager().filePatterns(
            { "fbproject", "freebasic", "properties", "markdown", "batch", "bash", "makefile", "json", "css", "all" }
        ),
        wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE
    );

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    wxArrayString paths;
    dlg.GetPaths(paths);
    for (const auto& path : paths) {
        openFile(toFsPath(path));
    }
}

auto WorkspaceManager::loadProject(const std::filesystem::path& path) -> Project* {
    // Re-opening the already-open project: keep it, no prompt.
    if (m_project != nullptr && m_projectPath == path) {
        return m_project;
    }

    // A different project is already open — ask what to do with it.
    if (m_project != nullptr) {
        wxMessageDialog dlg(
            m_ctx.getUIManager().getMainFrame(),
            m_ctx.tr("project.switch.message"),
            m_ctx.tr("project.switch.title"),
            wxYES_NO | wxCANCEL | wxICON_QUESTION
        );
        dlg.SetYesNoCancelLabels(
            m_ctx.tr("project.switch.close"),
            m_ctx.tr("project.switch.newWindow"),
            m_ctx.tr("project.switch.cancel")
        );
        switch (dlg.ShowModal()) {
        case wxID_YES: // Close the current project, then open the new one below.
            if (!closeProject(*m_project)) {
                return nullptr; // user cancelled a save prompt — keep current project
            }
            break;
        case wxID_NO: { // Open the new project in a separate window (process).
            const auto exe = wxStandardPaths::Get().GetExecutablePath();
            wxExecute("\"" + exe + "\" --new-window \"" + toWxString(path) + "\"");
            return nullptr;
        }
        default: // wxID_CANCEL / dialog dismissed.
            return nullptr;
        }
    }

    auto loaded = Project::loadFrom(path, m_ctx.getCompilerManager().catalog(), m_ctx.getConfigManager());
    if (!loaded) {
        wxMessageBox(
            m_ctx.tr("project.error.loadFailed"),
            m_ctx.tr("project.error.title"),
            wxICON_ERROR | wxOK,
            m_ctx.getUIManager().getMainFrame()
        );
        return nullptr;
    }
    m_project = loaded->get();
    m_projectPath = path;
    m_projects.emplace(m_project->getId(), std::move(*loaded));

    m_ctx.getFileHistory().addFile(path);
    m_ctx.getSideBarManager().showProjectTree(*m_project);
    m_ctx.getUIManager().syncProjectCommands();
    restoreProjectSession();
    return m_project;
}

auto WorkspaceManager::newProject() -> Project* {
    // Replacing an already-open project needs explicit confirmation.
    if (m_project != nullptr) {
        wxMessageDialog confirm(
            m_ctx.getUIManager().getMainFrame(),
            m_ctx.tr("project.new.message"),
            m_ctx.tr("project.new.title"),
            wxYES_NO | wxICON_QUESTION
        );
        if (confirm.ShowModal() != wxID_YES) {
            return nullptr;
        }
    }

    // Ask where to write the new project file.
    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("project.new.dialogTitle"),
        "",
        "untitled.fbp",
        m_ctx.getConfigManager().filePatterns({ "fbproject" }),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );
    if (dlg.ShowModal() != wxID_OK) {
        return nullptr;
    }
    const auto projectFile = toFsPath(dlg.GetPath());

    // Close the current project first (prompts to save its members); abort the
    // whole operation if the user cancels a save there.
    if (m_project != nullptr && !closeProject(*m_project)) {
        return nullptr;
    }

    // Write an empty project file, then open it through the normal load path
    // (which registers it, shows the tree, records it in the recent files, and
    // enables Close Project).
    const Project draft(
        m_ctx.getCompilerManager().catalog(), m_ctx.getConfigManager(),
        toWxString(projectFile.stem()), projectFile.parent_path()
    );
    if (const auto saved = draft.saveTo(projectFile); !saved) {
        wxMessageBox(
            m_ctx.tr("project.error.saveFailed"),
            m_ctx.tr("project.error.title"),
            wxICON_ERROR | wxOK,
            m_ctx.getUIManager().getMainFrame()
        );
        return nullptr;
    }
    return loadProject(projectFile);
}

auto WorkspaceManager::ephemeral() -> EphemeralProject& {
    if (m_ephemeral == nullptr) {
        // Lazily created — the compiler catalog (owned by CompilerManager) is
        // not yet available when this WorkspaceManager is constructed.
        m_ephemeral = std::make_unique<EphemeralProject>(m_ctx.getCompilerManager().catalog());
    }
    return *m_ephemeral;
}

auto WorkspaceManager::adoptStandalone(std::unique_ptr<Document> doc) -> Document* {
    return ephemeral().adopt(std::move(doc));
}

void WorkspaceManager::closeStandalone(Document* doc) {
    if (m_ephemeral != nullptr) {
        m_ephemeral->remove(doc);
    }
}

auto WorkspaceManager::documents() const -> std::vector<Document*> {
    std::vector<Document*> all;
    if (m_ephemeral != nullptr) {
        const auto docs = m_ephemeral->getDocuments();
        all.insert(all.end(), docs.begin(), docs.end());
    }
    for (const auto& project : m_projects | std::views::values) {
        const auto docs = project->getDocuments();
        all.insert(all.end(), docs.begin(), docs.end());
    }
    return all;
}

auto WorkspaceManager::closeProject(Project& project) -> bool {
    // Snapshot the session (open documents, active tab, tree state) while the
    // editors and tree are still alive, before tearing anything down.
    if (&project == m_project) {
        saveProjectSession();
    }
    // Run each open member through the full close pipeline so the user is
    // prompted to save modified files; bail (leaving the project open) on
    // cancel. closeFile keeps a persistent-project document alive (editor-less)
    // under its node — the documents themselves are owned by the project's
    // nodes and are destroyed with it on erase below. Iterates a snapshot, so
    // the per-document teardown can't invalidate the loop.
    auto& docManager = m_ctx.getDocumentManager();
    for (auto* document : project.getDocuments()) {
        if (document->hasView() && !docManager.closeFile(*document)) {
            return false;
        }
    }
    // Drop the cached handle + sidebar view *before* the owning unique_ptr is
    // erased (which would leave `project` dangling).
    if (&project == m_project) {
        m_project = nullptr;
        m_projectPath.clear();
        m_ctx.getSideBarManager().hideProjectTree();
        m_ctx.getUIManager().syncProjectCommands();
    }
    m_projects.erase(project.getId());
    return true;
}

void WorkspaceManager::applyDocumentSession(Document& doc) {
    auto* node = doc.getNode();
    if (node == nullptr) {
        return; // standalone / ephemeral document — no per-project session
    }
    if (auto* session = static_cast<Project*>(doc.getProject())->session()) {
        session->applyTo(doc);
    }
}

void WorkspaceManager::captureDocumentSession(Document& doc) {
    const auto* node = doc.getNode();
    if (node == nullptr) {
        return;
    }
    if (auto* session = static_cast<Project*>(doc.getProject())->session()) {
        session->capture(doc);
    }
}

void WorkspaceManager::persistProjectFile(Document& doc) {
    if (doc.getNode() != nullptr) { // persistent-project member
        static_cast<Project*>(doc.getProject())->save();
    }
}

void WorkspaceManager::saveProjectSession() {
    if (m_project == nullptr) {
        return;
    }
    auto* session = m_project->session();
    if (session == nullptr) {
        return;
    }
    const auto& docManager = m_ctx.getDocumentManager();

    // Capture every open project document (in tab order) and the active tab.
    std::vector<Project::Node::Id> openIds;
    for (auto* doc : docManager.documentsInTabOrder()) {
        if (const auto* node = doc->getNode()) {
            session->capture(*doc);
            openIds.push_back(node->id);
        }
    }
    session->setOpenDocuments(openIds);

    auto* active = docManager.getActive();
    auto* activeNode = active != nullptr ? active->getNode() : nullptr;
    session->setActiveDocument(activeNode != nullptr ? activeNode->id : Project::Node::Id {});

    // Capture the tree's expanded folders + selected node.
    m_ctx.getSideBarManager().captureProjectSession();

    session->save();
    // Re-write the .fbp too, so the open documents' project-scope state
    // (encoding, EOL, type override) is captured before they are torn down.
    m_project->save();
}

void WorkspaceManager::restoreProjectSession() {
    if (m_project == nullptr) {
        return;
    }
    auto* session = m_project->session();
    if (session == nullptr) {
        return;
    }
    auto& docManager = m_ctx.getDocumentManager();
    const auto thaw = m_ctx.getUIManager().freeze();

    // Reopen the documents that were open last session, in tab order.
    for (const auto id : session->openDocuments()) {
        if (auto* node = m_project->findNode(id); node != nullptr && node->document() != nullptr) {
            docManager.openEditorFor(*node->document());
        }
    }
    // Restore the focused tab.
    if (const auto activeId = session->activeDocument()) {
        if (auto* node = m_project->findNode(activeId); node != nullptr && node->document() != nullptr) {
            docManager.setActive(node->document());
        }
    }
}

auto WorkspaceManager::find(const ProjectBase::Id id) -> ProjectBase* {
    if (m_ephemeral != nullptr && m_ephemeral->getId() == id) {
        return m_ephemeral.get();
    }
    const auto it = m_projects.find(id);
    return it != m_projects.end() ? it->second.get() : nullptr;
}

auto WorkspaceManager::contains(const ProjectBase* project) const -> bool {
    if (project == nullptr) {
        return false;
    }
    if (project == m_ephemeral.get()) {
        return true;
    }
    // Scan owning storage directly — never dereference `project`, which may
    // already have been destroyed by the time a stale pointer reaches us.
    return std::ranges::any_of(m_projects | std::views::values,
        [project](const auto& owned) { return owned.get() == project; });
}

auto WorkspaceManager::getActiveProject() const -> ProjectBase* {
    auto* doc = m_ctx.getDocumentManager().getActive();
    return doc != nullptr ? doc->getProject() : nullptr;
}

void WorkspaceManager::onActiveDocumentChanged(Document* doc) {
    // Keep the shared ephemeral's build context pointed at the focused
    // standalone document (drives its capabilities / sources / config); clear
    // it when the active document belongs to a persistent project or is gone.
    if (m_ephemeral != nullptr) {
        m_ephemeral->setActive(doc != nullptr && doc->getProject() == m_ephemeral.get() ? doc : nullptr);
    }
}
