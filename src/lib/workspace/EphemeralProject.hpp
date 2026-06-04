//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ProjectBase.hpp"

namespace fbide {
class Document;

/**
 * A throwaway project auto-created for a single standalone FreeBASIC
 * document — preserves the "new tab → type → run" experience. It owns
 * the source file's path (`m_path`, authoritative — the bound `Document`
 * reads it back through `getFilePath`), the selected compiler-config slug
 * (`m_configuration`), and a non-owning back-link to the document. No
 * node tree: there is only ever one file. Lives only while the document
 * stays a FreeBASIC source; `WorkspaceManager` creates it on type-in and
 * tears it down on close / type change.
 *
 * Build configuration is the *active* compiler configuration: the
 * selected slug is the project's own, and the dropdown shows the
 * catalog's menu-visible compiler configurations verbatim.
 */
class EphemeralProject final : public ProjectBase {
public:
    /// Bind to `doc` against the shared `catalog`, capturing the
    /// document's current path as the project's authoritative source path.
    EphemeralProject(CompilerConfigCatalog& catalog, Document& doc);

    [[nodiscard]] auto isEphemeral() const -> bool override { return true; }

    /// The single source document — reported only while it is still bound
    /// to this project (mirrors the persistent tree's "File::doc != null"
    /// so teardown doesn't re-enter via a stale link).
    [[nodiscard]] auto getDocuments() const -> std::vector<Document*> override;
    [[nodiscard]] auto getSources() const -> std::vector<Document*> override;

    /// No tree, no root constraint — the single file moves freely.
    [[nodiscard]] auto isUnderRoot(const std::filesystem::path& /*candidate*/) const -> bool override { return true; }

    /// The source file's path — authoritative. The bound `Document`
    /// delegates `getFilePath` / `setFilePath` here.
    [[nodiscard]] auto getPath() const -> const std::filesystem::path& { return m_path; }
    void setPath(const std::filesystem::path& path) { m_path = path; }

    /// The selected compiler-configuration slug — owned by the project.
    [[nodiscard]] auto getConfigurationSlug() const -> std::optional<wxString> override { return m_configuration; }
    void setConfigurationSlug(std::optional<wxString> slug) override { m_configuration = std::move(slug); }

    /// Pass the catalog's menu-visible compiler configurations through.
    [[nodiscard]] auto getMenuConfigurations(const wxString& alwaysInclude) const
        -> std::vector<const ResolvedCompilerConfig*> override;

    /// Single-file executable — every build action applies.
    [[nodiscard]] auto getCapabilities() const -> std::uint8_t override;

private:
    Document* m_document;                    ///< Bound source document (non-owning).
    std::filesystem::path m_path;            ///< Authoritative source-file path.
    std::optional<wxString> m_configuration; ///< Pinned compiler-config slug; empty = follow active.
};

} // namespace fbide
