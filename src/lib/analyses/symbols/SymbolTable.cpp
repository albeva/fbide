//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SymbolTable.hpp"
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
    constexpr std::size_t kMix = sizeof(std::size_t) >= 8
        ? static_cast<std::size_t>(0x9e3779b97f4a7c15ULL)
        : static_cast<std::size_t>(0x9e3779b9UL);
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
/// `KeywordKind::PpInclude` is set on the merged Preprocessor token at lex
/// time (FBSciLexer keeps the whole directive line — including the quoted
/// path — styled under PP states, and `StyleLexer::emitPreprocessor` folds
/// those runs into a single token). Identification is a single field check
/// and the path is the first quoted span inside the token text.
struct IncludeMatch {
    wxString path;
    int line = 0;
};

auto detectInclude(const std::vector<Token>& tokens) -> std::optional<IncludeMatch> {
    const Token* pp = nullptr;
    for (const auto& tkn : tokens) {
        if (tkn.kind == TokenKind::Whitespace || tkn.kind == TokenKind::Newline) {
            continue;
        }
        if (tkn.kind == TokenKind::Preprocessor && tkn.keywordKind == KeywordKind::PpInclude) {
            pp = &tkn;
        }
        break;
    }
    if (pp == nullptr) {
        return std::nullopt;
    }

    const std::string_view text { pp->text };
    const auto open = text.find_first_of("\"'");
    if (open == std::string_view::npos) {
        return std::nullopt;
    }

    const char quote = text[open];
    const auto close = text.find(quote, open + 1);
    if (close == std::string_view::npos) {
        return std::nullopt;
    }

    return IncludeMatch {
        .path = wxString::FromUTF8(text.substr(open + 1, close - open - 1)),
        .line = pp->line,
    };
}

/// First significant token's `KeywordKind` and its index inside `tokens`.
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

/// Pull the macro name out of a merged `#macro NAME[(args)]` Preprocessor
/// token. The lexer folds the entire directive line into one token, so the
/// name is extracted by skipping the directive keyword and the whitespace
/// that follows. Returns empty `wxString` when the line has no identifier.
auto detectMacroName(const Token& pp) -> wxString {
    const std::string_view text { pp.text };
    std::size_t i = 0;
    if (i < text.size() && text[i] == '#') {
        i++;
    }
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
        i++;
    }
    // Skip the directive word (`macro`).
    while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) {
        i++;
    }
    // Skip whitespace between the directive and the name.
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
        i++;
    }
    const std::size_t nameStart = i;
    while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) {
        i++;
    }
    if (i == nameStart) {
        return {};
    }
    return wxString::FromUTF8(text.substr(nameStart, i - nameStart));
}

} // namespace

SymbolTable::SymbolTable(const ProgramTree& tree) {
    populate(tree);
}

void SymbolTable::populate(const ProgramTree& tree) {
    walkNodes(tree.nodes);
    collectIncludes(tree.nodes);
    computeHash();
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
        // The merged Preprocessor token holds the whole `#macro NAME(...)`
        // line; pull the name out of its text directly.
        wxString name = detectMacroName(openerTokens[first.index]);
        if (!name.empty()) {
            m_macros.push_back(Symbol {
                .kind = SymbolKind::Macro,
                .name = std::move(name),
                .line = openerTokens[first.index].line,
            });
        }
        break;
    }
    case KeywordKind::Namespace:
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
    const Token* name = findWordlikeAfter(opener, keywordIdx + 1);
    if (name == nullptr) {
        return; // anonymous — skip
    }
    Symbol sym {
        .kind = kind,
        .name = wxString::FromUTF8(name->text),
        .line = opener.empty() ? 0 : opener.front().line,
    };
    switch (kind) {
    case SymbolKind::Sub:
        m_subs.push_back(std::move(sym));
        break;
    case SymbolKind::Function:
        m_functions.push_back(std::move(sym));
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
    hash = hashVector(hash, SymbolKind::Type, m_types);
    hash = hashVector(hash, SymbolKind::Union, m_unions);
    hash = hashVector(hash, SymbolKind::Enum, m_enums);
    hash = hashVector(hash, SymbolKind::Macro, m_macros);
    hash = hashIncludes(hash, m_includes);
    m_hash = hash;
}
