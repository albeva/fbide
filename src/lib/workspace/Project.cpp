//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Project.hpp"
#include "compiler/CompilerConfigCatalog.hpp"
#include "config/ConfigManager.hpp"
#include "document/Document.hpp"

using namespace fbide;

namespace {

// Process cwd, or empty on failure — used as the Ephemeral root path
// fallback when the project is created around an untitled buffer.
auto currentWorkingPath() -> std::filesystem::path {
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    return ec ? std::filesystem::path {} : cwd;
}

} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
Project::Project(ConfigManager& config, const Mode mode)
: m_config(config)
, m_id(Id::generate())
, m_mode(mode) {
    m_path = currentWorkingPath();

    if (mode == Mode::Persistent) {
        const auto id = Node::Id::generate();
        auto root = std::make_unique<Node>(Node {
            .id = id,
            .parent = nullptr,
            .path = {},
            .entry = Node::Folder { .name = "", .children = {} },
        });
        m_root = m_nodes.emplace(id, std::move(root)).first->second.get();
    }
}

// `path` taken by value so callers can move-in; moved into the Node below.
auto Project::addFile(Document* doc, Node* parent) -> Node* {
    const auto path = doc->getFilePath();
    const auto hasPath = not path.empty();

    if (parent != nullptr) {
        assert(m_nodes.contains(parent->id) && "parent should be part of this project");
    }

    auto node = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = parent,
        .path = std::move(path),
        .entry = Node::File { .doc = doc },
    });
    auto* ptr = node.get();

    // Ephemeral: file becomes the root, no parent. Persistent: resolve parent
    // (defaulting to m_root) and validate before allocating.
    if (m_mode == Mode::Ephemeral) {
        assert(m_root == nullptr && "Ephemeral project should have no root");
        assert(parent == nullptr && "Ephemeral project file should not have a parent");

        m_root = ptr;
        m_path = hasPath ? ptr->path.parent_path() : currentWorkingPath();
    } else {
        if (parent == nullptr) {
            parent = m_root;
        }
        assert(parent != nullptr && parent->isFolder() && "parent must be a folder node");

        if (hasPath) {
            if (not isUnderRoot(path)) {
                return nullptr; // REVIEW: should return an error
            }
            if (m_pathMap.contains(path)) {
                return nullptr; // REVIEW: should return an error? existing node?
            }
        }

        parent->getFolder()->children.push_back(ptr);
    }

    m_nodes.emplace(ptr->id, std::move(node));
    doc->bindToProject(this, ptr);
    if (hasPath) {
        m_pathMap.emplace(ptr->path, ptr);
    }

    return ptr;
}

auto Project::findNode(const Node::Id id) -> Node* {
    const auto it = m_nodes.find(id);
    return it != m_nodes.end() ? it->second.get() : nullptr;
}

auto Project::findNode(const Node::Id id) const -> const Node* {
    const auto it = m_nodes.find(id);
    return it != m_nodes.end() ? it->second.get() : nullptr;
}

auto Project::isUnderRoot(const std::filesystem::path& candidate) const -> bool {
    // Empty candidate (untitled file) or no anchor yet (Ephemeral pre-first-
    // addFile, fresh Persistent before loader sets the root) — no meaningful
    // constraint to enforce.
    const auto* rootPath = (m_root != nullptr && !m_root->path.empty()) ? &m_root->path : nullptr;
    if (candidate.empty() || rootPath == nullptr) {
        return true;
    }
    // Lexical comparison only — symlink resolution would require a
    // canonicalisation step that can fail on paths that don't exist yet.
    const auto rel = candidate.lexically_relative(*rootPath);
    return !rel.empty() && !rel.native().starts_with("..");
}

auto Project::setFilePath(Node* file, const std::filesystem::path& newPath) -> std::expected<void, Error> {
    assert(file != nullptr);
    assert(file->isFile() && "setFilePath is file-only; use renameNode for folders");

    // Persistent projects keep all members under the project root —
    // Save As to elsewhere is a UI-layer error. Ephemeral projects
    // skip the check because their root moves to accommodate.
    if (m_mode == Mode::Persistent && !isUnderRoot(newPath)) {
        return std::unexpected(Error::OutOfTree);
    }

    setNodePath(file, newPath);
    if (m_mode == Mode::Ephemeral) {
        m_path = newPath.empty() ? currentWorkingPath() : newPath.parent_path();
    }
    return {};
}

