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

/// Boost-style 64-bit hash combiner.
auto hashCombine(std::size_t seed, std::size_t value) -> std::size_t {
    constexpr std::size_t kMix = 0x9e3779b97f4a7c15ULL;
    return seed ^ (value + kMix + (seed << 6) + (seed >> 2));
}

auto hashVector(std::size_t seed, const std::vector<Symbol>& vec) -> std::size_t {
    seed = hashCombine(seed, std::hash<std::size_t> {}(vec.size()));
    for (const auto& sym : vec) {
        seed = hashCombine(seed, std::hash<std::uint8_t> {}(static_cast<std::uint8_t>(sym.kind)));
        seed = hashCombine(seed, std::hash<std::string> {}(sym.name.utf8_string()));
        seed = hashCombine(seed, std::hash<int> {}(sym.line));
    }
    return seed;
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

} // namespace

SymbolTable::SymbolTable(const ProgramTree& tree) {
    walkNodes(tree.nodes);
    computeHash();
}

void SymbolTable::walkNodes(const std::vector<Node>& nodes) {
    for (const auto& node : nodes) {
        if (const auto* block = std::get_if<std::unique_ptr<BlockNode>>(&node)) {
            walkBlock(**block);
        }
        // BlankLineNode, StatementNode, VerbatimNode are not declaration sites.
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
    case KeywordKind::Namespace:
        // Recurse: namespaced declarations surface as flat entries for now.
        walkNodes(block.body);
        break;
    default:
        // Control-flow scopes (Do/For/While/If/Scope/Asm/Constructor/...)
        // don't contribute to the table. Body is also not walked: nested
        // declarations only matter inside Namespace.
        break;
    }
}

void SymbolTable::emit(SymbolKind kind,
    const std::vector<Token>& opener,
    std::size_t keywordIdx) {
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
    case SymbolKind::Sub:      m_subs.push_back(std::move(sym)); break;
    case SymbolKind::Function: m_functions.push_back(std::move(sym)); break;
    case SymbolKind::Type:     m_types.push_back(std::move(sym)); break;
    case SymbolKind::Union:    m_unions.push_back(std::move(sym)); break;
    case SymbolKind::Enum:     m_enums.push_back(std::move(sym)); break;
    }
}

void SymbolTable::computeHash() {
    std::size_t hash = 0;
    hash = hashVector(hash, m_subs);
    hash = hashVector(hash, m_functions);
    hash = hashVector(hash, m_types);
    hash = hashVector(hash, m_unions);
    hash = hashVector(hash, m_enums);
    m_hash = hash;
}
