//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Project.hpp"
#include "config/ConfigManager.hpp"

using namespace fbide;

Project::Project(ConfigManager& config, const Mode mode)
: m_config(config)
, m_id(Id::generate())
, m_mode(mode) {
    if (mode == Mode::Persistent) {
        // Persistent projects host arbitrary trees; synthesise a virtual
        // root folder so top-level entries have a valid parent. Ephemeral
        // projects skip this — their single `File` becomes the root.
        auto root = std::make_unique<Node>(Node {
            .id = Node::Id::generate(),
            .parent = nullptr,
            .path = {},
            .entry = Node::Folder {
                .name = {},
                .children = {},
            },
        });
        const auto rootId = root->id;
        m_root = root.get();
        m_nodes.emplace(rootId, std::move(root));
    }
}

auto Project::addFile(Node* parent, std::filesystem::path path, Document* doc) -> Node* {
    if (m_mode == Mode::Ephemeral) {
        assert(parent == nullptr && "Ephemeral projects do not take a parent");
        assert(m_root == nullptr && "Ephemeral projects host exactly one file");
    } else {
        assert(parent != nullptr && "Persistent file requires a parent folder");
        assert(std::holds_alternative<Node::Folder>(parent->entry) && "parent must be a folder");
    }

    if (!path.empty()) {
        // m_byPath must stay one-key-one-node — a collision would mask
        // one of the entries and silently break "is this path mine?"
        // queries. Trap in debug.
        assert(!m_byPath.contains(path) && "duplicate path in project node index");
    }

    auto node = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = parent,
        .path = std::move(path),
        .entry = Node::File { .doc = doc },
    });
    const auto nodeId = node->id;
    auto* nodePtr = node.get();
    if (!nodePtr->path.empty()) {
        m_byPath.emplace(nodePtr->path, nodePtr);
    }
    m_nodes.emplace(nodeId, std::move(node));

    if (m_mode == Mode::Ephemeral) {
        m_root = nodePtr;
    } else {
        std::get<Node::Folder>(parent->entry).children.push_back(nodePtr);
    }
    return nodePtr;
}

auto Project::findNode(const Node::Id id) -> Node* {
    const auto it = m_nodes.find(id);
    return it != m_nodes.end() ? it->second.get() : nullptr;
}

auto Project::findNode(const Node::Id id) const -> const Node* {
    const auto it = m_nodes.find(id);
    return it != m_nodes.end() ? it->second.get() : nullptr;
}

void Project::setNodePath(Node* node, const std::filesystem::path& path) {
    assert(node != nullptr);

    // Re-key the path index: drop the old key (if any) before inserting
    // the new one. Collisions on the new key are an upstream caller bug.
    if (!node->path.empty()) {
        m_byPath.erase(node->path);
    }
    node->path = path;
    if (!path.empty()) {
        assert(!m_byPath.contains(path) && "duplicate path in project node index");
        m_byPath.emplace(path, node);
    }
}

void Project::clearNodeDocument(Node* node) {
    assert(node != nullptr);
    if (auto* file = std::get_if<Node::File>(&node->entry)) {
        file->doc = nullptr;
    }
}

auto Project::getPrimarySource() const -> Document* {
    assert(m_mode == Mode::Ephemeral && "getPrimarySource is ephemeral-only");

    // Ephemeral: the file IS the root. Empty until `addFile` runs.
    if (m_root == nullptr) {
        return nullptr;
    }
    return std::get<Node::File>(m_root->entry).doc;
}

auto Project::getDocuments() const -> std::vector<Document*> {
    std::vector<Document*> result;
    result.reserve(m_nodes.size());
    for (const auto& nodePtr : m_nodes | std::views::values) {
        if (const auto* file = std::get_if<Node::File>(&nodePtr->entry); file != nullptr && file->doc != nullptr) {
            result.push_back(file->doc);
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

auto Project::getCapabilities() const -> std::uint8_t {
    // Ephemeral projects host exactly one source file and always
    // produce a runnable executable — every capability applies.
    // Persistent projects (future) will derive this from their stored
    // output kind (Executable / Library / StaticLib / …).
    assert(isEphemeral() && "Persistent project capabilities not implemented yet");
    return +Capability::Compile | +Capability::CompileAndRun | +Capability::Run | +Capability::QuickRun;
}