void Project::setNodePath(Node* node, const std::filesystem::path& newPath) {
    assert(node != nullptr);

    // m_pathMap is the *file* lookup index — folders aren't openable so
    // they don't participate. Re-key it only when this is a file node.
    const bool isFile = node->isFile();

    if (isFile && !node->path.empty()) {
        m_pathMap.erase(node->path);
    }

    node->path = newPath;
    if (isFile && !newPath.empty()) {
        assert(!m_pathMap.contains(newPath) && "duplicate path in project node index");
        m_pathMap.emplace(newPath, node);
    }
}

void Project::clearNodeDocument(Node* node) {
    assert(node != nullptr);
    if (auto* file = node->getFile()) {
        file->doc = nullptr;
    }
}

// --- Tree-management API ---------------------------------------------------

namespace {

// Validate that no file in the subtree carries a Document* back-link.
// removeNode would otherwise leave Document::Source dangling — callers
// must close the relevant tabs through DocumentManager first.
void assertNoBoundDoc(const Project::Node* node) {
    if (const auto* file = node->getFile()) {
        assert(file->doc == nullptr && "removeNode: file has bound document — close tab first");
        return;
    }
    for (const auto* child : node->getFolder()->children) {
        assertNoBoundDoc(child);
    }
}

// newName must be a single path component — non-empty and free of
// path separators. Reused by rename / future create-folder validation.
auto isValidLeafName(const std::string& name) -> bool {
    return !name.empty()
        && name.find('/') == std::string::npos
        && name.find('\\') == std::string::npos;
}

} // namespace

// NOLINTNEXTLINE(performance-unnecessary-value-param)
auto Project::addFolder(Node* parent, std::string name) -> Node* {
    assert(m_mode == Mode::Persistent && "folders are Persistent-only");
    if (parent == nullptr) {
        parent = m_root;
    }
    assert(parent != nullptr);
    assert(parent->isFolder() && "parent must be a folder");

    auto node = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = parent,
        .path = {},
        .entry = Node::Folder { .name = std::move(name), .children = {} },
    });
    auto* nodePtr = node.get();
    m_nodes.emplace(node->id, std::move(node));
    parent->getFolder()->children.push_back(nodePtr);
    return nodePtr;
}

auto Project::addRealFolder(Node* parent, std::filesystem::path path) -> std::expected<Node*, Error> {
    assert(m_mode == Mode::Persistent && "folders are Persistent-only");
    if (parent == nullptr) {
        parent = m_root;
    }
    assert(parent != nullptr);
    assert(parent->isFolder() && "parent must be a folder");
    assert(!path.empty() && "addRealFolder requires a path");
    assert(isUnderRoot(path) && "Persistent folder must live under project root");

    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        if (!std::filesystem::is_directory(path, ec)) {
            // Path exists as a non-directory — can't pretend it's a folder.
            return std::unexpected(Error::Clash);
        }
    } else {
        // Create the directory (recursively, so callers don't have to
        // pre-create intermediate parents).
        std::filesystem::create_directories(path, ec);
        if (ec) {
            return std::unexpected(Error::IoError);
        }
    }

    auto name = path.filename().string();
    auto node = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = parent,
        .path = std::move(path),
        .entry = Node::Folder { .name = std::move(name), .children = {} },
    });
    const auto nodeId = node->id;
    auto* nodePtr = node.get();
    m_nodes.emplace(nodeId, std::move(node));
    parent->getFolder()->children.push_back(nodePtr);
    return nodePtr;
}

auto Project::removeNode(Node* node, const bool deleteOnDisk) -> std::expected<void, Error> {
    assert(node != nullptr);
    assert(node != m_root && "cannot remove the project root");
    assertNoBoundDoc(node);

    // Disk delete first (best-effort). Tree mutation runs either way —
    // once the user asks to remove from the project the tree should
    // reach the requested state even if cleanup on disk fails. The
    // expected return carries the first disk failure for the caller to
    // surface.
    std::expected<void, Error> result;
    if (deleteOnDisk) {
        std::error_code firstErr;
        deleteSubtreeFromDisk(node, firstErr);
        if (firstErr) {
            result = std::unexpected(Error::IoError);
        }
    }

    auto& siblings = node->parent->getFolder()->children;
    std::erase(siblings, node);
    destroySubtree(node);

    return result;
}

