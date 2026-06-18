//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "analyses/parser/ProgramTree.hpp"

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
    SymbolKind kind;          ///< Declaration kind.
    wxString name;            ///< Declared name; qualified (`Type.Method`) for methods.
    int line = 0;             ///< 0-based source line of the opener; negative if synthetic.
    bool declaration = false; ///< True for a forward `Declare` (vs a definition).
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
 * A fresh instance is built per parse and published as an immutable
 * `shared_ptr` — held by the UI and by other documents' include closures —
 * so a table is never mutated after publishing or reused for another parse.
 *
 * See @ref analyses.
 */
class SymbolTable final {
public:
    /// Default-constructed empty table.
    SymbolTable() = default;
    /// Build by walking `tree` once.
    explicit SymbolTable(parser::ProgramTree&& tree);

    /// Walk `tree` once to fill the table. Expects a freshly constructed
    /// (empty) instance — the table is build-once and is not cleared here.
    /// `defines` (the active `#if` define set) is shared, not copied, so every
    /// table in a parse drain can point at the one instance; null means none.
    void populate(parser::ProgramTree&& tree,
        std::shared_ptr<const std::unordered_set<std::string>> defines = {});

    /// The retained scope tree (the lean `ProgramTree` this table was built
    /// from). Carries `BlockNode::parent` links and positioned tokens for
    /// scope/keyword matching. Lives as long as this table's `shared_ptr`.
    [[nodiscard]] auto tree() const -> const parser::ProgramTree& {
        static const parser::ProgramTree kEmpty;
        return m_tree ? *m_tree : kEmpty;
    }

    /// Innermost block whose text extent contains `pos` (a document byte
    /// offset), or `nullptr` when `pos` lies outside every block. Backed by a
    /// flat index sorted by start: O(log n) plus a short walk to the innermost.
    [[nodiscard]] auto blockAt(int pos) const -> const parser::BlockNode*;

    /// Keyword spans to highlight when the caret at `pos` sits on a block's
    /// opener or closer keyword: the opener keyword plus its matching closer
    /// (`For`/`Next`, `Sub`/`End Sub`, ...). Empty when `pos` is not on one.
    [[nodiscard]] auto matchBlockAt(int pos) const -> const std::vector<std::pair<int, int>>&;
    /// Opener + closer keyword spans of the procedure (Sub / Function /
    /// Constructor / ...) enclosing `pos` — for `Return`. Empty when `pos`
    /// is not inside a procedure.
    [[nodiscard]] auto matchProcedureAt(int pos, const std::optional<std::pair<int, int>>& caretWord = std::nullopt) const
        -> const std::vector<std::pair<int, int>>&;

    /// Append global symbol candidates — free-standing `Sub`/`Function` names
    /// and every `Type`/`Union`/`Enum`/`#macro` name — to `out`.
    /// Position-independent; valid at any scope.
    void globalSymbolCompletions(std::vector<wxString>& out) const;

    /// Append module-level variable names (`Dim`/`Const`/`Var` at file scope)
    /// to `out`. Position-independent; globally visible.
    void moduleVariableCompletions(std::vector<wxString>& out) const;

    /// When `pos` lies inside a type method body (e.g. `Sub Vec.Foo`), append
    /// that type's member names (its `Sub`/`Function`/`Property` members and
    /// data fields, unqualified) — from this file and from its `#include`
    /// closure — to `out`. No-op outside a type method.
    void memberCompletionsAt(int pos, std::vector<wxString>& out) const;

    /// Append in-scope local names — parameters and `Dim`/`Const`/`Static`/`Var`
    /// declarations visible at `pos` (declared at or before it) — walking the
    /// scope chain outward. No-op outside any block.
    void localCompletionsAt(int pos, std::vector<wxString>& out) const;

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

    /// Byte ranges `[start, end)` of source in preprocessor branches that are
    /// definitely inactive under the current defines — for editor dimming. Empty
    /// when there is no compiler probe (then nothing can be known inactive).
    [[nodiscard]] auto getInactiveRanges() const -> const std::vector<std::pair<int, int>>& {
        return m_inactiveRanges;
    }
    /// Symbol tables of resolved `#include`d files, set by the intellisense
    /// service after include resolution. Empty until includes are wired.
    [[nodiscard]] auto getImported() const -> const std::vector<std::shared_ptr<const SymbolTable>>& {
        return m_imported;
    }
    /// Replace the imported (included-file) symbol tables. Called before publishing.
    void setImported(std::vector<std::shared_ptr<const SymbolTable>> imported) { m_imported = std::move(imported); }

    /// Lookup an `#include` directive by its source line (0-based).
    /// Returns `nullptr` when the line carries no recognised include.
    [[nodiscard]] auto findIncludeAt(int line) const -> const Include*;

    /// Stable hash over (kind, name) of visible symbol names and kinds.
    /// Hash does not include individual line numbers, etc.
    [[nodiscard]] auto getHash() const -> std::size_t { return m_hash; }

    /// Source file this table was parsed from (set by the intellisense service);
    /// empty for an unsaved document or a standalone parse.
    [[nodiscard]] auto getSourcePath() const -> const std::filesystem::path& { return m_sourcePath; }
    void setSourcePath(std::filesystem::path path) { m_sourcePath = std::move(path); }

