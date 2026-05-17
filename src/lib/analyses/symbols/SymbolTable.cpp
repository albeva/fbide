//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SymbolTable.hpp"
#include <unordered_set>
#include "analyses/lexer/Token.hpp"
using namespace fbide;
using namespace fbide::reformat;
using namespace fbide::lexer;

namespace {

/// Boost-style hash combiner. The golden-ratio mixing constant is sized to
/// `std::size_t` so the function compiles on both 32-bit and 64-bit targets:
/// the 64-bit constant overflows a 32-bit `size_t`, so pick the matching
/// width at compile time.
auto hashCombine(const std::size_t seed, const std::size_t value) -> std::size_t {
    constexpr std::size_t kMix = sizeof(std::size_t) >= 8 ? 0x9e3779b97f4a7c15ULL : 0x9e3779b9UL;
    return seed ^ (value + kMix + (seed << 6) + (seed >> 2));
}

auto hashVector(std::size_t seed, const SymbolKind kind, const std::vector<Symbol>& vec) -> std::size_t {
    seed = hashCombine(seed, std::hash<std::size_t> {}(vec.size()));
    seed = hashCombine(seed, std::hash<SymbolKind> {}(kind));
    for (const auto& sym : vec) {
        seed = hashCombine(seed, std::hash<wxString> {}(sym.name));
    }
    return seed;
}

auto hashIncludes(std::size_t seed, const std::vector<Include>& vec) -> std::size_t {
    seed = hashCombine(seed, std::hash<std::size_t> {}(vec.size()));
    seed = hashCombine(seed, std::hash<SymbolKind> {}(SymbolKind::Include));
    for (const auto& inc : vec) {
        seed = hashCombine(seed, std::hash<wxString> {}(inc.path));
    }
    return seed;
}

/// Detect `#include [once] "path"` from a statement's tokens.
/// PP body tokens are emitted individually (FBSciLexer paints strings,
/// numbers, operators inside `#`-directive lines as their *PP variants;
/// `StyleLexer` routes them to standard `TokenKind`s with the source
/// `style` preserved). Detection is a structured walk:
///   PpInclude → optional `KeywordPP` "once" → String/UnterminatedString.
struct IncludeMatch {
    wxString path;
    int line = 0;
};

auto isLayoutToken(const Token& t) -> bool {
    return t.kind == TokenKind::Whitespace || t.kind == TokenKind::Newline;
}

auto detectInclude(const std::vector<Token>& tokens) -> std::optional<IncludeMatch> {
    std::size_t i = 0;
    while (i < tokens.size() && isLayoutToken(tokens[i])) {
        i++;
    }
    if (i == tokens.size()
        || tokens[i].kind != TokenKind::Preprocessor
        || tokens[i].keywordKind != KeywordKind::PpInclude) {
        return std::nullopt;
    }
    const int line = tokens[i].line;
    i++;

    while (i < tokens.size() && isLayoutToken(tokens[i])) {
        i++;
    }

    // Optional `once` modifier — only the directive identifier is KeywordPP;
    // `once` is a regular body identifier styled IdentifierPP.
    if (i < tokens.size()
        && tokens[i].kind == TokenKind::Identifier
        && tokens[i].style == ThemeCategory::IdentifierPP) {
        std::string lower = tokens[i].text;
        std::ranges::transform(lower, lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower == "once") {
            i++;
            while (i < tokens.size() && isLayoutToken(tokens[i])) {
                i++;
            }
        }
    }

    if (i == tokens.size()) {
        return std::nullopt;
    }
    if (tokens[i].kind != TokenKind::String
        && tokens[i].kind != TokenKind::UnterminatedString) {
        return std::nullopt;
    }

    const std::string_view text { tokens[i].text };
    if (text.size() < 2 || (text.front() != '"' && text.front() != '\'')) {
        return std::nullopt;
    }
    const std::size_t end = (text.back() == text.front() && text.size() > 1)
                              ? text.size() - 1
                              : text.size();
    return IncludeMatch {
        .path = wxString::FromUTF8(text.substr(1, end - 1)),
        .line = line,
    };
}

/// First structurally significant token's `KeywordKind` and its index inside
/// `tokens`. Leading access modifiers (`Private` / `Public` / `Protected`) are
/// skipped so `Private Sub Foo` dispatches on `Sub`, not the modifier.
struct FirstKeyword {
    KeywordKind kind = KeywordKind::None;
    std::size_t index = std::string::npos;
};
auto findFirstKeyword(const std::vector<Token>& tokens) -> FirstKeyword {
    for (std::size_t i = 0; i < tokens.size(); i++) {
        const auto& tkn = tokens[i];
        if (tkn.kind == TokenKind::Whitespace || tkn.kind == TokenKind::Newline) {
            continue;
        }
        if (isAccessModifier(tkn.keywordKind)) {
            continue;
        }
        if (tkn.keywordKind != KeywordKind::None && tkn.keywordKind != KeywordKind::Other) {
            return { .kind = tkn.keywordKind, .index = i };
        }
    }
    return {};
}

/// First word-like token (Identifier or any keyword group) at or after
/// `start`, or `nullptr` if none.
auto findWordlikeAfter(const std::vector<Token>& tokens, std::size_t start) -> const Token* {
    for (std::size_t i = start; i < tokens.size(); i++) {
        if (isWordLike(tokens[i].kind)) {
            return &tokens[i];
        }
    }
    return nullptr;
}

/// Index of the first non-layout token at or after `start`, or `npos`.
auto nextSignificant(const std::vector<Token>& tokens, std::size_t start) -> std::size_t {
    while (start < tokens.size() && isLayoutToken(tokens[start])) {
        start++;
    }
    return start < tokens.size() ? start : std::string::npos;
}

/// Build the declared name of a Sub / Function starting from `start`.
/// FreeBASIC methods carry a qualifier — `Sub TypeName.MethodName` — so a
/// trailing `.identifier` chain is folded into the name (`TypeName.MethodName`).
/// Returns an empty string when no name token is present (anonymous).
auto qualifiedNameAfter(const std::vector<Token>& tokens, std::size_t start) -> wxString {
    const Token* head = findWordlikeAfter(tokens, start);
    if (head == nullptr) {
        return {};
    }
    wxString name = wxString::FromUTF8(head->text);

    // Continue across `.identifier` segments — `Type.Method`, or a deeper
    // `Namespace.Type.Method` qualifier.
    std::size_t i = static_cast<std::size_t>(head - tokens.data()) + 1;
    while (true) {
        const std::size_t dot = nextSignificant(tokens, i);
        if (dot == std::string::npos || tokens[dot].operatorKind != OperatorKind::Dot) {
            break;
        }
        const std::size_t member = nextSignificant(tokens, dot + 1);
        if (member == std::string::npos || !isWordLike(tokens[member].kind)) {
            break;
        }
        name += '.';
        name += wxString::FromUTF8(tokens[member].text);
        i = member + 1;
    }
    return name;
}

/// Build the declared name of an `Operator` starting from `start`. An operator
/// name is not a plain identifier — it may be a symbol (`+`, `[]`, `@`), a
/// keyword (`Cast`, `New`, `Delete`, `Let`), and either free-standing or a UDT
/// member (`Operator TypeName.+`). Every significant token from `start` up to
/// the parameter list `(` is concatenated verbatim, e.g. `TypeName.New[]`.
auto operatorNameAfter(const std::vector<Token>& tokens, std::size_t start) -> wxString {
    wxString name;
    for (std::size_t i = start; i < tokens.size(); i++) {
        const auto& tkn = tokens[i];
        if (isLayoutToken(tkn)) {
            continue;
        }
        if (tkn.operatorKind == OperatorKind::ParenOpen) {
            break; // parameter list — name complete
        }
        name += wxString::FromUTF8(tkn.text);
    }
    return name;
}

} // namespace

