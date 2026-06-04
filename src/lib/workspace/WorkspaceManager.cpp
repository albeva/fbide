//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "WorkspaceManager.hpp"
#include "EphemeralProject.hpp"
#include "Project.hpp"
#include "analyses/intellisense/IntellisenseService.hpp"
#include "app/Context.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "document/DocumentNotebook.hpp"
#include "document/DocumentPath.hpp"
#include "document/DocumentType.hpp"
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

    // Already open — surface the existing tab (no file I/O, no duplicate).
    if (auto* existing = docManager.findByPath(canonical)) {
        docManager.notebook().selectDocument(*existing);
        return existing;
    }

    // Future (Persistent projects): if `canonical` is a member of the open
    // project, open it and bind to that project before returning.

    // Ordinary document open — creates a tab and, for FreeBASIC, an
    // Ephemeral project to host it.
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
            closeProject(*m_project);
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

    auto project = std::make_unique<Project>(m_ctx.getCompilerManager().catalog(), path.stem().string(), path.parent_path());
    m_project = project.get();
    m_projectPath = path;
    m_projects.emplace(project->getId(), std::move(project));

    m_ctx.getSideBarManager().showProjectTree(*m_project);
    return m_project;
}

auto WorkspaceManager::createEphemeral(Document& doc) -> ProjectBase* {
    assert(doc.getProject() == nullptr && "document already bound to a project");
    assert(doc.getType() == DocumentType::FreeBASIC && "ephemeral projects only host FreeBASIC documents");

    auto project = std::make_unique<EphemeralProject>(m_ctx.getCompilerManager().catalog(), doc);
    doc.bindToProject(project.get());

    return m_projects.emplace(project->getId(), std::move(project)).first->second.get();
}

void WorkspaceManager::destroyEphemeral(Document& doc) {
    auto* project = doc.getProject();
    if (project != nullptr && project->isEphemeral()) {
        doc.unbindFromProject();
        closeProject(*project);
    }
}

void WorkspaceManager::closeProject(ProjectBase& project) {
    // `Document::unbindFromProject` clears the project-side `File::doc`
    // back-link, so `getDocuments()` won't include any doc that was
    // pre-unbound (e.g. by `destroyEphemeral` before it dispatched here).
    // The remaining loop body unbinds and closes any still-bound docs.
    auto& docManager = m_ctx.getDocumentManager();
    for (auto* document : project.getDocuments()) {
        if (document->getView() != nullptr) {
            document->unbindFromProject();
            docManager.closeFile(*document);
        }
    }
    // If this is the open persistent project, drop our cached handle and
    // remove its sidebar view *before* the owning unique_ptr is erased
    // (which would leave `project` dangling).
    if (&project == m_project) {
        m_project = nullptr;
        m_projectPath.clear();
        m_ctx.getSideBarManager().hideProjectTree();
    }
    m_projects.erase(project.getId());
}

auto WorkspaceManager::find(const ProjectBase::Id id) -> ProjectBase* {
    const auto it = m_projects.find(id);
    return it != m_projects.end() ? it->second.get() : nullptr;
}

auto WorkspaceManager::contains(const ProjectBase* project) const -> bool {
    if (project == nullptr) {
        return false;
    }
    // Scan owning storage directly — never dereference `project`, which
    // may already have been destroyed by the time a stale pointer reaches
    // us. ProjectBase counts are small (single-digit typical) so the linear
    // walk is cheaper than maintaining a parallel pointer set.
    return std::ranges::any_of(m_projects | std::views::values,
        [project](const auto& owned) { return owned.get() == project; });
}

void WorkspaceManager::onDocumentTypeChanged(Document& doc) {
    if (doc.getType() == DocumentType::FreeBASIC) {
        if (doc.getProject() == nullptr) {
            createEphemeral(doc);
        }
        return;
    }
    // Non-FreeBASIC: drop the ephemeral binding so the user can flip
    // the doc into HTML / Properties / etc. without dragging a
    // FreeBASIC-flavoured project along. Persistent projects survive
    // type changes — their non-source assets (images, Info.plist, …)
    // legitimately live under non-FreeBASIC types.
    if (const auto* project = doc.getProject(); project != nullptr && project->isEphemeral()) {
        destroyEphemeral(doc);
    }
}

auto WorkspaceManager::getActiveProject() const -> ProjectBase* {
    const auto* doc = m_ctx.getDocumentManager().getActive();
    return doc != nullptr ? doc->getProject() : nullptr;
}
