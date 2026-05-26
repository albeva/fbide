//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Project.hpp"

using namespace fbide;

namespace {
/// Process-wide monotonic allocator for `Project::Id`. Project creation
/// is constrained to the UI thread today, but an atomic costs nothing
/// and keeps the contract robust if that ever changes. Encapsulated in
/// a function-local static so it doesn't appear as a non-const global.
auto allocateProjectId() -> Project::Id::Underlying {
    static std::atomic<Project::Id::Underlying> counter { 0 };
    return ++counter;
}
} // namespace

Project::Project(const Mode mode)
: m_id(allocateProjectId())
, m_mode(mode) {
    // Synthesise the virtual root folder so every top-level entry has a
    // valid parent. The root is never path-indexed and carries no name.
    const auto rootId = allocateNodeId();
    m_root = rootId;
    m_nodes.emplace(rootId, Node {
        .id = rootId,
        .parent = {},
        .path = std::nullopt,
        .entry = Node::Folder {
            .name = {},
            .children = {},
        },
    });
}

auto Project::addFile(std::optional<std::filesystem::path> path, Document* doc) -> Node::Id {
    const auto id = allocateNodeId();

    // Index by path *before* moving it into the node — m_byPath is the
    // lookup index for resolveOrOpen-style queries; only real (non-empty)
    // paths participate.
    if (path.has_value() && !path->empty()) {
        m_byPath.emplace(*path, id);
    }

    m_nodes.emplace(id, Node {
        .id = id,
        .parent = m_root,
        .path = std::move(path),
        .entry = Node::File { .doc = doc },
    });

    // Maintain the root's children list so tree-walking code can iterate
    // in insertion order even though m_nodes itself is unordered.
    std::get<Node::Folder>(m_nodes.at(m_root).entry).children.push_back(id);
    return id;
}

auto Project::getNodePath(const Node::Id id) const -> std::filesystem::path {
    const auto it = m_nodes.find(id);
    if (it == m_nodes.end()) {
        return {};
    }
    return it->second.path.value_or(std::filesystem::path {});
}

void Project::setNodePath(const Node::Id id, const std::filesystem::path& path) {
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) {
        return;
    }

    // Re-key the path index: drop the old key (if any) before inserting
    // the new one, otherwise two entries point at the same node.
    if (it->second.path.has_value() && !it->second.path->empty()) {
        m_byPath.erase(*it->second.path);
    }

    it->second.path = path.empty() ? std::optional<std::filesystem::path> {} : std::optional { path };

    if (!path.empty()) {
        m_byPath.emplace(path, id);
    }
}

auto Project::getPrimarySource() const -> Document* {
    assert(m_mode == Mode::Ephemeral && "getPrimarySource is ephemeral-only");

    for (const auto& [id, node] : m_nodes) {
        if (const auto* file = std::get_if<Node::File>(&node.entry)) {
            return file->doc;
        }
    }
    return nullptr;
}

auto Project::getDocuments() const -> std::vector<Document*> {
    std::vector<Document*> result;
    result.reserve(m_nodes.size());
    for (const auto& [id, node] : m_nodes) {
        if (const auto* file = std::get_if<Node::File>(&node.entry); file != nullptr && file->doc != nullptr) {
            result.push_back(file->doc);
        }
    }
    return result;
}

auto Project::allocateNodeId() -> Node::Id {
    return Node::Id { m_nextNodeId++ };
}
