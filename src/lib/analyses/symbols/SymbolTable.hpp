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

/// Top-level declaration kinds we surface from a parse tree. Namespaces are
/// recursed into but don't appear as a kind themselves. The numeric order is
/// load-bearing: `SymbolBrowser` uses `static_cast<int>(kind)` as an image-list
/// index, so new kinds must be appended at matching positions there.
enum class SymbolKind : std::uint8_t {
    Sub,         ///< `Sub` definition.
    Function,    ///< `Function` definition.
    Constructor, ///< `Constructor` definition.
    Destructor,  ///< `Destructor` definition.
    Operator,    ///< `Operator` definition (free-standing or UDT member).
    Property,    ///< `Property` definition.
    Type,        ///< `Type` declaration.
    Union,       ///< `Union` declaration.
    Enum,        ///< `Enum` declaration.
    Macro,       ///< `#macro` definition.
    Include,     ///< `#include` directive.
};

/// One captured declaration. `line` is 0-based and refers to the opener
/// (e.g., the `Sub` keyword line), suitable for `wxStyledTextCtrl::GotoLine`.
/// A negative `line` marks a synthetic `Type` entry — an undeclared owner of
/// a method (`Sub Foo.Bar` with no `Type Foo`) — which exists only to group
/// its members and has no navigable location.
struct Symbol {
    SymbolKind kind; ///< Declaration kind.
    wxString name;   ///< Declared name; qualified (`Type.Method`) for methods.
    int line = 0;    ///< 0-based source line of the opener; negative if synthetic.
};

/// Owning UDT of a member symbol, or an empty string for a free-standing one.
/// `Constructor` / `Destructor` are always members — their whole name is the
/// owning type. `Sub` / `Function` / `Operator` / `Property` are members when
/// method-qualified (`Owner.member`); the owner is the text before the final
/// dot. Other kinds are never members.
[[nodiscard]] auto symbolOwner(const Symbol& sym) -> wxString;

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
 * - Captures top-level Sub / Function / Constructor / Destructor /
 *   Operator / Property / Type / Union / Enum.
 * - Keeps the qualified name for methods (`Sub Type.Method`,
 *   `Operator Type.+`).
 * - Recurses into Namespace bodies (flat list — no qualified names
 *   for now).
 * - Skips anonymous declarations.
 * - Only definitions are captured; `Declare`d prototypes are not.
 * - Synthesises a `Type` entry for any method owner that is not itself
 *   declared, so the browser can group members under it. Synthetic
 *   types carry a negative `line`.
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
    explicit SymbolTable(reformat::ProgramTree&& tree);

    /// Clear all vectors (keeping their capacity) and rewalk `tree`. Used by
    /// `IntellisenseService` to recycle a pooled instance instead of
    /// allocating a fresh one on every parse.
    void populate(reformat::ProgramTree&& tree);

    /// The retained scope tree (the lean `ProgramTree` this table was built
    /// from). Carries `BlockNode::parent` links and positioned tokens for
    /// scope/keyword matching. Lives as long as this table's `shared_ptr`.
    [[nodiscard]] auto tree() const -> const reformat::ProgramTree& { return m_tree; }
    /// Move the retained tree out (leaving this table's tree empty) so the
    /// next parse can recycle its nodes. Used by `IntellisenseService`.
    [[nodiscard]] auto takeTree() -> reformat::ProgramTree { return std::move(m_tree); }

    /// Innermost block whose text extent contains `pos` (a document byte
    /// offset), or `nullptr` when `pos` lies outside every block. Backed by a
    /// flat index sorted by start: O(log n) plus a short walk to the innermost.
    [[nodiscard]] auto blockAt(int pos) const -> const reformat::BlockNode*;

    /// Keyword spans to highlight when the caret at `pos` sits on a block's
    /// opener or closer keyword: the opener keyword plus its matching closer
    /// (`For`/`Next`, `Sub`/`End Sub`, ...). Empty when `pos` is not on one.
    [[nodiscard]] auto matchBlockAt(int pos) const -> std::vector<std::pair<int, int>>;
    /// Opener + closer keyword spans of the procedure (Sub / Function /
    /// Constructor / ...) enclosing `pos` — for `Return`. Empty when `pos`
    /// is not inside a procedure.
    [[nodiscard]] auto matchProcedureAt(int pos) const -> std::vector<std::pair<int, int>>;

    /// `Sub` definitions in source order.
    [[nodiscard]] auto getSubs() const -> const std::vector<Symbol>& { return m_subs; }
    /// `Function` definitions in source order.
    [[nodiscard]] auto getFunctions() const -> const std::vector<Symbol>& { return m_functions; }
    /// `Constructor` definitions in source order.
    [[nodiscard]] auto getConstructors() const -> const std::vector<Symbol>& { return m_constructors; }
    /// `Destructor` definitions in source order.
    [[nodiscard]] auto getDestructors() const -> const std::vector<Symbol>& { return m_destructors; }
    /// `Operator` definitions in source order.
    [[nodiscard]] auto getOperators() const -> const std::vector<Symbol>& { return m_operators; }
    /// `Property` definitions in source order.
    [[nodiscard]] auto getProperties() const -> const std::vector<Symbol>& { return m_properties; }
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
    /// Rebuild `m_scopes` from the retained tree (pre-order, ascending start).
    void buildScopeIndex();
    /// Append every block under `nodes` (and its descendants) to `m_scopes`.
    void indexBlocks(const std::vector<reformat::Node>& nodes);
    /// Collect every `#include` directive in `nodes` (recurses into blocks).
    void collectIncludes(const std::vector<reformat::Node>& nodes);
    /// If `tokens` is a recognised `#include`, push it onto `m_includes`.
    void tryAddInclude(const std::vector<lexer::Token>& tokens);
    /// Push one symbol drawn from an opener's tokens at `keywordIdx`.
    void emit(SymbolKind kind,
        const std::vector<lexer::Token>& opener,
        std::size_t keywordIdx);
    /// Append a synthetic `Type` (negative line) for every method owner that
    /// is not already a declared type, so members can be grouped under it.
    void synthesizeOwnerTypes();
    /// Recompute `m_hash` from the captured (kind, name) pairs.
    void computeHash();

    std::vector<Symbol> m_subs;         ///< `Sub` definitions.
    std::vector<Symbol> m_functions;    ///< `Function` definitions.
    std::vector<Symbol> m_constructors; ///< `Constructor` definitions.
    std::vector<Symbol> m_destructors;  ///< `Destructor` definitions.
    std::vector<Symbol> m_operators;    ///< `Operator` definitions.
    std::vector<Symbol> m_properties;   ///< `Property` definitions.
    std::vector<Symbol> m_types;        ///< `Type` declarations.
    std::vector<Symbol> m_unions;       ///< `Union` declarations.
    std::vector<Symbol> m_enums;        ///< `Enum` declarations.
    std::vector<Symbol> m_macros;       ///< `#macro` definitions.
    std::vector<Include> m_includes;    ///< `#include` directives.
    std::size_t m_hash = 0;             ///< Stable hash over (kind, name) pairs.
    reformat::ProgramTree m_tree;       ///< Retained lean scope tree (parents wired, tokens positioned).
    /// One block's text extent, pointing into the retained tree.
    struct ScopeRange {
        int start;                        ///< First opener-token byte offset.
        int end;                          ///< One past the block's last token.
        const reformat::BlockNode* block; ///< The block (into `m_tree`).
    };
    std::vector<ScopeRange> m_scopes;   ///< Block extents, sorted by start, for `blockAt`.
};

} // namespace fbide