    /// A symbol's source location: file (empty when its table has no path) and
    /// 0-based line.
    struct Location {
        std::filesystem::path path;
        int line = 0;
    };
    /// Find where `name` is defined — preferring a real definition over a forward
    /// `Declare` — across this table and its `#include` closure. Case-insensitive;
    /// accepts a method's unqualified name. nullopt when unknown.
    [[nodiscard]] auto findDefinition(const wxString& name) const -> std::optional<Location>;
    /// Like `findDefinition`, but preferring a forward `Declare`.
    [[nodiscard]] auto findDeclaration(const wxString& name) const -> std::optional<Location>;

private:
    /// Recursively walk a node list at any depth.
    void walkNodes(std::span<const parser::Node> nodes);
    /// Process a single block — emit its opener, then recurse into the body.
    void walkBlock(const parser::BlockNode& block);
    /// Append the enumerators in an enum body to `m_enumMembers` (recursing into
    /// the live `#if` branches). Called only for non-explicit enums, whose
    /// members import into the enclosing namespace.
    void gatherEnumMembers(std::span<const parser::Node> body);
    /// One full pre-order walk that records every block's extent into `m_scopes`
    /// (ascending start, for `blockAt`) and collects `#include` directives from
    /// the live `#if` branches only — `collectIncludes` is cleared inside dead
    /// branches, while scopes are still recorded so navigation works everywhere.
    void indexTree(std::span<const parser::Node> nodes, bool collectIncludes = true);
    /// If `tokens` is a recognised `#include`, push it onto `m_includes`.
    void tryAddInclude(const std::vector<lexer::Token>& tokens);
    /// Push one symbol drawn from an opener's tokens at `keywordIdx`.
    void emit(SymbolKind kind,
        const std::vector<lexer::Token>& opener,
        std::size_t keywordIdx,
        bool declaration = false);
    /// Append a synthetic `Type` (negative line) for every method owner that
    /// is not already a declared type, so members can be grouped under it.
    void synthesizeOwnerTypes();
    /// Recompute `m_hash` from the captured (kind, name) pairs.
    void computeHash();
    /// Append the member names (methods + data fields) of `owner` declared in
    /// this table to `out`. Shared by this file and its imported closure.
    void appendMembersOf(const wxString& owner, std::vector<wxString>& out) const;
    /// Shared implementation of findDefinition/findDeclaration.
    [[nodiscard]] auto findSymbol(const wxString& name, bool preferDeclaration) const -> std::optional<Location>;

    std::vector<Symbol> m_subs;                                       ///< `Sub` definitions.
    std::vector<Symbol> m_functions;                                  ///< `Function` definitions.
    std::vector<Symbol> m_constructors;                               ///< `Constructor` definitions.
    std::vector<Symbol> m_destructors;                                ///< `Destructor` definitions.
    std::vector<Symbol> m_operators;                                  ///< `Operator` definitions.
    std::vector<Symbol> m_properties;                                 ///< `Property` definitions.
    std::vector<Symbol> m_types;                                      ///< `Type` declarations.
    std::vector<Symbol> m_unions;                                     ///< `Union` declarations.
    std::vector<Symbol> m_enums;                                      ///< `Enum` declarations.
    std::vector<Symbol> m_enumMembers;                                ///< Enumerators of non-explicit enums (imported into scope).
    std::vector<Symbol> m_macros;                                     ///< `#macro` definitions.
    std::vector<Include> m_includes;                                  ///< `#include` directives.
    std::vector<std::shared_ptr<const SymbolTable>> m_imported;       ///< Resolved `#include` symbol tables.
    std::filesystem::path m_sourcePath;                               ///< File this table was parsed from (set by the service).
    std::shared_ptr<const std::unordered_set<std::string>> m_defines; ///< Shared define set (live `#if`/`#ifdef` selection); non-null after populate.
    std::vector<std::pair<int, int>> m_inactiveRanges;                ///< Byte [start,end) of inactive `#if` branches (dimming).
    std::size_t m_hash = 0;                                           ///< Stable hash over (kind, name) pairs.
    /// Retained lean scope tree (parents wired, tokens positioned). Shared so a
    /// publishable copy (own symbols + a fresh include closure) reuses it instead
    /// of re-parsing; `m_scopes`' BlockNode pointers stay valid across the share.
    std::shared_ptr<const parser::ProgramTree> m_tree; ///< Retained lean scope tree (parents wired, tokens positioned).
    /// One block's text extent, pointing into the retained tree.
    struct ScopeRange {
        int start;                      ///< First opener-token byte offset.
        int end;                        ///< One past the block's last token.
        const parser::BlockNode* block; ///< The block (into `m_tree`).
    };
    std::vector<ScopeRange> m_scopes; ///< Block extents, sorted by start, for `blockAt`.
    /// UDT (Type / Union) name -> its field names, for member completion in methods.
    std::unordered_map<wxString, std::vector<wxString>> m_typeFields;
    /// Reusable result buffer for `matchBlockAt` / `matchProcedureAt`, returned
    /// by const reference so no vector is allocated per caret move. The caller
    /// must consume it before the next match call. Mutable: the queries are
    /// logically const and run on the UI thread only.
    mutable std::vector<std::pair<int, int>> m_matchSpans;
};

} // namespace fbide