SymbolTable::SymbolTable(const ProgramTree& tree) {
    populate(tree);
}

auto fbide::symbolOwner(const Symbol& sym) -> wxString {
    switch (sym.kind) {
    case SymbolKind::Constructor:
    case SymbolKind::Destructor:
        // The constructor/destructor name is the owning type itself.
        return sym.name;
    case SymbolKind::Sub:
    case SymbolKind::Function:
    case SymbolKind::Operator:
    case SymbolKind::Property:
        // `Owner.member` → text before the final dot, empty when free-standing.
        return sym.name.BeforeLast('.');
    case SymbolKind::Type:
    case SymbolKind::Union:
    case SymbolKind::Enum:
    case SymbolKind::Macro:
    case SymbolKind::Include:
        return {};
    }
    return {};
}

void SymbolTable::populate(const ProgramTree& tree) {
    walkNodes(tree.nodes);
    collectIncludes(tree.nodes);
    synthesizeOwnerTypes();
    computeHash();
}

void SymbolTable::synthesizeOwnerTypes() {
    std::unordered_set<wxString> known;
    known.reserve(m_types.size());
    for (const auto& type : m_types) {
        known.insert(type.name);
    }
    // Scan members in declaration order; the first reference to an undeclared
    // owner appends a synthetic, group-only Type entry.
    const auto absorb = [&](const std::vector<Symbol>& members) {
        for (const auto& sym : members) {
            const wxString owner = symbolOwner(sym);
            if (owner.empty()) {
                continue;
            }
            if (known.insert(owner).second) {
                m_types.push_back(Symbol {
                    .kind = SymbolKind::Type,
                    .name = owner,
                    .line = -1,
                });
            }
        }
    };
    absorb(m_subs);
    absorb(m_functions);
    absorb(m_constructors);
    absorb(m_destructors);
    absorb(m_operators);
    absorb(m_properties);
}