auto Project::moveNode(Node* node, Node* newParent, const std::size_t index) -> std::expected<void, Error> {
    assert(node != nullptr);
    assert(node != m_root && "cannot move the project root");
    assert(newParent != nullptr);
    assert(newParent->isFolder() && "newParent must be a folder");

    // Cycle prevention: walking newParent's ancestry must never reach node.
    for (const auto* cursor = newParent; cursor != nullptr; cursor = cursor->parent) {
        assert(cursor != node && "moveNode would create a cycle");
    }

    auto& oldChildren = node->parent->getFolder()->children;
    auto& newChildren = newParent->getFolder()->children;
    const bool sameParent = node->parent == newParent;
    // After-move children count for the target — same-parent moves do
    // an erase before the insert, so the upper bound shrinks by one.
    const auto effectiveSize = sameParent ? newChildren.size() - 1 : newChildren.size();
    assert(index <= effectiveSize && "moveNode index out of range");

    // Auto-mv when we're actually changing parents and `node` has a
    // real on-disk artifact. The destination directory is the closest
    // non-virtual ancestor of `newParent` (walking up through virtual
    // folders, capped at `m_root`). If the walk finds no real
    // ancestor — e.g. an untitled Ephemeral or a Persistent whose
    // loader hasn't set a root path — the disk move is skipped and
    // tree mutation alone runs.
    if (!sameParent && !node->path.empty()) {
        const Node* realParent = newParent;
        while (realParent != nullptr && realParent->path.empty()) {
            realParent = realParent->parent;
        }
        if (realParent != nullptr) {
            const auto basename = node->path.filename();
            const auto newPath = realParent->path / basename;
            std::error_code ec;
            if (std::filesystem::exists(newPath, ec)) {
                return std::unexpected(Error::Clash);
            }
            std::filesystem::rename(node->path, newPath, ec);
            if (ec) {
                return std::unexpected(Error::IoError);
            }
            // For a folder, descendants' cached paths need to follow
            // the new prefix; for a file, only the leaf itself.
            if (node->isFolder()) {
                rewriteSubtreePaths(node, node->path, newPath);
            }
            setNodePath(node, newPath);
        }
    }

    std::erase(oldChildren, node);
    newChildren.insert(newChildren.begin() + static_cast<std::ptrdiff_t>(index), node);
    node->parent = newParent;

    return {};
}

auto Project::renameNode(Node* node, std::string newName) -> std::expected<void, Error> {
    assert(node != nullptr);

    if (!isValidLeafName(newName)) {
        return std::unexpected(Error::InvalidName);
    }

    // Virtual folder: display-name only, no disk side-effects.
    if (auto* folder = node->getFolder(); folder != nullptr && node->path.empty()) {
        folder->name = std::move(newName);
        return {};
    }

    assert(!node->path.empty() && "real node must have a non-empty path");

    const auto newPath = node->path.parent_path() / newName;
    std::error_code ec;
    if (std::filesystem::exists(newPath, ec)) {
        return std::unexpected(Error::Clash);
    }
    std::filesystem::rename(node->path, newPath, ec);
    if (ec) {
        return std::unexpected(Error::IoError);
    }

    if (auto* folder = node->getFolder()) {
        // Rewrite descendant paths BEFORE updating this folder's own
        // path — rewriteSubtreePaths uses (oldPrefix, newPrefix) to
        // recompute children, and "old" must still match what the
        // descendants currently store.
        rewriteSubtreePaths(node, node->path, newPath);
        folder->name = newName;
    }
    setNodePath(node, newPath);

    return {};
}

void Project::deleteSubtreeFromDisk(Node* node, std::error_code& firstErr) {
    assert(node != nullptr);

    // Recurse into children first — covers virtual folders whose
    // descendants own real paths, and real folders whose descendants
    // sit outside their parent's directory.
    if (const auto* folder = node->getFolder()) {
        for (auto* child : folder->children) {
            deleteSubtreeFromDisk(child, firstErr);
        }
    }
    if (node->path.empty()) {
        // Virtual folder or untitled file — nothing of our own on disk.
        return;
    }
    std::error_code ec;
    if (node->isFolder()) {
        // Real folder: remove_all cleans up anything still left
        // (descendants already handled their own tracked paths).
        std::filesystem::remove_all(node->path, ec);
    } else {
        std::filesystem::remove(node->path, ec);
    }
    if (ec && !firstErr) {
        firstErr = ec;
    }
}

