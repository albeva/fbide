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
    Sub,
    Function,
    Type,
    Union,
    Enum,
};

/// One captured declaration. `line` is 0-based and refers to the opener
/// (e.g., the `Sub` keyword line), suitable for `wxStyledTextCtrl::GotoLine`.
struct Symbol {
    SymbolKind kind;
    wxString   name;
    int        line = 0;
};

/// Per-document table of captured declarations. Vectors are filled in source
/// order. `hash` lets consumers (UI) skip work when nothing meaningful changed
/// between two parses.
///
/// Construct directly from a (lean) `ProgramTree`. The walk captures
/// top-level Sub / Function / Type / Union / Enum, recurses into Namespace
/// bodies (flat list — no qualified names for now), skips anonymous
/// declarations, and intentionally ignores Constructor / Destructor /
/// Operator at this stage.
class SymbolTable final {
public:
    SymbolTable() = default;
    explicit SymbolTable(const reformat::ProgramTree& tree);

    [[nodiscard]] auto getSubs() const -> const std::vector<Symbol>& { return m_subs; }
    [[nodiscard]] auto getFunctions() const -> const std::vector<Symbol>& { return m_functions; }
    [[nodiscard]] auto getTypes() const -> const std::vector<Symbol>& { return m_types; }
    [[nodiscard]] auto getUnions() const -> const std::vector<Symbol>& { return m_unions; }
    [[nodiscard]] auto getEnums() const -> const std::vector<Symbol>& { return m_enums; }

    /// Stable hash over (kind, name, line) of every symbol in canonical
    /// (subs, functions, types, unions, enums) order. Set during construction.
    [[nodiscard]] auto getHash() const -> std::size_t { return m_hash; }

private:
    void walkNodes(const std::vector<reformat::Node>& nodes);
    void walkBlock(const reformat::BlockNode& block);
    void emit(SymbolKind kind,
        const std::vector<lexer::Token>& opener,
        std::size_t keywordIdx);
    void computeHash();

    std::vector<Symbol> m_subs;
    std::vector<Symbol> m_functions;
    std::vector<Symbol> m_types;
    std::vector<Symbol> m_unions;
    std::vector<Symbol> m_enums;
    std::size_t m_hash = 0;
};

} // namespace fbide