auto SymbolTable::findIncludeAt(const int line) const -> const Include* {
    for (const auto& inc : m_includes) {
        if (inc.line == line) {
            return &inc;
        }
    }
    return nullptr;
}

void SymbolTable::collectIncludes(const std::vector<Node>& nodes) {
    for (const auto& node : nodes) {
        if (const auto* stmt = std::get_if<StatementNode>(&node)) {
            tryAddInclude(stmt->tokens);
        } else if (const auto* block = std::get_if<std::unique_ptr<BlockNode>>(&node)) {
            if ((*block)->opener.has_value()) {
                tryAddInclude((*block)->opener->tokens);
            }
            collectIncludes((*block)->body);
        }
    }
}

void SymbolTable::tryAddInclude(const std::vector<Token>& tokens) {
    if (auto match = detectInclude(tokens)) {
        m_includes.push_back(Include {
            .path = std::move(match->path),
            .line = match->line,
        });
    }
}

void SymbolTable::reset() {
    m_subs.clear();
    m_functions.clear();
    m_constructors.clear();
    m_destructors.clear();
    m_operators.clear();
    m_properties.clear();
    m_types.clear();
    m_unions.clear();
    m_enums.clear();
    m_macros.clear();
    m_includes.clear();
}

void SymbolTable::walkNodes(const std::vector<Node>& nodes) {
    for (const auto& node : nodes) {
        if (const auto* block = std::get_if<std::unique_ptr<BlockNode>>(&node)) {
            walkBlock(**block);
        }
    }
}

