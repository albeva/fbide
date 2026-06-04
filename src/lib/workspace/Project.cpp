//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Project.hpp"
#include "document/Document.hpp"
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

// Validate that no file in the subtree carries a Document* back-link.
// removeNode would otherwise leave Document::Source dangling — callers must
// close the relevant tabs through DocumentManager first.
void assertNoBoundDoc(const Project::Node* node) {
    if (const auto* file = node->getFile()) {
        assert(file->doc == nullptr && "removeNode: file has bound document — close tab first");
        return;
    }
    for (const auto* child : node->getFolder()->children) {
        assertNoBoundDoc(child);
    }
}

} // namespace

Project::Project(CompilerConfigCatalog& catalog, std::string name, fs::path rootDir)
: ProjectBase(catalog)
, m_name(std::move(name)) {
    createRoot(std::move(rootDir));
}

void Project::createRoot(fs::path rootDir) {
    const auto id = Node::Id::generate();
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
    auto node = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = parent,
        .path = std::move(path),
        .entry = std::move(entry),
    });
    auto* ptr = node.get();
    m_nodes.emplace(ptr->id, std::move(node));
    if (!ptr->path.empty()) {
        m_pathMap.emplace(ptr->path, ptr);
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
    return node;
}

auto Project::removeNode(Node* node, const RemoveMode mode) -> std::expected<void, Error> {
    assert(node != nullptr);
    assert(node != m_root && "cannot remove the project root");
    assertNoBoundDoc(node);

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

void Project::bindDocument(Node* file, Document& doc) {
    assert(file != nullptr && file->isFile() && "bindDocument target must be a file node");
    file->getFile()->doc = &doc;
    doc.bindToProject(this, file);
}

void Project::clearNodeDocument(Node* node) {
    assert(node != nullptr);
    if (auto* file = node->getFile()) {
        file->doc = nullptr;
    }
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
            result.push_back(file->doc);
        }
    }
    return result;
}

auto Project::getSources() const -> std::vector<Document*> {
    std::vector<Document*> result;
    for (const auto& node : m_nodes | std::views::values) {
        if (const auto* file = node->getFile(); file != nullptr && file->doc != nullptr) {
            if (node->path.extension() == ".bas") {
                result.push_back(file->doc);
            }
        }
    }
    return result;
}
