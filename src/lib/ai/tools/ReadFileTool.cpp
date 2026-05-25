//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ReadFileTool.hpp"
#include <nlohmann/json.hpp>
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {

constexpr auto kSchema = R"({
"type":"object",
"properties":{
"path":{"type":"string","description":"Relative or absolute path to a file in the workspace."}
},
"required":["path"]
})";

constexpr auto kDescription = "Read a file from the workspace by path. "
                              "Paths resolve against the active document's "
                              "directory; absolute paths must lie inside that "
                              "subtree or match an open tab. Returns the file's "
                              "text (UTF-8), capped at 256 KB.";

/// True when `child` is `root` or under `root` lexically (after
/// normalisation). Used after both inputs are normalised; we
/// intentionally don't resolve symlinks here — accepting "this is the
/// path the model asked for" is fine because the read itself is
/// read-only and bounded.
auto isUnder(const std::filesystem::path& child, const std::filesystem::path& root) -> bool {
    if (root.empty()) {
        return false;
    }
    const auto rootStr = root.lexically_normal().string();
    const auto childStr = child.lexically_normal().string();
    if (childStr == rootStr) {
        return true;
    }
    return childStr.starts_with(rootStr + std::filesystem::path::preferred_separator);
}

/// True when `path` contains a `..` segment that would walk above its
/// origin (after normalisation any over-walk leaves a leading `..`).
auto containsParentEscape(const std::filesystem::path& path) -> bool {
    const auto normalised = path.lexically_normal();
    return std::ranges::any_of(normalised, [](const auto& component) {
        return component == "..";
    });
}

} // namespace

ReadFileTool::ReadFileTool(DocumentManager& documents)
: m_documents(documents) {}

auto ReadFileTool::descriptor() const -> AiTool {
    return AiTool {
        .name = kName,
        .description = kDescription,
        .inputSchemaJson = kSchema,
    };
}

auto ReadFileTool::resolveSafePath(
    const std::string& pathArg,
    const std::filesystem::path& activeDocDir,
    const std::vector<std::filesystem::path>& openPaths
) -> std::optional<std::filesystem::path> {
    if (pathArg.empty()) {
        return std::nullopt;
    }
    const std::filesystem::path raw(pathArg);

    std::filesystem::path resolved;
    if (raw.is_absolute()) {
        resolved = raw.lexically_normal();
    } else {
        if (containsParentEscape(raw)) {
            // A `..` in a relative path is the most common escape attempt;
            // reject before joining so the user's active dir can't be
            // walked above.
            return std::nullopt;
        }
        if (activeDocDir.empty()) {
            // No anchor for relative paths — fall through to the
            // open-tab name match below using `raw` as-is.
            for (const auto& open : openPaths) {
                if (!open.empty() && open.filename() == raw.filename()) {
                    return open.lexically_normal();
                }
            }
            return std::nullopt;
        }
        resolved = (activeDocDir / raw).lexically_normal();
    }

    if (containsParentEscape(resolved)) {
        return std::nullopt;
    }

    // In-subtree match — the common case for relative paths and for
    // absolute paths that the model derived from an `#include` it
    // saw in already-attached content.
    if (!activeDocDir.empty() && isUnder(resolved, activeDocDir)) {
        return resolved;
    }
    // Open-tab match — by absolute path first (exact), then by
    // filename (model may not know full path).
    for (const auto& open : openPaths) {
        if (open.empty()) {
            continue;
        }
        auto normalised = open.lexically_normal();
        if (normalised == resolved || normalised.filename() == resolved.filename()) {
            return normalised;
        }
    }
    return std::nullopt;
}

auto ReadFileTool::readCapped(const std::filesystem::path& path) -> wxString {
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec) {
        return wxString::Format("[error: cannot stat '%s']", toWx(path));
    }
    if (bytes > kMaxBytes) {
        return wxString::Format(
            "[error: file '%s' is %llu bytes; exceeds %zu byte cap]",
            toWx(path),
            static_cast<unsigned long long>(bytes),
            kMaxBytes
        );
    }
    wxString content;
    wxFFile file(toWx(path), "rb");
    if (!file.IsOpened() || !file.ReadAll(&content, wxConvUTF8)) {
        return wxString::Format("[error: cannot read '%s']", toWx(path));
    }
    return content;
}

void ReadFileTool::invoke(AiToolCall call, ResultHandler handler) {
    // Parse arguments — the wire form is raw JSON, model-emitted, so
    // assume nothing and surface parse failures as tool errors.
    const auto args = json::parse(call.argumentsJson.utf8_string(), nullptr, false);
    if (args.is_discarded() || !args.is_object()) {
        handler(AiToolResult {
            .toolUseId = std::move(call.id),
            .content = "arguments must be a JSON object with a 'path' string",
            .isError = true,
        });
        return;
    }
    const auto pathField = args.find("path");
    if (pathField == args.end() || !pathField->is_string()) {
        handler(AiToolResult {
            .toolUseId = std::move(call.id),
            .content = "'path' argument is required and must be a string",
            .isError = true,
        });
        return;
    }
    const auto pathArg = pathField->get<std::string>();

    // Build the scope context from DocumentManager — the active doc's
    // dir defines the subtree, and every open document contributes a
    // candidate path for the open-tab match.
    std::filesystem::path activeDocDir;
    if (auto* active = m_documents.getActive(); active != nullptr) {
        if (const auto& activePath = active->getFilePath(); !activePath.empty()) {
            activeDocDir = activePath.parent_path();
        }
    }
    std::vector<std::filesystem::path> openPaths;
    for (const auto& doc : m_documents.getDocuments()) {
        if (!doc) {
            continue;
        }
        if (const auto& docPath = doc->getFilePath(); !docPath.empty()) {
            openPaths.push_back(docPath);
        }
    }

    const auto resolved = resolveSafePath(pathArg, activeDocDir, openPaths);
    if (!resolved) {
        handler(AiToolResult {
            .toolUseId = std::move(call.id),
            .content = wxString::Format(
                "Path '%s' is outside the active document's directory and does not match an open tab.",
                wxString::FromUTF8(pathArg)
            ),
            .isError = true,
        });
        return;
    }

    const auto content = readCapped(*resolved);
    const bool isError = content.StartsWith("[error:");
    handler(AiToolResult {
        .toolUseId = std::move(call.id),
        .content = content,
        .isError = isError,
    });
}