void SymbolTable::walkBlock(const BlockNode& block) {
    if (!block.opener.has_value()) {
        return;
    }
    const auto& openerTokens = block.opener->tokens;
    const auto first = findFirstKeyword(openerTokens);

    switch (first.kind) {
    case KeywordKind::Sub:
        emit(SymbolKind::Sub, openerTokens, first.index);
        break;
    case KeywordKind::Function:
        emit(SymbolKind::Function, openerTokens, first.index);
        break;
    case KeywordKind::Constructor:
        emit(SymbolKind::Constructor, openerTokens, first.index);
        break;
    case KeywordKind::Destructor:
        emit(SymbolKind::Destructor, openerTokens, first.index);
        break;
    case KeywordKind::Operator:
        emit(SymbolKind::Operator, openerTokens, first.index);
        break;
    case KeywordKind::Property:
        emit(SymbolKind::Property, openerTokens, first.index);
        break;
    case KeywordKind::Type:
        emit(SymbolKind::Type, openerTokens, first.index);
        break;
    case KeywordKind::Union:
        emit(SymbolKind::Union, openerTokens, first.index);
        break;
    case KeywordKind::Enum:
        emit(SymbolKind::Enum, openerTokens, first.index);
        break;
    case KeywordKind::PpMacro: {
        // PP body tokens are emitted individually — the macro name is the
        // first word-like token after the `#macro` directive.
        if (const Token* name = findWordlikeAfter(openerTokens, first.index + 1)) {
            m_macros.push_back(Symbol {
                .kind = SymbolKind::Macro,
                .name = wxString::FromUTF8(name->text),
                .line = openerTokens[first.index].line,
            });
        }
        break;
    }
    case KeywordKind::Namespace:
        walkNodes(block.body);
        break;
    case KeywordKind::PpIf:
    case KeywordKind::PpIfDef:
    case KeywordKind::PpIfNDef:
    case KeywordKind::PpElse:
    case KeywordKind::PpElseIf:
    case KeywordKind::PpElseIfDef:
    case KeywordKind::PpElseIfNDef:
        // Conditional-compilation branches are transparent for symbol
        // collection — recurse so declarations guarded by `#if` / `#else` /
        // `#elseif` still show up (mirrors `collectIncludes`). Conditions are
        // not evaluated, so every branch contributes. `#macro` bodies and
        // other nested scopes are intentionally not recursed into.
        walkNodes(block.body);
        break;
    default:
        break;
    }
}

void SymbolTable::emit(
    const SymbolKind kind,
    const std::vector<Token>& opener,
    const std::size_t keywordIdx
) {
    // Operator names are not plain identifiers (`+`, `[]`, `Cast`, `Type.New`);
    // every token up to the parameter list forms the name. Everything else is
    // an identifier, optionally method-qualified (`Sub TypeName.MethodName`,
    // `Constructor TypeName`) — qualifiedNameAfter returns a bare name unchanged
    // when no dot follows.
    wxString name = kind == SymbolKind::Operator
                      ? operatorNameAfter(opener, keywordIdx + 1)
                      : qualifiedNameAfter(opener, keywordIdx + 1);
    if (name.empty()) {
        return; // anonymous — skip
    }
    Symbol sym {
        .kind = kind,
        .name = std::move(name),
        .line = opener.empty() ? 0 : opener.front().line,
    };
    switch (kind) {
    case SymbolKind::Sub:
        m_subs.push_back(std::move(sym));
        break;
    case SymbolKind::Function:
        m_functions.push_back(std::move(sym));
        break;
    case SymbolKind::Constructor:
        m_constructors.push_back(std::move(sym));
        break;
    case SymbolKind::Destructor:
        m_destructors.push_back(std::move(sym));
        break;
    case SymbolKind::Operator:
        m_operators.push_back(std::move(sym));
        break;
    case SymbolKind::Property:
        m_properties.push_back(std::move(sym));
        break;
    case SymbolKind::Type:
        m_types.push_back(std::move(sym));
        break;
    case SymbolKind::Union:
        m_unions.push_back(std::move(sym));
        break;
    case SymbolKind::Enum:
        m_enums.push_back(std::move(sym));
        break;
    case SymbolKind::Macro:
        m_macros.push_back(std::move(sym));
        break;
    case SymbolKind::Include:
        break; // includes go through tryAddInclude
    }
}

void SymbolTable::computeHash() {
    std::size_t hash = 0;
    hash = hashVector(hash, SymbolKind::Sub, m_subs);
    hash = hashVector(hash, SymbolKind::Function, m_functions);
    hash = hashVector(hash, SymbolKind::Constructor, m_constructors);
    hash = hashVector(hash, SymbolKind::Destructor, m_destructors);
    hash = hashVector(hash, SymbolKind::Operator, m_operators);
    hash = hashVector(hash, SymbolKind::Property, m_properties);
    hash = hashVector(hash, SymbolKind::Type, m_types);
    hash = hashVector(hash, SymbolKind::Union, m_unions);
    hash = hashVector(hash, SymbolKind::Enum, m_enums);
    hash = hashVector(hash, SymbolKind::Macro, m_macros);
    hash = hashIncludes(hash, m_includes);
    m_hash = hash;
}
