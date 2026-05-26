//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "WorkspaceManager.hpp"
#include "analyses/intellisense/IntellisenseService.hpp"
#include "app/Context.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "document/DocumentType.hpp"

using namespace fbide;

WorkspaceManager::WorkspaceManager(Context& ctx)
: m_ctx(ctx)
, m_intellisense(std::make_unique<IntellisenseService>(ctx, &ctx.getDocumentManager())) {}

WorkspaceManager::~WorkspaceManager() = default;

auto WorkspaceManager::createEphemeral(Document& doc) -> Project& {
    assert(doc.getProject() == nullptr && "document already bound to a project");
    assert(doc.getType() == DocumentType::FreeBASIC && "ephemeral projects only host FreeBASIC documents");

    // Lift the path out of the document BEFORE binding — `bindToProject`
    // overwrites the source variant with the node ID, so the project's
    // node must already carry the path by then.
    std::optional<std::filesystem::path> path;
    if (const auto& src = doc.getSource();
        std::holds_alternative<std::filesystem::path>(src)) {
        if (const auto& candidate = std::get<std::filesystem::path>(src); !candidate.empty()) {
            path = candidate;
        }
    }

    auto project = std::make_unique<Project>(Project::Mode::Ephemeral);
    const auto nodeId = project->addFile(std::move(path), &doc);
    doc.bindToProject(*project, nodeId);

    auto& ref = *project;
    m_projects.emplace(ref.getId(), std::move(project));
    return ref;
}

void WorkspaceManager::destroyEphemeral(Document& doc) {
    auto* project = doc.getProject();
    assert(project != nullptr && project->isEphemeral() && "destroyEphemeral called without an ephemeral project");
    doc.unbindFromProject();
    closeProject(*project);
}

void WorkspaceManager::closeProject(Project& project) {
    // Snapshot the bound documents up-front — closeFile mutates the
    // DocumentManager's document list which would otherwise invalidate
    // mid-iteration views. Skip entries whose back-link has already
    // drifted away (unbound out-of-band); those are not our concern.
    auto& docManager = m_ctx.getDocumentManager();
    for (auto* document : project.getDocuments()) {
        if (document->getView() != nullptr && document->getProject() == &project) {
            document->unbindFromProject();
            docManager.closeFile(*document);
        }
    }
    m_projects.erase(project.getId());
    if (m_activeProject == &project) {
        m_activeProject = nullptr;
    }
}

auto WorkspaceManager::contains(const Project* project) const -> bool {
    if (project == nullptr) {
        return false;
    }
    return m_projects.contains(project->getId());
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
    if (auto* project = doc.getProject(); project != nullptr && project->isEphemeral()) {
        destroyEphemeral(doc);
    }
}

void WorkspaceManager::setActiveDocument(Document* doc) {
    m_activeProject = doc != nullptr ? doc->getProject() : nullptr;
}
