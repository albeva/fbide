//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "format/transformers/reformat/FormatTree.hpp"

namespace fbide {

/// Top-level declaration kinds we surface from a parse tree. Restricted to
/// the categories the old fbide Sub/Function browser showed; namespaces are
/// recursed into but don't appear as a kind themselves.
enum class SymbolKind : std::uint8_t {
    Sub,      ///< `Sub` declaration.
    Function, ///< `Function` declaration.
    Type,     ///< `Type` declaration.
    Union,    ///< `Union` declaration.
    Enum,     ///< `Enum` declaration.
    Macro,    ///< `#macro` definition.
    Include,  ///< `#include` directive.
};

/// One captured declaration. `line` is 0-based and refers to the opener
/// (e.g., the `Sub` keyword line), suitable for `wxStyledTextCtrl::GotoLine`.
struct Symbol {
    SymbolKind kind; ///< Declaration kind.
    wxString name;   ///< Declared identifier.
    int line = 0;    ///< 0-based source line of the opener.
};

/// One captured `#include` (or `#include once`) directive. `path` is the
/// literal quoted text with quotes stripped — no resolution (the caller
/// resolves it against source dir / compiler `inc/` / cwd at navigation
/// time). Found anywhere in the source, including inside conditional blocks.
struct Include {
    wxString path; ///< Quoted include target (quotes stripped, unresolved).
    int line = 0;  ///< 0-based source line of the directive.
};

/**
 * Per-document table of captured declarations.
 *
 * Vectors are filled in source order; `m_hash` is a stable hash over
 * (kind, name) pairs only (line numbers do not participate) so
 * consumers can skip rebuilds when nothing meaningful changed between
 * two parses.
 *
 * Constructed directly from a (lean) `ProgramTree`. The walk:
 *
 * - Captures top-level Sub / Function / Type / Union / Enum.
 * - Recurses into Namespace bodies (flat list — no qualified names
 *   for now).
 * - Skips anonymous declarations.
 * - Intentionally ignores Constructor / Destructor / Operator at
 *   this stage.
 *
 * Pooled by `IntellisenseService` — `populate` rewalks while keeping
 * vector capacities, and `reset` clears without freeing.
 *
 * See @ref analyses.
 */
class SymbolTable final {
public:
    /// Default-constructed empty table.
    SymbolTable() = default;
    /// Build by walking `tree` once.
    explicit SymbolTable(const reformat::ProgramTree& tree);

    /// Clear all vectors (keeping their capacity) and rewalk `tree`. Used by
    /// `IntellisenseService` to recycle a pooled instance instead of
    /// allocating a fresh one on every parse.
    void populate(const reformat::ProgramTree& tree);

    /// `Sub` declarations in source order.
    [[nodiscard]] auto getSubs() const -> const std::vector<Symbol>& { return m_subs; }
    /// `Function` declarations in source order.
    [[nodiscard]] auto getFunctions() const -> const std::vector<Symbol>& { return m_functions; }
    /// `Type` declarations in source order.
    [[nodiscard]] auto getTypes() const -> const std::vector<Symbol>& { return m_types; }
    /// `Union` declarations in source order.
    [[nodiscard]] auto getUnions() const -> const std::vector<Symbol>& { return m_unions; }
    /// `Enum` declarations in source order.
    [[nodiscard]] auto getEnums() const -> const std::vector<Symbol>& { return m_enums; }
    /// `#macro` definitions in source order.
    [[nodiscard]] auto getMacros() const -> const std::vector<Symbol>& { return m_macros; }
    /// `#include` directives in source order.
    [[nodiscard]] auto getIncludes() const -> const std::vector<Include>& { return m_includes; }

    /// Lookup an `#include` directive by its source line (0-based).
    /// Returns `nullptr` when the line carries no recognised include.
    [[nodiscard]] auto findIncludeAt(int line) const -> const Include*;

    /// Stable hash over (kind, name) of visible symbol names and kinds.
    /// Hash does not include individual line numbers, etc.
    [[nodiscard]] auto getHash() const -> std::size_t { return m_hash; }

    /// Reset the table while preserving allocated memory.
    void reset();

private:
    /// Recursively walk a node list at any depth.
    void walkNodes(const std::vector<reformat::Node>& nodes);
    /// Process a single block — emit its opener, then recurse into the body.
    void walkBlock(const reformat::BlockNode& block);
    /// Collect every `#include` directive in `nodes` (recurses into blocks).
    void collectIncludes(const std::vector<reformat::Node>& nodes);
    /// If `tokens` is a recognised `#include`, push it onto `m_includes`.
    void tryAddInclude(const std::vector<lexer::Token>& tokens);
    /// Push one symbol drawn from an opener's tokens at `keywordIdx`.
    void emit(SymbolKind kind,
        const std::vector<lexer::Token>& opener,
        std::size_t keywordIdx);
    /// Recompute `m_hash` from the captured (kind, name) pairs.
    void computeHash();

    std::vector<Symbol> m_subs;      ///< `Sub` declarations.
    std::vector<Symbol> m_functions; ///< `Function` declarations.
    std::vector<Symbol> m_types;     ///< `Type` declarations.
    std::vector<Symbol> m_unions;    ///< `Union` declarations.
    std::vector<Symbol> m_enums;     ///< `Enum` declarations.
    std::vector<Symbol> m_macros;    ///< `#macro` definitions.
    std::vector<Include> m_includes; ///< `#include` directives.
    std::size_t m_hash = 0;          ///< Stable hash over (kind, name) pairs.
};

} // namespace fbide