void Project::destroySubtree(Node* node) {
    assert(node != nullptr);

    // Tear down children first so any path-index entries they own go
    // away in the right order.
    if (const auto* folder = node->getFolder()) {
        // Snapshot — destroying children would otherwise invalidate
        // the live `children` vector we're iterating.
        const auto children = folder->children;
        for (auto* child : children) {
            destroySubtree(child);
        }
    }
    if (!node->path.empty() && node->isFile()) {
        m_pathMap.erase(node->path);
    }
    m_nodes.erase(node->id);
}

void Project::rewriteSubtreePaths(Node* folder, const std::filesystem::path& oldPrefix, const std::filesystem::path& newPrefix) {
    assert(folder != nullptr);
    assert(folder->isFolder());

    for (auto* child : folder->getFolder()->children) {
        const bool childIsFolder = child->isFolder();

        if (child->path.empty()) {
            // Untitled file or virtual folder — nothing to rewrite at
            // this node, but a virtual folder may still own descendants
            // whose paths sit under oldPrefix.
            if (childIsFolder) {
                rewriteSubtreePaths(child, oldPrefix, newPrefix);
            }
            continue;
        }

        std::error_code ec;
        const auto rel = std::filesystem::relative(child->path, oldPrefix, ec);
        if (ec || rel.empty() || rel.native().starts_with("..")) {
            // Child path sits outside oldPrefix — treat as unrelated
            // and don't rewrite this node, but still recurse so
            // descendants under oldPrefix get caught.
            if (childIsFolder) {
                rewriteSubtreePaths(child, oldPrefix, newPrefix);
            }
            continue;
        }

        const auto childNewPath = newPrefix / rel;
        // Recurse first (descendant paths must still match `oldPrefix`
        // at the point they're computed), then update this child.
        if (childIsFolder) {
            rewriteSubtreePaths(child, oldPrefix, newPrefix);
        }
        setNodePath(child, childNewPath);
    }
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
    if (isEphemeral()) {
        if (m_root == nullptr) {
            return {};
        }
        auto* doc = m_root->getFile()->doc;
        return doc != nullptr ? std::vector<Document*> { doc } : std::vector<Document*> {};
    }
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

// --- Build / run gateway ---------------------------------------------------

auto Project::getCompileTemplate() const -> wxString {
    // Ephemeral projects read through ConfigManager so settings edits
    // take effect immediately on the next build. Persistent projects
    // (future) will return a stored override instead.
    assert(isEphemeral() && "Persistent project compile-options not implemented yet");
    return m_config.config().at("compiler").get_or("compileCommand", R"("<$fbc>" "<$file>")");
}

auto Project::getRunTemplate() const -> wxString {
    assert(isEphemeral() && "Persistent project compile-options not implemented yet");
    return m_config.config().get_or("compiler.runCommand", R"(<$terminal> "<$file>" <$param>)");
}

auto Project::getConfigurationSlug() const -> std::optional<wxString> {
    // Ephemeral projects carry their configuration selection on the single
    // bound source document. Persistent projects (future) will store their
    // own slug.
    assert(isEphemeral() && "Persistent project configuration not implemented yet");
    const auto sources = getSources();
    return sources.empty() ? std::optional<wxString> {} : sources.front()->getConfiguration();
}

void Project::setConfigurationSlug(std::optional<wxString> slug) {
    assert(isEphemeral() && "Persistent project configuration not implemented yet");
    const auto sources = getSources();
    if (!sources.empty()) {
        sources.front()->setConfiguration(std::move(slug));
    }
}

auto Project::getCompilerConfig(const CompilerConfigCatalog& catalog) const -> const ResolvedCompilerConfig& {
    return catalog.resolveByPinnedSlug(getConfigurationSlug());
}

auto Project::getMenuConfigurations(const CompilerConfigCatalog& catalog, const wxString& alwaysInclude) const
    -> std::vector<const ResolvedCompilerConfig*> {
    // Ephemeral projects pass the global compiler configurations through
    // unchanged. Persistent projects (future) will instead return their
    // own internally-defined build targets.
    assert(isEphemeral() && "Persistent project build targets not implemented yet");
    return catalog.menuConfigs(alwaysInclude);
}

auto Project::getCapabilities() const -> std::uint8_t {
    // Ephemeral projects host exactly one source file and always
    // produce a runnable executable — every capability applies.
    // Persistent projects (future) will derive this from their stored
    // output kind (Executable / Library / StaticLib / …).
    assert(isEphemeral() && "Persistent project capabilities not implemented yet");
    return +Capability::Compile | +Capability::CompileAndRun | +Capability::Run | +Capability::QuickRun;
}
