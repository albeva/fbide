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
 * The single, session-long project that owns every **standalone** document —
 * any file open in the IDE that isn't a member of a persistent `Project`.
 * `WorkspaceManager` creates it lazily on first use and never destroys it;
 * documents come and go inside it, so there is no per-file project churn and
 * `Document::getProject()` is never null at runtime.
 *
 * It isn't a "project" in the persistent sense — its build context (sources,
 * capabilities, compiler-configuration dropdown) reflects the **currently
 * active** standalone document (`m_active`), so compile / run always target
 * the focused file. A non-FreeBASIC active document advertises no
 * capabilities and no build configurations.
 */
class EphemeralProject final : public ProjectBase {
public:
    /// Empty container bound to the shared compiler-configuration catalog.
    explicit EphemeralProject(CompilerConfigCatalog& catalog);

    /// Out-of-line so the owned-document `unique_ptr`s see the full
    /// `Document` definition when they tear down.
    ~EphemeralProject() override;

    [[nodiscard]] auto isEphemeral() const -> bool override { return true; }

    /// Take ownership of a standalone document and bind it to this project.
    /// Returns the (non-owning) pointer for the caller's continued use.
    auto adopt(std::unique_ptr<Document> doc) -> Document*;

    /// Destroy a document this project owns — called when its tab closes.
    void remove(Document* doc);

    /// Point the build context at the focused standalone document (or
    /// `nullptr` when the active document belongs elsewhere). Drives
    /// `getSources` / `getCapabilities` / `getMenuConfigurations`.
    void setActive(Document* doc);

    /// Every standalone document currently open — for enumeration / close-all.
    [[nodiscard]] auto getDocuments() const -> std::vector<Document*> override;

    /// The build target: the active document when it is FreeBASIC, else none.
    [[nodiscard]] auto getSources() const -> std::vector<Document*> override;

    /// No tree, no root constraint — standalone files move freely.
    [[nodiscard]] auto isUnderRoot(const std::filesystem::path& /*candidate*/) const -> bool override { return true; }

    /// Selected compiler-configuration slug — shared across all standalone
    /// files (empty = follow the active configuration).
    [[nodiscard]] auto getConfigurationSlug() const -> std::optional<wxString> override { return m_configuration; }
    void setConfigurationSlug(std::optional<wxString> slug) override { m_configuration = std::move(slug); }

    /// The catalog's menu-visible configurations when the active document is
    /// FreeBASIC; empty otherwise (no dropdown for a non-FB file).
    [[nodiscard]] auto getMenuConfigurations(const wxString& alwaysInclude) const
        -> std::vector<const ResolvedCompilerConfig*> override;

    /// All four capabilities when the active document is FreeBASIC, none otherwise.
    [[nodiscard]] auto getCapabilities() const -> std::uint8_t override;

private:
    std::vector<std::unique_ptr<Document>> m_documents; ///< Owned standalone documents.
    Document* m_active = nullptr;                       ///< Focused standalone doc (build context); may be null.
    std::optional<wxString> m_configuration;            ///< Shared pinned compiler-config slug; empty = follow active.
};

} // namespace fbide
