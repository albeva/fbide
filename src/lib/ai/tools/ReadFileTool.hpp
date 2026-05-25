//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ToolRegistry.hpp"

namespace fbide {
class DocumentManager;
} // namespace fbide

namespace fbide::ai {

/**
 * `read_file(path)` — let the model pull additional context from the
 * workspace on demand.
 *
 * **Scoping.** fbide has no project model, so the tool's "workspace"
 * is whatever the active document's directory + open tabs implies:
 *
 * - Relative paths resolve against the active document's directory.
 * - Absolute paths must lie inside that directory's subtree, OR
 *   match an open tab's path / filename.
 * - `..` segments are forbidden after lexical normalisation.
 * - Reads cap at `kMaxBytes` to keep the model from yanking enormous
 *   binaries into the context window.
 *
 * The scope rules live in static helpers (`resolveSafePath`,
 * `readCapped`) so they can be tested without standing up a
 * `DocumentManager` instance.
 */
class ReadFileTool final : public Tool {
public:
    NO_COPY_AND_MOVE(ReadFileTool)

    /// Maximum bytes returned from a single read. Larger files are
    /// reported as an error so the model can attach only what it needs.
    static constexpr std::size_t kMaxBytes = std::size_t { 256 } * 1024;

    /// Tool name as the model invokes it. Public so the registry and
    /// tests can refer to it without string duplication.
    static constexpr auto kName = "read_file";

    explicit ReadFileTool(DocumentManager& documents);
    ~ReadFileTool() override = default;

    [[nodiscard]] auto descriptor() const -> AiTool override;
    void invoke(AiToolCall call, ResultHandler handler) override;

    /// Resolve `pathArg` against `activeDocDir` (relative) or take it
    /// as-is (absolute), normalise, and check it sits in scope:
    ///
    /// - Under `activeDocDir`'s subtree, OR
    /// - Matches an entry in `openPaths` by absolute path, OR
    /// - Matches an entry in `openPaths` by filename only.
    ///
    /// Returns the resolved absolute path on success, std::nullopt on
    /// rejection (out-of-scope, `..` escape, or empty input).
    [[nodiscard]] static auto resolveSafePath(
        const std::string& pathArg,
        const std::filesystem::path& activeDocDir,
        const std::vector<std::filesystem::path>& openPaths
    ) -> std::optional<std::filesystem::path>;

    /// Read `path` capped at `kMaxBytes`. Returns the content on
    /// success or a `[error: ...]` placeholder when the file is
    /// missing, unreadable, or exceeds the cap. The "error" form is
    /// human text — the tool wraps it into an `AiToolResult` with
    /// `isError = true` for the model.
    [[nodiscard]] static auto readCapped(const std::filesystem::path& path) -> wxString;

private:
    DocumentManager& m_documents;
};

} // namespace fbide::ai
