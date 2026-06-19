//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SymbolTable.hpp"
#include <unordered_set>
#include "PpConditional.hpp"
#include "analyses/lexer/Token.hpp"
using namespace fbide;
using namespace fbide::parser;
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

// Case-insensitive ASCII compare of a token's text against a lowercase literal.
auto textEqualsLower(const Token& tok, const std::string_view lower) -> bool {
    if (tok.text.size() != lower.size()) {
        return false;
    }
    for (std::size_t k = 0; k < lower.size(); k++) {
        char ch = tok.text[k];
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch + ('a' - 'A'));
        }
        if (ch != lower[k]) {
            return false;
        }
    }
    return true;
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
        && tokens[i].style == ThemeCategory::IdentifierPP
        && textEqualsLower(tokens[i], "once")) {
        i++;
        while (i < tokens.size() && isLayoutToken(tokens[i])) {
            i++;
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

auto isPpIfKind(const KeywordKind kind) -> bool {
    return kind == KeywordKind::PpIf || kind == KeywordKind::PpIfDef || kind == KeywordKind::PpIfNDef;
}

auto isPpElseKind(const KeywordKind kind) -> bool {
    return kind == KeywordKind::PpElse || kind == KeywordKind::PpElseIf
        || kind == KeywordKind::PpElseIfDef || kind == KeywordKind::PpElseIfNDef;
}

/// Decompose a `#if` chain into its branches, marking which are live under
/// `defines`. Branch one spans the body up to the first `#elseif`/`#else` child;
/// later branches are those child blocks. A branch is live unless a prior branch
/// is certainly true or its own condition is certainly false — an Unknown
/// condition keeps the branch and does not suppress the later ones.
auto ppBranches(const BlockNode& ifBlock, const std::unordered_set<std::string>& defines) -> std::vector<PpBranch> {
    std::vector<PpBranch> branches;
    const auto& body = ifBlock.body;

    std::size_t firstElse = body.size();
    for (std::size_t i = 0; i < body.size(); i++) {
        const auto* slot = std::get_if<std::unique_ptr<BlockNode>>(&body[i]);
        if (slot != nullptr && *slot != nullptr && (*slot)->opener.has_value()
            && isPpElseKind(findFirstKeyword((*slot)->opener->tokens).kind)) {
            firstElse = i;
            break;
        }
    }

    branches.reserve(body.size() - firstElse + 1); // leading branch + each #elseif/#else child
    bool certainTrue = false;
    const auto consider = [&](const PpEval eval, const std::span<const Node> content, const BlockNode* blk) {
        const bool live = !certainTrue && eval != PpEval::False;
        branches.push_back({ .body = content, .block = blk, .live = live });
        if (live && eval == PpEval::True) {
            certainTrue = true;
        }
    };

    consider(ifBlock.opener.has_value() ? evaluatePpCondition(ifBlock.opener->tokens, defines) : PpEval::Unknown,
        std::span<const Node>(body.data(), firstElse), nullptr);

    for (std::size_t i = firstElse; i < body.size(); i++) {
        const auto* slot = std::get_if<std::unique_ptr<BlockNode>>(&body[i]);
        if (slot == nullptr || *slot == nullptr || !(*slot)->opener.has_value()) {
            continue;
        }
        const auto& child = **slot;
        const auto kind = findFirstKeyword(child.opener->tokens).kind;
        if (!isPpElseKind(kind)) {
            continue;
        }
        const PpEval eval = (kind == KeywordKind::PpElse)
                              ? PpEval::True
                              : evaluatePpCondition(child.opener->tokens, defines);
        consider(eval, std::span<const Node>(child.body), &child);
    }
    return branches;
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

SymbolTable::SymbolTable(ProgramTree&& tree) {
    populate(std::move(tree));
}

auto fbide::symbolOwner(const Symbol& sym) -> const wxString& {
    // Computed once at emit time (see SymbolTable::emit); this is a cheap
    // accessor so completion / browser / lookup filters don't reallocate a
    // BeforeLast substring per symbol per query.
    return sym.owner;
}

namespace {
auto tokensSpan(const std::vector<lexer::Token>& toks) -> std::optional<std::pair<int, int>> {
    if (toks.empty()) {
        return std::nullopt;
    }
    const auto& last = toks.back();
    return std::pair { toks.front().pos, last.pos + static_cast<int>(last.text.size()) };
}

auto nodeSpan(const Node& node) -> std::optional<std::pair<int, int>>;

// Full text extent of a block: union of its opener, body and closer spans.
auto blockSpan(const BlockNode& block) -> std::pair<int, int> {
    int start = std::numeric_limits<int>::max();
    int end = std::numeric_limits<int>::min();
    const auto consider = [&](const std::optional<std::pair<int, int>>& span) {
        if (span) {
            start = std::min(start, span->first);
            end = std::max(end, span->second);
        }
    };
    if (block.opener) {
        consider(tokensSpan(block.opener->tokens));
    }
    for (const auto& child : block.body) {
        consider(nodeSpan(child));
    }
    if (block.closer) {
        consider(tokensSpan(block.closer->tokens));
    }
    if (start > end) {
        return { 0, 0 }; // no positioned tokens (defensive)
    }
    return { start, end };
}

auto nodeSpan(const Node& node) -> std::optional<std::pair<int, int>> {
    if (const auto* stmt = std::get_if<StatementNode>(&node)) {
        return tokensSpan(stmt->tokens);
    }
    if (const auto* verb = std::get_if<VerbatimNode>(&node)) {
        return tokensSpan(verb->tokens);
    }
    if (const auto* block = std::get_if<std::unique_ptr<BlockNode>>(&node); block != nullptr && *block) {
        return blockSpan(**block);
    }
    return std::nullopt; // BlankLineNode carries no position
}
/// Text extent of an inactive `#if` branch for dimming: an `#elseif`/`#else`
/// child spans its own block; the leading `#if` branch spans its content nodes.
auto branchExtent(const PpBranch& branch) -> std::optional<std::pair<int, int>> {
    if (branch.block != nullptr) {
        return blockSpan(*branch.block);
    }
    int start = std::numeric_limits<int>::max();
    int end = std::numeric_limits<int>::min();
    for (const auto& node : branch.body) {
        if (const auto span = nodeSpan(node)) {
            start = std::min(start, span->first);
            end = std::max(end, span->second);
        }
    }
    if (start > end) {
        return std::nullopt; // empty branch (e.g. `#if X` immediately followed by `#else`)
    }
    return std::pair { start, end };
}

} // namespace

auto SymbolTable::ppBranchesCached(const BlockNode& ifBlock) -> const std::vector<PpBranch>& {
    auto it = m_ppCache.find(&ifBlock);
    if (it == m_ppCache.end()) {
        it = m_ppCache.emplace(&ifBlock, ppBranches(ifBlock, *m_defines)).first;
    }
    return it->second;
}

void SymbolTable::populate(ProgramTree&& tree, std::shared_ptr<const std::unordered_set<std::string>> defines) {
    // Shared, not copied — every table in a parse drain points at the one set. A
    // null argument (standalone parse / no defines) falls back to a shared empty
    // so the walks below can dereference `m_defines` unconditionally.
    static const auto kEmptyDefines = std::make_shared<const std::unordered_set<std::string>>();
    m_defines = defines ? std::move(defines) : kEmptyDefines;
    walkNodes(tree.nodes);
    indexTree(tree.nodes); // #includes + scope ranges in one pass (before the move;
                           // BlockNodes are heap-stable so the pointers survive it)
    synthesizeOwnerTypes();
    computeHash();
    // Shared so a published copy (own symbols + a fresh include closure) can
    // reuse it instead of re-parsing. The BlockNodes are heap-stable, so the
    // `m_scopes` pointers stay valid across the move into the shared tree.
    m_tree = std::make_shared<const ProgramTree>(std::move(tree));
    m_ppCache.clear(); // populate-transient: its spans point into the tree above
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

void SymbolTable::indexTree(std::span<const Node> nodes, bool collectIncludes) {
    for (const auto& node : nodes) {
        if (const auto* stmt = std::get_if<StatementNode>(&node)) {
            if (collectIncludes) {
                tryAddInclude(stmt->tokens);
            }
        } else if (const auto* slot = std::get_if<std::unique_ptr<BlockNode>>(&node); slot != nullptr && *slot) {
            const auto& block = **slot;
            if (collectIncludes && block.opener.has_value()) {
                tryAddInclude(block.opener->tokens);
            }
            const auto [start, end] = blockSpan(block);
            m_scopes.push_back({ start, end, slot->get() }); // scope recorded for every branch

            if (block.opener.has_value() && isPpIfKind(findFirstKeyword(block.opener->tokens).kind)) {
                // A `#if` chain: record each branch's scope (so navigation works
                // everywhere) but only gather `#include`s from the live branches.
                for (const auto& branch : ppBranchesCached(block)) {
                    if (branch.block != nullptr) {
                        const auto [bstart, bend] = blockSpan(*branch.block);
                        m_scopes.push_back({ bstart, bend, branch.block });
                    }
                    // Record a definitively inactive branch for editor dimming,
                    // but only at the live/dead boundary (`collectIncludes` still
                    // set) — a dead branch dims as a single region.
                    if (!branch.live && collectIncludes) {
                        if (const auto extent = branchExtent(branch)) {
                            m_inactiveRanges.push_back(*extent);
                        }
                    }
                    indexTree(branch.body, collectIncludes && branch.live);
                }
            } else {
                indexTree(block.body, collectIncludes); // pre-order keeps m_scopes sorted by start
            }
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

auto SymbolTable::blockAt(const int pos) const -> const BlockNode* {
    // Sorted by start; the innermost container has the largest start <= pos, so
    // scan back from the first entry whose start is past pos.
    auto it = std::upper_bound(m_scopes.begin(), m_scopes.end(), pos,
        [](const int position, const ScopeRange& range) { return position < range.start; });
    while (it != m_scopes.begin()) {
        --it;
        if (pos < it->end) {
            return it->block;
        }
    }
    return nullptr;
}

namespace {
auto tokenRange(const Token& tok) -> std::pair<int, int> {
    return { tok.pos, tok.pos + static_cast<int>(tok.text.size()) };
}

// First structural keyword of an opener, skipping access modifiers
// (`Private`/`Public`/`Protected`). The token whose keyword decides the block.
auto openerKeywordToken(const BlockNode& block) -> const Token* {
    if (!block.opener) {
        return nullptr;
    }
    for (const auto& tok : block.opener->tokens) {
        if (tok.keywordKind == KeywordKind::None || isAccessModifier(tok.keywordKind)) {
            continue;
        }
        return &tok;
    }
    return nullptr;
}

// Closer keyword span: `End X` covers both tokens; single-word closers
// (`Next`/`Loop`/`Wend`) cover just the keyword, not any trailing variable.
auto closerKeywordSpan(const BlockNode& block) -> std::optional<std::pair<int, int>> {
    if (!block.closer || block.closer->tokens.empty()) {
        return std::nullopt;
    }
    const auto& toks = block.closer->tokens;
    if (toks.front().keywordKind == KeywordKind::End && toks.size() >= 2) {
        return std::pair { toks.front().pos, tokenRange(toks[1]).second };
    }
    return tokenRange(toks.front());
}

auto isProcedureKind(const KeywordKind kind) -> bool {
    switch (kind) {
    case KeywordKind::Sub:
    case KeywordKind::Function:
    case KeywordKind::Constructor:
    case KeywordKind::Destructor:
    case KeywordKind::Operator:
    case KeywordKind::Property:
        return true;
    default:
        return false;
    }
}

auto procSymbolKind(const KeywordKind kind) -> std::optional<SymbolKind> {
    switch (kind) {
    case KeywordKind::Sub:
        return SymbolKind::Sub;
    case KeywordKind::Function:
        return SymbolKind::Function;
    case KeywordKind::Constructor:
        return SymbolKind::Constructor;
    case KeywordKind::Destructor:
        return SymbolKind::Destructor;
    case KeywordKind::Operator:
        return SymbolKind::Operator;
    case KeywordKind::Property:
        return SymbolKind::Property;
    default:
        return std::nullopt;
    }
}

auto spanContains(const std::pair<int, int>& span, const int pos) -> bool {
    return pos >= span.first && pos < span.second;
}

// Each header / branch contributes a primary keyword span (the one that
// triggers the group) plus an optional secondary (the trailing `Then` of an
// If / ElseIf). Returned on the stack — no per-call heap allocation.
struct KeywordSpans {
    std::pair<int, int> primary;                  ///< Keyword that triggers the group / branch.
    std::optional<std::pair<int, int>> secondary; ///< Trailing `Then`, when present.
};

auto selectOpener(const BlockNode& block) -> std::optional<KeywordSpans> {
    if (!block.opener || block.opener->tokens.empty()) {
        return std::nullopt;
    }
    const auto& toks = block.opener->tokens;
    if (toks.size() >= 2 && toks[1].keywordKind == KeywordKind::Case) {
        return KeywordSpans { { toks.front().pos, tokenRange(toks[1]).second }, std::nullopt };
    }
    return KeywordSpans { tokenRange(toks.front()), std::nullopt };
}

// A single-token keyword span (no secondary): `Case`, and the PP directives
// `#if` / `#else` / `#endif` (each lexed as one token).
auto singleKeyword(const BlockNode& block) -> std::optional<KeywordSpans> {
    const auto* kw = openerKeywordToken(block);
    if (kw == nullptr) {
        return std::nullopt;
    }
    return KeywordSpans { tokenRange(*kw), std::nullopt };
}

auto isCaseKind(const KeywordKind kind) -> bool {
    return kind == KeywordKind::Case;
}

auto isElseKind(const KeywordKind kind) -> bool {
    return kind == KeywordKind::ElseIf || kind == KeywordKind::Else;
}

auto isSelectKind(const KeywordKind kind) -> bool {
    return kind == KeywordKind::Select;
}

auto isIfKind(const KeywordKind kind) -> bool {
    return kind == KeywordKind::If;
}

// Block kinds an `Exit` / `Continue` argument can name.
auto isExitArgKind(const KeywordKind kind) -> bool {
    switch (kind) {
    case KeywordKind::For:
    case KeywordKind::Do:
    case KeywordKind::While:
    case KeywordKind::Select:
    case KeywordKind::Sub:
    case KeywordKind::Function:
    case KeywordKind::Property:
    case KeywordKind::Operator:
    case KeywordKind::Constructor:
    case KeywordKind::Destructor:
        return true;
    default:
        return false;
    }
}

// The opener's trailing `Then` (multi-line If / ElseIf), if present.
auto trailingThenSpan(const BlockNode& block) -> std::optional<std::pair<int, int>> {
    if (!block.opener || block.opener->tokens.empty()) {
        return std::nullopt;
    }
    const auto& last = block.opener->tokens.back();
    return last.keywordKind == KeywordKind::Then ? std::optional { tokenRange(last) } : std::nullopt;
}

auto ifOpener(const BlockNode& block) -> std::optional<KeywordSpans> {
    const auto* kw = openerKeywordToken(block);
    if (kw == nullptr) {
        return std::nullopt;
    }
    return KeywordSpans { tokenRange(*kw), trailingThenSpan(block) };
}

auto ifBranch(const BlockNode& branch) -> std::optional<KeywordSpans> {
    if (!branch.opener || branch.opener->tokens.empty()) {
        return std::nullopt;
    }
    const auto& toks = branch.opener->tokens;
    const auto primary = (toks.front().keywordKind == KeywordKind::Else && toks.size() >= 2 && toks[1].keywordKind == KeywordKind::If)
                           ? std::pair { toks.front().pos, tokenRange(toks[1]).second } // Else If
                           : tokenRange(toks.front());                                  // ElseIf / Else
    return KeywordSpans { primary, trailingThenSpan(branch) };
}

auto openerKindOf(const BlockNode& block) -> KeywordKind {
    const auto* kw = openerKeywordToken(block);
    return kw != nullptr ? kw->keywordKind : KeywordKind::None;
}

// A container block (Select / If) with branch children (Case / ElseIf, Else)
// highlights as a group. On a single branch's primary keyword: the header, that
// one branch, and the closer. On the header's primary keyword or the closer: the
// header, every branch, and the closer. Each header / branch contributes its
// `Then` too. Requiring a closer skips single-line / unclosed `If`s. Appends to
// `out`; returns whether it matched.
auto matchBranchGroup(
    std::vector<std::pair<int, int>>& out,
    const BlockNode* block,
    const int pos,
    bool (*isContainerKind)(KeywordKind),
    bool (*isBranchKind)(KeywordKind),
    std::optional<KeywordSpans> (*openerOf)(const BlockNode&),
    std::optional<KeywordSpans> (*branchOf)(const BlockNode&)
) -> bool {
    if (block == nullptr) {
        return false;
    }
    const auto isContainer = [&](const BlockNode& node) {
        return isContainerKind(openerKindOf(node)) && node.closer.has_value();
    };
    const auto append = [&](const KeywordSpans& spans) {
        out.push_back(spans.primary);
        if (spans.secondary) {
            out.push_back(*spans.secondary);
        }
    };
    // Caret on a branch's primary keyword -> that branch plus the scope pair.
    // (Its `Then`, if any, routes to the Then-scope path before this runs.)
    if (isBranchKind(openerKindOf(*block))) {
        const auto branch = branchOf(*block);
        const auto* head = block->parent;
        if (branch && spanContains(branch->primary, pos) && head != nullptr && isContainer(*head)) {
            if (const auto header = openerOf(*head)) {
                append(*header);
            }
            append(*branch);
            if (const auto closer = closerKeywordSpan(*head)) {
                out.push_back(*closer);
            }
            return true;
        }
        return false;
    }
    // Caret on the header's primary keyword or the closer -> the whole group.
    if (isContainer(*block)) {
        const auto header = openerOf(*block);
        const auto closer = closerKeywordSpan(*block);
        if ((header && spanContains(header->primary, pos)) || (closer && spanContains(*closer, pos))) {
            if (header) {
                append(*header);
            }
            for (const auto& child : block->body) {
                const auto* branch = std::get_if<std::unique_ptr<BlockNode>>(&child);
                if (branch != nullptr && *branch && isBranchKind(openerKindOf(**branch))) {
                    if (const auto spans = branchOf(**branch)) {
                        append(*spans);
                    }
                }
            }
            if (closer) {
                out.push_back(*closer);
            }
            return true;
        }
    }
    return false;
}

// Caret on a `Then` appends only the enclosing If scope (If + that Then +
// End If) to `out`, not the Else/ElseIf branches. Returns whether it matched.
auto matchThenScope(std::vector<std::pair<int, int>>& out, const BlockNode* block, const int pos) -> bool {
    if (block == nullptr) {
        return false;
    }
    const auto then = trailingThenSpan(*block);
    if (!then || !spanContains(*then, pos)) {
        return false;
    }
    const BlockNode* ifBlock = nullptr;
    if (openerKindOf(*block) == KeywordKind::If && block->closer.has_value()) {
        ifBlock = block; // the If's own Then
    } else if (isElseKind(openerKindOf(*block)) && block->parent != nullptr
               && openerKindOf(*block->parent) == KeywordKind::If && block->parent->closer.has_value()) {
        ifBlock = block->parent; // an ElseIf's Then
    }
    if (ifBlock == nullptr) {
        return false;
    }
    if (const auto* kw = openerKeywordToken(*ifBlock)) {
        out.push_back(tokenRange(*kw)); // If
    }
    out.push_back(*then); // Then
    if (const auto closer = closerKeywordSpan(*ifBlock)) {
        out.push_back(*closer); // End If
    }
    return true;
}

// Caret on an `Exit` / `Continue` statement appends the keyword (or arg) under
// the caret plus the opener/closer of the scope it resolves to. Each comma arg
// steps one block further out among matches of its kind, so `Continue For, For`
// resolves the 2nd `For` two levels up. The `Exit`/`Continue` keyword targets
// the outermost (last) arg; an intermediary arg targets its own scope.
auto matchExitContinue(std::vector<std::pair<int, int>>& out, const BlockNode* block, const int pos) -> bool {
    if (block == nullptr) {
        return false;
    }
    // The statement under the caret is a direct child of the innermost block.
    const StatementNode* stmt = nullptr;
    for (const auto& child : block->body) {
        const auto* candidate = std::get_if<StatementNode>(&child);
        if (candidate == nullptr) {
            continue;
        }
        if (const auto span = tokensSpan(candidate->tokens); span && spanContains(*span, pos)) {
            stmt = candidate;
            break;
        }
    }
    if (stmt == nullptr) {
        return false;
    }
    const auto& toks = stmt->tokens;
    // `Exit` / `Continue` may sit mid-statement (single-line `If ... Then Exit
    // Sub`), so scan for the one the caret is on (its keyword or one of its args).
    for (std::size_t i = 0; i < toks.size(); ++i) {
        if (toks[i].keywordKind != KeywordKind::Exit && toks[i].keywordKind != KeywordKind::Continue) {
            continue;
        }
        // Args follow immediately: kind keywords, comma-separated, ending at the
        // first other token (e.g. `Else` / `:` of a single-line If).
        std::vector<const Token*> args;
        for (std::size_t j = i + 1; j < toks.size(); ++j) {
            if (isExitArgKind(toks[j].keywordKind)) {
                args.push_back(&toks[j]);
            } else if (toks[j].text == ",") {
                continue;
            } else {
                break;
            }
        }
        if (args.empty()) {
            continue;
        }
        // Is the caret on this Exit/Continue keyword or one of its args?
        const bool onKeyword = spanContains(tokenRange(toks[i]), pos);
        int argIndex = -1;
        for (std::size_t k = 0; k < args.size(); ++k) {
            if (spanContains(tokenRange(*args[k]), pos)) {
                argIndex = static_cast<int>(k);
                break;
            }
        }
        if (!onKeyword && argIndex < 0) {
            continue; // the caret is on a different Exit/Continue, or neither
        }
        // Resolve each arg to a block, stepping one level further out each time.
        std::vector<const BlockNode*> targets;
        const BlockNode* searchFrom = block;
        for (const auto* arg : args) {
            const BlockNode* target = nullptr;
            for (const BlockNode* node = searchFrom; node != nullptr; node = node->parent) {
                if (openerKindOf(*node) == arg->keywordKind) {
                    target = node;
                    break;
                }
            }
            if (target == nullptr) {
                break; // not enough enclosing scopes
            }
            targets.push_back(target);
            searchFrom = target->parent;
        }
        // Keyword -> outermost (last) arg; an arg -> its own resolved scope.
        const BlockNode* target = nullptr;
        std::optional<std::pair<int, int>> clicked;
        if (onKeyword) {
            if (!targets.empty()) {
                target = targets.back();
                clicked = tokenRange(toks[i]);
            }
        } else if (static_cast<std::size_t>(argIndex) < targets.size()) {
            target = targets[static_cast<std::size_t>(argIndex)];
            clicked = tokenRange(*args[static_cast<std::size_t>(argIndex)]);
        }
        if (target == nullptr || !clicked) {
            return false;
        }
        out.push_back(*clicked);
        if (const auto* kw = openerKeywordToken(*target)) {
            out.push_back(tokenRange(*kw));
        }
        if (const auto closer = closerKeywordSpan(*target)) {
            out.push_back(*closer);
        }
        return true;
    }
    return false;
}
} // namespace

auto SymbolTable::matchBlockAt(const int pos) const -> const std::vector<std::pair<int, int>>& {
    m_matchSpans.clear();
    const auto* block = blockAt(pos);
    if (block == nullptr) {
        return m_matchSpans;
    }
    // Caret on a `Then` -> just the enclosing If scope, not the else branches.
    if (matchThenScope(m_matchSpans, block, pos)) {
        return m_matchSpans;
    }
    // Container/branch keywords highlight as a group (Select/Case, If/ElseIf/Else).
    if (matchBranchGroup(m_matchSpans, block, pos, isSelectKind, isCaseKind, selectOpener, singleKeyword)) {
        return m_matchSpans;
    }
    if (matchBranchGroup(m_matchSpans, block, pos, isIfKind, isElseKind, ifOpener, ifBranch)) {
        return m_matchSpans;
    }
    // Preprocessor conditional: #if / #ifdef / #ifndef ... #elseif / #else ... #endif.
    if (matchBranchGroup(m_matchSpans, block, pos, isPpIfKind, isPpElseKind, singleKeyword, singleKeyword)) {
        return m_matchSpans;
    }
    // Exit / Continue statements resolve to the loop / scope(s) they act on.
    if (matchExitContinue(m_matchSpans, block, pos)) {
        return m_matchSpans;
    }
    // Plain opener/closer pair (For/Next, Sub/End Sub, ...). Both must exist, so
    // branches and single-line / unclosed blocks are skipped.
    const auto* kw = openerKeywordToken(*block);
    const auto closer = closerKeywordSpan(*block);
    if (kw != nullptr && closer) {
        const auto opener = tokenRange(*kw);
        if (spanContains(opener, pos) || spanContains(*closer, pos)) {
            m_matchSpans.push_back(opener);
            m_matchSpans.push_back(*closer);
        }
    }
    return m_matchSpans;
}

auto SymbolTable::matchProcedureAt(const int pos, const std::optional<std::pair<int, int>>& caretWord) const
    -> const std::vector<std::pair<int, int>>& {
    m_matchSpans.clear();
    for (const auto* block = blockAt(pos); block != nullptr; block = block->parent) {
        const auto* kw = openerKeywordToken(*block);
        if (kw == nullptr || !isProcedureKind(kw->keywordKind)) {
            continue;
        }
        m_matchSpans.push_back(tokenRange(*kw));
        if (const auto closer = closerKeywordSpan(*block)) {
            m_matchSpans.push_back(*closer);
        }
        if (caretWord) {
            m_matchSpans.push_back(*caretWord); // the Return keyword itself
        }
        break;
    }
    return m_matchSpans;
}

namespace {

// Modifier keywords that may precede the name(s) in a declaration.
auto isDeclModifier(const Token& tok) -> bool {
    static constexpr std::string_view mods[]
        = { "dim", "redim", "const", "static", "var", "common", "extern", "shared", "threadlocal", "preserve" };
    return std::ranges::any_of(mods, [&](const auto sv) { return textEqualsLower(tok, sv); });
}

// Keywords that, as the first word of a statement, start a variable declaration.
auto isDeclLeader(const Token& tok) -> bool {
    static constexpr std::string_view leaders[] = { "dim", "redim", "const", "static", "var", "common", "extern" };
    return std::ranges::any_of(leaders, [&](const auto sv) { return textEqualsLower(tok, sv); });
}

/// True when an enum opener carries the `Explicit` modifier (`Enum Foo Explicit`),
/// whose members are scoped rather than imported into the enclosing namespace.
/// `Explicit` occupies the slot *after* the enum name, so an enum merely named
/// `Explicit` is not mistaken for one.
auto enumIsExplicit(const std::vector<Token>& opener) -> bool {
    const auto enumKw = findFirstKeyword(opener);
    if (enumKw.index == std::string::npos) {
        return false;
    }
    bool seenName = false;
    for (std::size_t idx = enumKw.index + 1; idx < opener.size(); idx++) {
        if (!isWordLike(opener[idx].kind)) {
            continue;
        }
        if (!seenName) {
            seenName = true; // the enum name
            continue;
        }
        return textEqualsLower(opener[idx], "explicit"); // the modifier slot
    }
    return false;
}

// ByRef / ByVal parameter qualifiers — they precede the parameter name.
auto isParamQualifier(const Token& tok) -> bool {
    return textEqualsLower(tok, "byref") || textEqualsLower(tok, "byval");
}

// First non-layout token of a statement, or nullptr.
auto firstSignificant(const std::vector<Token>& toks) -> const Token* {
    for (const auto& tok : toks) {
        if (tok.kind != TokenKind::Whitespace && tok.kind != TokenKind::Newline
            && tok.kind != TokenKind::Comment && tok.kind != TokenKind::CommentBlock) {
            return &tok;
        }
    }
    return nullptr;
}

auto isOpenBracket(const OperatorKind op) -> bool {
    return op == OperatorKind::ParenOpen || op == OperatorKind::BracketOpen || op == OperatorKind::BraceOpen;
}
auto isCloseBracket(const OperatorKind op) -> bool {
    return op == OperatorKind::ParenClose || op == OperatorKind::BracketClose || op == OperatorKind::BraceClose;
}

// Collect declared names from a declaration / UDT-field statement: skip leading
// access + declaration modifier keywords, then take the name-first identifiers
// (comma-separated, at bracket depth 0), each ending at `As` / `=` / `:`.
// `As`-first forms (`Dim As Integer x`) are not captured.
void collectDeclNames(const std::vector<Token>& toks, std::vector<wxString>& out) {
    std::size_t idx = 0;
    const auto skipLayout = [&] {
        while (idx < toks.size()
               && (toks[idx].kind == TokenKind::Whitespace || toks[idx].kind == TokenKind::Newline
                   || toks[idx].kind == TokenKind::Comment || toks[idx].kind == TokenKind::CommentBlock)) {
            idx++;
        }
    };
    while (true) {
        skipLayout();
        if (idx >= toks.size()) {
            return;
        }
        if (isAccessModifier(toks[idx].keywordKind) || isDeclModifier(toks[idx])) {
            idx++;
            continue;
        }
        break;
    }
    // FB allows a shared leading type: `Const As <type> name1 = v1, name2 = v2`.
    // There the name follows the type, so advance past `As <type>` to the first
    // declarator (an identifier before `=` / `,` / end) before scanning names.
    if (idx < toks.size() && toks[idx].keywordKind == KeywordKind::As) {
        for (idx++; idx < toks.size(); idx++) {
            if (isLayoutToken(toks[idx])) {
                continue;
            }
            if (!isWordLike(toks[idx].kind)) {
                continue;
            }
            const auto nxt = nextSignificant(toks, idx + 1);
            if (nxt == std::string::npos || toks[nxt].operatorKind == OperatorKind::Assign
                || toks[nxt].operatorKind == OperatorKind::Comma) {
                break; // idx now at the first name; the scan below captures it
            }
        }
    }
    bool expectName = true;
    int depth = 0;
    for (; idx < toks.size(); idx++) {
        const auto& tok = toks[idx];
        if (tok.kind == TokenKind::Whitespace || tok.kind == TokenKind::Newline
            || tok.kind == TokenKind::Comment || tok.kind == TokenKind::CommentBlock) {
            continue;
        }
        const auto op = tok.operatorKind;
        if (isOpenBracket(op)) {
            depth++;
            continue;
        }
        if (isCloseBracket(op)) {
            if (depth > 0) {
                depth--;
            }
            continue;
        }
        if (depth > 0) {
            continue;
        }
        if (op == OperatorKind::Comma) {
            expectName = true;
            continue;
        }
        if (tok.keywordKind == KeywordKind::As || op == OperatorKind::Assign || op == OperatorKind::Colon
            || op == OperatorKind::Semicolon) {
            expectName = false;
            continue;
        }
        if (expectName && isWordLike(tok.kind)) {
            out.push_back(wxString::FromUTF8(tok.text));
        }
        expectName = false;
    }
}

/// Append module-level `Dim`/`Const`/`Var`/... names from `nodes`, descending
/// into transparent scopes (namespaces, `#if` branches) so namespaced constants
/// are seen too.
void collectModuleDeclNames(std::span<const parser::Node> nodes, std::vector<wxString>& out,
    const std::unordered_set<std::string>& defines) {
    for (const auto& node : nodes) {
        if (const auto* stmt = std::get_if<parser::StatementNode>(&node)) {
            const Token* lead = firstSignificant(stmt->tokens);
            if (lead != nullptr && isDeclLeader(*lead)) {
                collectDeclNames(stmt->tokens, out);
            }
        } else if (const auto* block = std::get_if<std::unique_ptr<parser::BlockNode>>(&node)) {
            const auto& blk = **block;
            if (!blk.opener.has_value()) {
                continue;
            }
            const auto kind = findFirstKeyword(blk.opener->tokens).kind;
            if (kind == KeywordKind::Namespace) {
                collectModuleDeclNames(blk.body, out, defines);
            } else if (isPpIfKind(kind)) {
                for (const auto& branch : ppBranches(blk, defines)) {
                    if (branch.live) {
                        collectModuleDeclNames(branch.body, out, defines);
                    }
                }
            }
        }
    }
}

// Collect parameter names from a procedure opener (between its `(` and `)`).
void collectParamNames(const std::vector<Token>& opener, std::vector<wxString>& out) {
    int depth = 0;
    bool expectName = false;
    for (const auto& tok : opener) {
        const auto op = tok.operatorKind;
        if (isOpenBracket(op)) {
            depth++;
            if (depth == 1) {
                expectName = true;
            }
            continue;
        }
        if (isCloseBracket(op)) {
            depth--;
            if (depth == 0) {
                break;
            }
            continue;
        }
        if (depth != 1) {
            continue;
        }
        if (op == OperatorKind::Comma) {
            expectName = true;
            continue;
        }
        if (tok.keywordKind == KeywordKind::As || op == OperatorKind::Assign) {
            expectName = false;
            continue;
        }
        if (expectName && isWordLike(tok.kind)) {
            if (isParamQualifier(tok)) {
                continue; // ByRef / ByVal — the name follows
            }
            out.push_back(wxString::FromUTF8(tok.text));
            expectName = false;
        }
    }
}

// Append field names declared directly in a UDT body (skips method prototypes).
void gatherFields(const std::vector<Node>& body, std::vector<wxString>& out) {
    for (const auto& node : body) {
        const auto* stmt = std::get_if<StatementNode>(&node);
        if (stmt == nullptr) {
            continue;
        }
        const Token* lead = firstSignificant(stmt->tokens);
        if (lead == nullptr || lead->keywordKind == KeywordKind::Declare) {
            continue;
        }
        collectDeclNames(stmt->tokens, out);
    }
}

} // namespace

void SymbolTable::globalSymbolCompletions(std::vector<wxString>& out) const {
    // Free-standing callables only — methods carry an owner qualifier.
    const auto addFreeStanding = [&out](const std::vector<Symbol>& vec) {
        for (const auto& sym : vec) {
            if (symbolOwner(sym).empty()) {
                out.push_back(sym.name);
            }
        }
    };
    addFreeStanding(m_subs);
    addFreeStanding(m_functions);

    // Typenames and macros are always global names.
    const auto addAll = [&out](const std::vector<Symbol>& vec) {
        for (const auto& sym : vec) {
            out.push_back(sym.name);
        }
    };
    addAll(m_types);
    addAll(m_unions);
    addAll(m_enums);
    addAll(m_macros);
    addAll(m_enumMembers); // non-explicit enum members are global names
}

void SymbolTable::moduleVariableCompletions(std::vector<wxString>& out) const {
    // Module-level Dim/Const/Var declarations — globally visible (no position
    // filter), including those nested in namespaces and `#if` branches.
    if (m_tree) {
        collectModuleDeclNames(m_tree->nodes, out, *m_defines);
    }
}

void SymbolTable::memberCompletionsAt(const int pos, std::vector<wxString>& out,
    const std::vector<std::shared_ptr<const SymbolTable>>& imported) const {
    // Walk out to the enclosing procedure; a type method's opener is qualified
    // (`Sub Vec.Foo`), so its owner is the type whose members are in scope.
    wxString owner;
    for (const auto* block = blockAt(pos); block != nullptr; block = block->parent) {
        if (!block->opener.has_value()) {
            continue;
        }
        const auto& toks = block->opener->tokens;
        const auto first = findFirstKeyword(toks);
        if (!isProcedureKind(first.kind)) {
            continue;
        }
        switch (first.kind) {
        case KeywordKind::Constructor:
        case KeywordKind::Destructor:
            owner = qualifiedNameAfter(toks, first.index + 1); // the type itself
            break;
        case KeywordKind::Operator:
            owner = operatorNameAfter(toks, first.index + 1).BeforeLast('.');
            break;
        default: // Sub / Function / Property
            owner = qualifiedNameAfter(toks, first.index + 1).BeforeLast('.');
            break;
        }
        break;
    }
    if (owner.empty()) {
        return;
    }

    // Members of `owner` from this file and from its #include closure — a type
    // declared in an included header may be implemented in this file.
    appendMembersOf(owner, out);
    for (const auto& table : imported) {
        if (table) {
            table->appendMembersOf(owner, out);
        }
    }
}

auto SymbolTable::findDefinition(const wxString& name,
    const std::vector<std::shared_ptr<const SymbolTable>>& imported) const -> std::optional<Location> {
    return findSymbol(name, /*preferDeclaration*/ false, imported);
}

auto SymbolTable::findDeclaration(const wxString& name,
    const std::vector<std::shared_ptr<const SymbolTable>>& imported) const -> std::optional<Location> {
    return findSymbol(name, /*preferDeclaration*/ true, imported);
}

auto SymbolTable::findSymbol(const wxString& name, const bool preferDeclaration,
    const std::vector<std::shared_ptr<const SymbolTable>>& imported) const -> std::optional<Location> {
    std::optional<Location> any;       // best match of any declaration kind (fallback)
    std::optional<Location> preferred; // best match of the preferred declaration kind
    bool anyOwn = false;
    bool preferredOwn = false;
    const auto consider = [&](const Symbol& sym, const std::filesystem::path& path, const bool own) {
        if (sym.line < 0) {
            return; // synthetic owner type — no real source location
        }
        if (!sym.name.IsSameAs(name, false) && !sym.name.AfterLast('.').IsSameAs(name, false)) {
            return; // case-insensitive; methods match on their unqualified name
        }
        if (!any || (own && !anyOwn)) {
            any = Location { .path = path, .line = sym.line };
            anyOwn = own;
        }
        if (sym.declaration == preferDeclaration && (!preferred || (own && !preferredOwn))) {
            preferred = Location { .path = path, .line = sym.line };
            preferredOwn = own;
        }
    };
    const auto scan = [&consider](const SymbolTable& table, const bool own) {
        for (const auto* vec : { &table.m_subs, &table.m_functions, &table.m_constructors,
                 &table.m_destructors, &table.m_operators, &table.m_properties, &table.m_types,
                 &table.m_unions, &table.m_enums, &table.m_macros, &table.m_enumMembers }) {
            for (const auto& sym : *vec) {
                consider(sym, table.m_sourcePath, own);
            }
        }
    };
    scan(*this, /*own*/ true);
    for (const auto& table : imported) {
        if (table) {
            scan(*table, /*own*/ false);
        }
    }
    return preferred ? preferred : any;
}

void SymbolTable::appendMembersOf(const wxString& owner, std::vector<wxString>& out) const {
    // The owner's callable members, by unqualified name.
    const auto addMembers = [&out, &owner](const std::vector<Symbol>& vec) {
        for (const auto& sym : vec) {
            if (symbolOwner(sym) == owner) {
                out.push_back(sym.name.AfterLast('.'));
            }
        }
    };
    addMembers(m_subs);
    addMembers(m_functions);
    addMembers(m_properties);

    // The owner type's data fields (`x As T`).
    if (const auto it = m_typeFields.find(owner); it != m_typeFields.end()) {
        out.insert(out.end(), it->second.begin(), it->second.end());
    }
}

void SymbolTable::localCompletionsAt(const int pos, std::vector<wxString>& out) const {
    // Walk scopes innermost-out. Each block contributes its parameters (always
    // in scope) and the locals declared in its direct body at or before `pos`.
    // Sibling blocks are off the chain, so their locals stay invisible.
    for (const auto* block = blockAt(pos); block != nullptr; block = block->parent) {
        if (block->opener.has_value()) {
            const auto first = findFirstKeyword(block->opener->tokens);
            if (isProcedureKind(first.kind)) {
                collectParamNames(block->opener->tokens, out);
            }
        }
        for (const auto& node : block->body) {
            const auto* stmt = std::get_if<StatementNode>(&node);
            if (stmt == nullptr) {
                continue;
            }
            const Token* lead = firstSignificant(stmt->tokens);
            if (lead == nullptr || lead->pos > pos || !isDeclLeader(*lead)) {
                continue;
            }
            collectDeclNames(stmt->tokens, out);
        }
    }
}

void SymbolTable::walkNodes(std::span<const Node> nodes) {
    for (const auto& node : nodes) {
        if (const auto* block = std::get_if<std::unique_ptr<BlockNode>>(&node)) {
            walkBlock(**block);
        } else if (const auto* stmt = std::get_if<StatementNode>(&node)) {
            // `#define` is a single-line directive (no block); capture it as a
            // macro — the name is the first word-like token after the directive
            // (function-like `#define NAME(a)` yields NAME before the paren).
            const auto first = findFirstKeyword(stmt->tokens);
            if (first.kind == KeywordKind::PpDefine) {
                if (const Token* name = findWordlikeAfter(stmt->tokens, first.index + 1)) {
                    m_macros.push_back(Symbol {
                        .kind = SymbolKind::Macro,
                        .name = wxString::FromUTF8(name->text),
                        .line = stmt->tokens[first.index].line,
                    });
                }
            } else if (first.kind == KeywordKind::Declare) {
                // `Declare Sub/Function/...` — a (header-style) forward declaration;
                // capture it as the callable it declares so headers contribute their
                // API. The procedure keyword follows `Declare`.
                for (std::size_t idx = first.index + 1; idx < stmt->tokens.size(); idx++) {
                    if (const auto kind = procSymbolKind(stmt->tokens[idx].keywordKind)) {
                        emit(*kind, stmt->tokens, idx, /*declaration*/ true);
                        break;
                    }
                }
            } else if (first.kind == KeywordKind::Type) {
                // A single-line `Type NAME As <target>` is a type alias (typedef);
                // the UDT form (`Type ... End Type`) is a block handled by
                // walkBlock. Capture the alias name so it completes as a typename.
                // Require an identifier after `Type` so malformed `Type As ...`
                // (no name) doesn't capture the `As` keyword as a bogus type.
                if (const Token* name = findWordlikeAfter(stmt->tokens, first.index + 1);
                    name != nullptr && name->kind == TokenKind::Identifier) {
                    emit(SymbolKind::Type, stmt->tokens, first.index);
                }
            }
        }
    }
}

void SymbolTable::gatherEnumMembers(std::span<const Node> body) {
    for (const auto& node : body) {
        if (const auto* stmt = std::get_if<StatementNode>(&node)) {
            // A member line starts with the enumerator identifier (optionally
            // `= value`); skip comments, blanks and any preprocessor directives.
            const Token* lead = firstSignificant(stmt->tokens);
            if (lead != nullptr && lead->kind == TokenKind::Identifier) {
                m_enumMembers.push_back(Symbol {
                    .kind = SymbolKind::Enum,
                    .name = wxString::FromUTF8(lead->text),
                    .line = lead->line,
                });
            }
        } else if (const auto* block = std::get_if<std::unique_ptr<BlockNode>>(&node)) {
            const auto& blk = **block;
            // Members guarded by `#if` import only from the live branches.
            if (blk.opener.has_value() && isPpIfKind(findFirstKeyword(blk.opener->tokens).kind)) {
                for (const auto& branch : ppBranchesCached(blk)) {
                    if (branch.live) {
                        gatherEnumMembers(branch.body);
                    }
                }
            }
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
        gatherFields(block.body, m_typeFields[qualifiedNameAfter(openerTokens, first.index + 1)]);
        break;
    case KeywordKind::Union:
        emit(SymbolKind::Union, openerTokens, first.index);
        gatherFields(block.body, m_typeFields[qualifiedNameAfter(openerTokens, first.index + 1)]);
        break;
    case KeywordKind::Enum:
        emit(SymbolKind::Enum, openerTokens, first.index);
        // A non-explicit enum imports its members into the enclosing namespace
        // (C-style), so capture them as global names; explicit enums keep them
        // scoped and are left alone.
        if (!enumIsExplicit(openerTokens)) {
            gatherEnumMembers(block.body);
        }
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
        // A `#if` chain: recurse only the branches live under the current
        // defines, so declarations behind an inactive `#ifdef` are dropped. An
        // Unknown condition keeps its branch, so we never wrongly drop code.
        for (const auto& branch : ppBranchesCached(block)) {
            if (branch.live) {
                walkNodes(branch.body);
            }
        }
        break;
    case KeywordKind::PpElse:
    case KeywordKind::PpElseIf:
    case KeywordKind::PpElseIfDef:
    case KeywordKind::PpElseIfNDef:
        // Reached only for a stray `#else`/`#elseif` without a leading `#if`;
        // a well-formed chain is consumed by `ppBranches` above. Walk through.
        walkNodes(block.body);
        break;
    default:
        break;
    }
}

void SymbolTable::emit(
    const SymbolKind kind,
    const std::vector<Token>& opener,
    const std::size_t keywordIdx,
    const bool declaration
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
    // Cache the owner now (kind-dependent), so query-time filters read the field
    // instead of recomputing a substring per symbol. Non-member kinds (Type /
    // Union / Enum / Macro) leave it empty; this is the only site that emits a
    // member-capable symbol.
    wxString owner;
    switch (kind) {
    case SymbolKind::Constructor:
    case SymbolKind::Destructor:
        owner = name; // the whole name is the owning type
        break;
    case SymbolKind::Sub:
    case SymbolKind::Function:
    case SymbolKind::Operator:
    case SymbolKind::Property:
        owner = name.BeforeLast('.'); // empty when free-standing
        break;
    default:
        break;
    }
    Symbol sym {
        .kind = kind,
        .name = std::move(name),
        .owner = std::move(owner),
        .line = opener.empty() ? 0 : opener.front().line,
        .declaration = declaration,
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
    hash = hashVector(hash, SymbolKind::Enum, m_enumMembers);
    hash = hashVector(hash, SymbolKind::Macro, m_macros);
    hash = hashIncludes(hash, m_includes);
    m_hash = hash;
}
