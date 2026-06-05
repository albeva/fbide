//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Project.hpp"
#include "ProjectSession.hpp"
#include "config/Version.hpp"
#include "document/Document.hpp"
#include "document/DocumentType.hpp"
#include "utils/PathConversions.hpp"
#include "utils/PlatformTrash.hpp"

using namespace fbide;
namespace fs = std::filesystem;

namespace {

// A leaf name must be a single path component — non-empty, no separators.
auto isValidLeafName(const std::string& name) -> bool {
    return !name.empty()
        && name.find('/') == std::string::npos
        && name.find('\\') == std::string::npos;
}

// Ordering for a folder's children: folders before files, then
// case-insensitive (ASCII) by filename.
auto nameLess(const Project::Node* lhs, const Project::Node* rhs) -> bool {
    if (lhs->isFolder() != rhs->isFolder()) {
        return lhs->isFolder(); // folders before files
    }
    const auto fold = [](std::string str) {
        for (auto& chr : str) {
            if (chr >= 'A' && chr <= 'Z') {
                chr = static_cast<char>(chr - 'A' + 'a');
            }
        }
        return str;
    };
    return fold(lhs->path.filename().string()) < fold(rhs->path.filename().string());
}

// Validate that no file in the subtree still has an open editor/tab — its
// EditorPanel back-links the Document we're about to destroy, so callers must
// close the relevant tabs through DocumentManager first.
void assertNoOpenEditor(const Project::Node* node) {
    if (const auto* file = node->getFile()) {
        assert((file->doc == nullptr || !file->doc->hasView()) && "removeNode: file is open — close its tab first");
        return;
    }
    for (const auto* child : node->getFolder()->children) {
        assertNoOpenEditor(child);
    }
}

// Parse a base-62 id string back to a node id; nullopt when malformed (the
// `Node::Id` string constructor throws on bad input).
auto parseNodeId(const wxString& text) -> std::optional<Project::Node::Id> {
    try {
        return Project::Node::Id { text };
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

Project::Project(CompilerConfigCatalog& catalog, ConfigManager& config, wxString name, fs::path rootDir)
: ProjectBase(catalog)
, m_config(config)
, m_name(std::move(name)) {
    createRoot(std::move(rootDir));
}

Project::~Project() = default;

auto Project::makeDocument(const fs::path& path) const -> std::unique_ptr<Document> {
    return std::make_unique<Document>(m_config, documentTypeFromPath(path), nullptr);
}

auto Project::makeNodeId() const -> Node::Id {
    // Random ids practically never collide, but guarantee uniqueness within the
    // project by re-rolling on the rare clash with an existing node.
    auto id = Node::Id::generate();
    while (m_nodes.contains(id)) {
        id = Node::Id::generate();
    }
    return id;
}

void Project::createRoot(fs::path rootDir) {
    const auto id = makeNodeId();
    auto root = std::make_unique<Node>(Node {
        .id = id,
        .parent = nullptr,
        .path = std::move(rootDir),
        .entry = Node::Folder {},
    });
    m_root = m_nodes.emplace(id, std::move(root)).first->second.get();
    if (!m_root->path.empty()) {
        m_pathMap.emplace(m_root->path, m_root);
    }
}

auto Project::attachNode(Node* parent, fs::path path, Node::Entry entry) -> Node* {
    return attachNode(parent, makeNodeId(), std::move(path), std::move(entry));
}

auto Project::attachNode(Node* parent, const Node::Id id, fs::path path, Node::Entry entry) -> Node* {
    auto node = std::make_unique<Node>(Node {
        .id = id,
        .parent = parent,
        .path = std::move(path),
        .entry = std::move(entry),
    });
    auto* ptr = node.get();
    m_nodes.emplace(ptr->id, std::move(node));
    if (!ptr->path.empty()) {
        m_pathMap.emplace(ptr->path, ptr);
    }
    // A file node owns an editor-less document, bound to the node.
    if (auto* file = ptr->getFile()) {
        file->doc = makeDocument(ptr->path);
        file->doc->bindToProject(this, ptr);
    }
    parent->getFolder()->children.push_back(ptr);
    return ptr;
}

void Project::sortChildren(Node* folder) {
    auto* data = folder->getFolder();
    switch (data->sort) {
    case Node::SortOrder::Name:
        std::ranges::sort(data->children, nameLess);
        break;
    }
}

auto Project::addFolder(Node* parent, const std::string& name) -> std::expected<Node*, Error> {
    if (parent == nullptr) {
        parent = m_root;
    }
    assert(parent->isFolder() && "parent must be a folder");
    if (!isValidLeafName(name)) {
        return std::unexpected(Error::InvalidName);
    }

    const auto path = parent->path / name;
    std::error_code ec;
    if (fs::exists(path, ec)) {
        return std::unexpected(Error::Clash);
    }
    fs::create_directory(path, ec);
    if (ec) {
        return std::unexpected(Error::IoError);
    }

    auto* node = attachNode(parent, path, Node::Folder {});
    sortChildren(parent);
    autosave();
    return node;
}

auto Project::addFile(Node* parent, const std::string& name) -> std::expected<Node*, Error> {
    if (parent == nullptr) {
        parent = m_root;
    }
    assert(parent->isFolder() && "parent must be a folder");
    if (!isValidLeafName(name)) {
        return std::unexpected(Error::InvalidName);
    }

    const auto path = parent->path / name;
    std::error_code ec;
    if (fs::exists(path, ec)) {
        return std::unexpected(Error::Clash);
    }
    {
        std::ofstream out(path);
        if (!out) {
            return std::unexpected(Error::IoError);
        }
    }

    auto* node = attachNode(parent, path, Node::File {});
    sortChildren(parent);
    autosave();
    return node;
}

auto Project::ensureFolderChain(const fs::path& dir) -> std::expected<Node*, Error> {
    if (dir == m_root->path) {
        return m_root;
    }
    if (!isUnderRoot(dir)) {
        return std::unexpected(Error::OutOfTree);
    }
    if (const auto it = m_pathMap.find(dir); it != m_pathMap.end()) {
        if (!it->second->isFolder()) {
            return std::unexpected(Error::Clash);
        }
        return it->second;
    }

    auto parent = ensureFolderChain(dir.parent_path());
    if (!parent) {
        return std::unexpected(parent.error());
    }
    auto* node = attachNode(*parent, dir, Node::Folder {});
    sortChildren(*parent);
    return node;
}

auto Project::addExisting(const fs::path& path) -> std::expected<Node*, Error> {
    if (!isUnderRoot(path)) {
        return std::unexpected(Error::OutOfTree);
    }
    if (const auto it = m_pathMap.find(path); it != m_pathMap.end()) {
        return it->second; // already part of the project
    }

    std::error_code ec;
    const bool isDir = fs::is_directory(path, ec);
    auto parent = ensureFolderChain(path.parent_path());
    if (!parent) {
        return std::unexpected(parent.error());
    }
    auto* node = attachNode(*parent, path, isDir ? Node::Entry { Node::Folder {} } : Node::Entry { Node::File {} });
    sortChildren(*parent);
    autosave();
    return node;
}

auto Project::removeNode(Node* node, const RemoveMode mode) -> std::expected<void, Error> {
    assert(node != nullptr);
    assert(node != m_root && "cannot remove the project root");
    assertNoOpenEditor(node);

    // Trash the on-disk subtree first (best-effort). The tree mutation runs
    // either way — once the user asks to remove, the tree should reach the
    // requested state even if the trash op fails; the failure is reported.
    std::expected<void, Error> result;
    if (mode == RemoveMode::AndTrash && !node->path.empty()) {
        if (!moveToTrash(node->path)) {
            result = std::unexpected(Error::IoError);
        }
    }

    std::erase(node->parent->getFolder()->children, node);
    destroySubtree(node);
    autosave();
    return result;
}

auto Project::moveNode(Node* node, Node* newParent) -> std::expected<void, Error> {
    assert(node != nullptr);
    assert(node != m_root && "cannot move the project root");
    assert(newParent != nullptr);
    assert(newParent->isFolder() && "newParent must be a folder");

    // Cycle prevention: walking newParent's ancestry must never reach node.
    for (const auto* cursor = newParent; cursor != nullptr; cursor = cursor->parent) {
        assert(cursor != node && "moveNode would create a cycle");
    }

    if (node->parent == newParent) {
        return {}; // already there
    }

    const auto newPath = newParent->path / node->path.filename();
    std::error_code ec;
    if (fs::exists(newPath, ec)) {
        return std::unexpected(Error::Clash);
    }
    fs::rename(node->path, newPath, ec);
    if (ec) {
        return std::unexpected(Error::IoError);
    }

    // Descendants' cached paths must follow the new prefix (rewrite BEFORE
    // updating this node's own path so the old prefix still matches).
    if (node->isFolder()) {
        rewriteSubtreePaths(node, node->path, newPath);
    }
    std::erase(node->parent->getFolder()->children, node);
    setNodePath(node, newPath);
    node->parent = newParent;
    newParent->getFolder()->children.push_back(node);
    sortChildren(newParent);
    autosave();
    return {};
}

auto Project::renameNode(Node* node, const std::string& newName) -> std::expected<void, Error> {
    assert(node != nullptr);
    assert(node != m_root && "cannot rename the project root");
    if (!isValidLeafName(newName)) {
        return std::unexpected(Error::InvalidName);
    }

    const auto newPath = node->path.parent_path() / newName;
    std::error_code ec;
    if (fs::exists(newPath, ec)) {
        return std::unexpected(Error::Clash);
    }
    fs::rename(node->path, newPath, ec);
    if (ec) {
        return std::unexpected(Error::IoError);
    }

    if (node->isFolder()) {
        rewriteSubtreePaths(node, node->path, newPath);
    }
    setNodePath(node, newPath);
    sortChildren(node->parent); // name changed → re-order siblings
    autosave();
    return {};
}

auto Project::findNode(const Node::Id id) -> Node* {
    const auto it = m_nodes.find(id);
    return it != m_nodes.end() ? it->second.get() : nullptr;
}

auto Project::findNode(const Node::Id id) const -> const Node* {
    const auto it = m_nodes.find(id);
    return it != m_nodes.end() ? it->second.get() : nullptr;
}

auto Project::findByPath(const fs::path& path) -> Node* {
    const auto it = m_pathMap.find(path);
    return it != m_pathMap.end() ? it->second : nullptr;
}

auto Project::setFilePath(Node* file, const fs::path& newPath) -> std::expected<void, Error> {
    assert(file != nullptr);
    assert(file->isFile() && "setFilePath is file-only; use renameNode for folders");
    if (!isUnderRoot(newPath)) {
        return std::unexpected(Error::OutOfTree);
    }
    setNodePath(file, newPath);
    return {};
}

void Project::setNodePath(Node* node, const fs::path& newPath) {
    assert(node != nullptr);
    if (!node->path.empty()) {
        m_pathMap.erase(node->path);
    }
    node->path = newPath;
    if (!newPath.empty()) {
        assert(!m_pathMap.contains(newPath) && "duplicate path in project node index");
        m_pathMap.emplace(newPath, node);
    }
}

void Project::destroySubtree(Node* node) {
    assert(node != nullptr);
    if (const auto* folder = node->getFolder()) {
        const auto children = folder->children; // snapshot — destroy invalidates the live vector
        for (auto* child : children) {
            destroySubtree(child);
        }
    }
    // Prune the removed node from the session (per-doc state + tree state) so it
    // doesn't linger in `.fbide/session.ini`.
    if (m_session != nullptr) {
        m_session->forget(node->id);
    }
    if (!node->path.empty()) {
        m_pathMap.erase(node->path);
    }
    m_nodes.erase(node->id);
}

void Project::rewriteSubtreePaths(Node* folder, const fs::path& oldPrefix, const fs::path& newPrefix) {
    assert(folder != nullptr && folder->isFolder());
    for (auto* child : folder->getFolder()->children) {
        std::error_code ec;
        const auto rel = fs::relative(child->path, oldPrefix, ec);
        const auto childNew = (ec || rel.empty()) ? child->path : (newPrefix / rel);
        // Recurse first (descendant paths must still match oldPrefix when
        // computed), then update this child.
        if (child->isFolder()) {
            rewriteSubtreePaths(child, oldPrefix, newPrefix);
        }
        setNodePath(child, childNew);
    }
}

auto Project::contextActions(const Node* node) const -> std::vector<Action> {
    assert(node != nullptr);
    if (node->isFolder()) {
        std::vector<Action> actions {
            Action::AddFolder,
            Action::AddSourceFile,
            Action::AddHeaderFile,
            Action::AddExisting,
        };
        if (node != m_root) {
            actions.push_back(Action::Remove);
        }
        return actions;
    }
    return { Action::Remove };
}

auto Project::isUnderRoot(const fs::path& candidate) const -> bool {
    if (candidate.empty() || m_root->path.empty()) {
        return true;
    }
    // Lexical comparison only — symlink resolution would require a
    // canonicalisation step that can fail on paths that don't exist yet.
    const auto rel = candidate.lexically_relative(m_root->path);
    return !rel.empty() && !rel.native().starts_with("..");
}

auto Project::getDocuments() const -> std::vector<Document*> {
    std::vector<Document*> result;
    result.reserve(m_nodes.size());
    for (const auto& nodePtr : m_nodes | std::views::values) {
        if (const auto* file = nodePtr->getFile(); file != nullptr && file->doc != nullptr) {
            result.push_back(file->doc.get());
        }
    }
    return result;
}

auto Project::getSources() const -> std::vector<Document*> {
    std::vector<Document*> result;
    for (const auto& node : m_nodes | std::views::values) {
        if (const auto* file = node->getFile(); file != nullptr && file->doc != nullptr) {
            if (node->path.extension() == ".bas") {
                result.push_back(file->doc.get());
            }
        }
    }
    return result;
}

// --- Persistence -----------------------------------------------------------

void Project::sortTree(Node* folder) {
    sortChildren(folder);
    for (auto* child : folder->getFolder()->children) {
        if (child->isFolder()) {
            sortTree(child);
        }
    }
}

auto Project::saveTo(const fs::path& projectFile) const -> std::expected<void, Error> {
    wxFileConfig cfg(wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString, 0);
    cfg.Write("/format", 1L);
    cfg.Write("/version", Version::fbide().asString());
    cfg.Write("/name", m_name);

    for (const auto& nodePtr : m_nodes | std::views::values) {
        const Node* node = nodePtr.get();
        if (node == m_root) {
            continue;
        }
        const wxString idStr = node->id.string();
        const wxString group = node->isFolder() ? "/folders/" : "/files/";
        cfg.Write(group + idStr, node->name());
        if (node->parent != m_root) {
            cfg.Write(group + idStr + "/parent", node->parent->id.string());
        }
    }

    wxFFileOutputStream out(toWxString(projectFile));
    if (!out.IsOk()) {
        return std::unexpected(Error::IoError);
    }
    cfg.Save(out, wxConvUTF8);
    return {};
}

auto Project::loadFrom(const fs::path& projectFile, CompilerConfigCatalog& catalog, ConfigManager& config)
    -> std::expected<std::unique_ptr<Project>, Error> {
    wxFFileInputStream in(toWxString(projectFile));
    if (!in.IsOk()) {
        return std::unexpected(Error::IoError);
    }
    wxFileConfig cfg(in, wxConvUTF8);

    wxString name = cfg.Read("/name", wxEmptyString);
    if (name.empty()) {
        name = toWxString(projectFile.stem());
    }

    auto project = std::make_unique<Project>(catalog, config, std::move(name), projectFile.parent_path());
    if (const auto built = project->buildFromConfig(cfg); !built) {
        return std::unexpected(built.error());
    }
    project->m_projectFile = projectFile;
    project->m_session = std::make_unique<ProjectSession>(projectFile);
    project->m_session->load();
    return project;
}

auto Project::buildFromConfig(wxFileConfig& cfg) -> std::expected<void, Error> {
    // Collect folder + file entry names (UUIDs) up front — wxFileConfig's
    // entry iteration is stateful, so it must not interleave with other reads.
    const auto collect = [&cfg](const wxString& group) {
        std::vector<wxString> uuids;
        cfg.SetPath(group);
        wxString name;
        long cookie = 0;
        for (bool ok = cfg.GetFirstEntry(name, cookie); ok; ok = cfg.GetNextEntry(name, cookie)) {
            uuids.push_back(name);
        }
        cfg.SetPath("/");
        return uuids;
    };
    const auto folderUuids = collect("/folders");
    const auto fileUuids = collect("/files");

    // Parse folders into id → {basename, optional<parentId>}.
    struct ParsedFolder {
        std::string basename;
        std::optional<Node::Id> parent;
    };
    std::unordered_map<Node::Id, ParsedFolder> parsedFolders;
    for (const auto& uuidWx : folderUuids) {
        const auto id = parseNodeId(uuidWx);
        if (!id) {
            return std::unexpected(Error::FormatError);
        }
        ParsedFolder parsed { cfg.Read("/folders/" + uuidWx, wxEmptyString).utf8_string(), std::nullopt };
        if (const wxString parentWx = cfg.Read("/folders/" + uuidWx + "/parent", wxEmptyString); !parentWx.empty()) {
            const auto parentId = parseNodeId(parentWx);
            if (!parentId) {
                return std::unexpected(Error::FormatError);
            }
            parsed.parent = parentId;
        }
        parsedFolders.emplace(*id, std::move(parsed));
    }

    // Materialise folders in dependency order (parents before children),
    // adopting their UUIDs. A non-empty residue means a cycle or a dangling
    // parent reference.
    std::unordered_map<Node::Id, Node*> folderNodes;
    std::size_t remaining = parsedFolders.size();
    for (bool progress = true; remaining > 0 && progress;) {
        progress = false;
        for (const auto& [id, parsed] : parsedFolders) {
            if (folderNodes.contains(id)) {
                continue;
            }
            Node* parent = nullptr;
            if (!parsed.parent) {
                parent = m_root;
            } else if (const auto it = folderNodes.find(*parsed.parent); it != folderNodes.end()) {
                parent = it->second;
            } else {
                continue; // parent not built yet
            }
            folderNodes.emplace(id, attachNode(parent, id, parent->path / parsed.basename, Node::Folder {}));
            --remaining;
            progress = true;
        }
    }
    if (remaining > 0) {
        return std::unexpected(Error::FormatError);
    }

    // Materialise files under their parent folder (or the root).
    for (const auto& uuidWx : fileUuids) {
        const auto id = parseNodeId(uuidWx);
        if (!id) {
            return std::unexpected(Error::FormatError);
        }
        const auto basename = cfg.Read("/files/" + uuidWx, wxEmptyString).utf8_string();
        Node* parent = m_root;
        if (const wxString parentWx = cfg.Read("/files/" + uuidWx + "/parent", wxEmptyString); !parentWx.empty()) {
            const auto parentId = parseNodeId(parentWx);
            if (!parentId) {
                return std::unexpected(Error::FormatError);
            }
            const auto it = folderNodes.find(*parentId);
            if (it == folderNodes.end()) {
                return std::unexpected(Error::FormatError); // dangling parent reference
            }
            parent = it->second;
        }
        attachNode(parent, *id, parent->path / basename, Node::File {});
    }

    sortTree(m_root);
    return {};
}

void Project::autosave() const {
    if (m_projectFile.empty()) {
        return;
    }
    if (const auto result = saveTo(m_projectFile); !result) {
        wxLogError("Failed to save project file '%s'", toWxString(m_projectFile));
    }
}
