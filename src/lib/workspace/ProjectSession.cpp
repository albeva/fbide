//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ProjectSession.hpp"
#include "document/Document.hpp"
#include "utils/PathConversions.hpp"

using namespace fbide;

namespace {
constexpr int kVersion = 1;

auto joinIds(const std::vector<ProjectSession::Id>& ids) -> wxString {
    wxString out;
    for (const auto& id : ids) {
        if (!out.empty()) {
            out += ",";
        }
        out += id.string();
    }
    return out;
}

auto parseId(const wxString& text) -> ProjectSession::Id {
    if (text.empty()) {
        return {};
    }
    try {
        return ProjectSession::Id { text };
    } catch (...) {
        return {};
    }
}

auto splitIds(const wxString& text) -> std::vector<ProjectSession::Id> {
    std::vector<ProjectSession::Id> ids;
    for (const auto& part : wxSplit(text, ',')) {
        if (const auto id = parseId(part)) {
            ids.push_back(id);
        }
    }
    return ids;
}

/// Serialised form of an id, or empty for the invalid sentinel.
auto idText(ProjectSession::Id id) -> wxString {
    return id ? id.string() : wxString {};
}
} // namespace

ProjectSession::ProjectSession(const std::filesystem::path& projectFile)
: m_path(projectFile.parent_path() / ".fbide" / "session.ini")
, m_cfg(std::make_unique<wxFileConfig>(wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString, 0)) {}

ProjectSession::~ProjectSession() = default;

void ProjectSession::load() {
    std::error_code ec;
    if (!std::filesystem::exists(m_path, ec)) {
        return;
    }
    wxFFileInputStream in(toWxString(m_path));
    if (!in.IsOk()) {
        return;
    }
    m_cfg = std::make_unique<wxFileConfig>(in, wxConvUTF8);
}

void ProjectSession::save() {
    std::error_code ec;
    std::filesystem::create_directories(m_path.parent_path(), ec);
    m_cfg->Write("/session/version", kVersion);
    wxFFileOutputStream out(toWxString(m_path));
    if (!out.IsOk()) {
        wxLogError("Failed to write project session '%s'", toWxString(m_path));
        return;
    }
    m_cfg->Save(out, wxConvUTF8);
}

void ProjectSession::applyTo(Document& doc) {
    const auto* node = doc.getNode();
    if (node == nullptr) {
        return;
    }
    m_cfg->SetPath("/files/" + node->id.string());
    doc.loadSessionAttributes(*m_cfg);
    m_cfg->SetPath("/");
}

void ProjectSession::capture(Document& doc) {
    const auto* node = doc.getNode();
    if (node == nullptr) {
        return;
    }
    m_cfg->SetPath("/files/" + node->id.string());
    doc.setSessionAttributes(*m_cfg);
    m_cfg->SetPath("/");
}

void ProjectSession::forget(const Id id) {
    m_cfg->DeleteGroup("/files/" + id.string());

    auto open = openDocuments();
    std::erase(open, id);
    setOpenDocuments(open);

    auto expanded = expandedFolders();
    std::erase(expanded, id);
    setExpandedFolders(expanded);

    if (activeDocument() == id) {
        setActiveDocument({});
    }
    if (selectedNode() == id) {
        setSelectedNode({});
    }
}

void ProjectSession::setOpenDocuments(const std::vector<Id>& ids) {
    m_cfg->Write("/session/open", joinIds(ids));
}

auto ProjectSession::openDocuments() const -> std::vector<Id> {
    return splitIds(m_cfg->Read("/session/open", wxEmptyString));
}

void ProjectSession::setActiveDocument(const Id id) {
    m_cfg->Write("/session/activeDocument", idText(id));
}

auto ProjectSession::activeDocument() const -> Id {
    return parseId(m_cfg->Read("/session/activeDocument", wxEmptyString));
}

void ProjectSession::setSelectedNode(const Id id) {
    m_cfg->Write("/session/selectedNode", idText(id));
}

auto ProjectSession::selectedNode() const -> Id {
    return parseId(m_cfg->Read("/session/selectedNode", wxEmptyString));
}

void ProjectSession::setExpandedFolders(const std::vector<Id>& ids) {
    m_cfg->Write("/session/expanded", joinIds(ids));
}

auto ProjectSession::expandedFolders() const -> std::vector<Id> {
    return splitIds(m_cfg->Read("/session/expanded", wxEmptyString));
}
