//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FBSciLexer.hpp"
#include "StyleContext.h"
using namespace fbide;

namespace {

// region ---------- Metadata ----------

constexpr std::array lexicalClasses {
    Lexilla::LexicalClass { +FBSciLexerState::Default,          "state.default",          "default",                 "Default" },
    Lexilla::LexicalClass { +FBSciLexerState::Comment,          "state.comment",          "comment line",            "Single-line comment" },
    Lexilla::LexicalClass { +FBSciLexerState::MultilineComment, "state.comment.block",    "comment",                 "Block comment" },
    Lexilla::LexicalClass { +FBSciLexerState::Number,           "state.number",           "literal numeric",         "Number" },
    Lexilla::LexicalClass { +FBSciLexerState::String,           "state.string",           "literal string",          "String literal" },
    Lexilla::LexicalClass { +FBSciLexerState::StringOpen,       "state.string.unclosed",  "literal string unclosed", "Unclosed string" },
    Lexilla::LexicalClass { +FBSciLexerState::Identifier,       "state.identifier",       "identifier",              "Identifier" },
    Lexilla::LexicalClass { +FBSciLexerState::Keyword1,         "state.keyword",          "keyword",                 "Keywords" },
    Lexilla::LexicalClass { +FBSciLexerState::Keyword2,         "state.keyword2",         "keyword",                 "Types" },
    Lexilla::LexicalClass { +FBSciLexerState::Keyword3,         "state.keyword3",         "keyword",                 "Operators" },
    Lexilla::LexicalClass { +FBSciLexerState::Keyword4,         "state.keyword4",         "keyword",                 "Defines" },
    Lexilla::LexicalClass { +FBSciLexerState::Keyword5,         "state.keyword5",         "keyword",                 "User keywords" },
    Lexilla::LexicalClass { +FBSciLexerState::Operator,         "state.operator",         "operator",                "Operator" },
    Lexilla::LexicalClass { +FBSciLexerState::Label,            "state.label",            "label",                   "Label" },
    Lexilla::LexicalClass { +FBSciLexerState::Constant,         "state.constant",         "keyword constant value",  "Built-in constants" },
    Lexilla::LexicalClass { +FBSciLexerState::Preprocessor,     "state.preprocessor",     "preprocessor",            "Preprocessor" },
};

constexpr std::array<const char*, FBSciLexer::WORD_LIST_COUNT + 1> wordListDescriptions {
    "Keywords",
    "Types",
    "Operators",
    "Defines",
    "Constants",
    nullptr
};

//endregion

// region ---------- Utilities ----------

enum class CharClass: int {
    Whitespace = 1 << 0,
    Operator   = 1 << 1,
    Identifier = 1 << 2,
    Digit      = 1 << 3,
    HexDigit   = 1 << 4,
    BinDigit   = 1 << 5,
    Letter     = 1 << 6
};

constexpr auto operator+(const CharClass& rhs) -> int {
    return static_cast<int>(rhs);
}

// clang-format off
constexpr std::array charClasses = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  1,   1,  0,  0,  1,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,
    1,  2,  0,  2,  2,  2,  2,  2,  2,  2,   2,  2,  2,  2,  10, 2,
    60, 60, 28, 28, 28, 28, 28, 28, 28, 28,  2,  2,  2,  2,  2,  2,
    2,  84, 84, 84, 84, 84, 84, 68, 68, 68, 68, 68, 68, 68, 68, 68,
    68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,  2,  2,  2,  2, 68,
    2,  84, 84, 84, 84, 84, 84, 68, 68, 68, 68, 68, 68, 68, 68, 68,
    68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,  2,  2,  2,  2,  0
};
// clang-format on

bool isSpace(const int ch) {
    return ch < charClasses.size() && (charClasses[ch] & +CharClass::Whitespace);
}

bool isOperator(const int ch) {
    return ch < charClasses.size() && (charClasses[ch] & +CharClass::Operator);
}

bool isIdentifier(const int ch) {
    return ch < charClasses.size() && (charClasses[ch] & +CharClass::Identifier);
}

bool isDigit(const int ch) {
    return ch < charClasses.size() && (charClasses[ch] & +CharClass::Digit);
}

bool isHexDigit(const int ch) {
    return ch < charClasses.size() && (charClasses[ch] & +CharClass::HexDigit);
}

bool isBinDigit(const int ch) {
    return ch < charClasses.size() && (charClasses[ch] & +CharClass::BinDigit);
}

bool isLetter(const int ch) {
    return ch < charClasses.size() && (charClasses[ch] & +CharClass::Letter);
}

int lowerCase(const int c) {
    if (c >= 'A' && c <= 'Z') {
        return 'a' + c - 'A';
    }
    return c;
}

// endregion

} // namespace

// region ---------- Scintilla Boilerplate ----------

FBSciLexer::FBSciLexer()
    : DefaultLexer("freebasic", SCLEX_AUTOMATIC, lexicalClasses.data(), lexicalClasses.size()) {}

const char* SCI_METHOD FBSciLexer::DescribeWordListSets() {
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    static const std::string desc = [] {
        std::string result;
        for (const auto* entry : wordListDescriptions) {
            if (entry == nullptr) break;
            if (!result.empty()) result += '\n';
            result += entry;
        }
        return result;
    }();
    return desc.c_str();
}

Sci_Position SCI_METHOD FBSciLexer::WordListSet(const int n, const char* wl) {
    if (n >= 0 && n < WORD_LIST_COUNT) {
        if (m_wordLists[n].Set(wl)) {
            return 0;
        }
    }
    return -1;
}

auto FBSciLexer::Create() -> ILexer5* {
    return new FBSciLexer();
}

// endregion

// region ---------- Lexing ----------

void SCI_METHOD FBSciLexer::Lex(
    const Sci_PositionU startPos,
    const Sci_Position lengthDoc,
    const int initStyle,
    Scintilla::IDocument* pAccess
) {
    Lexilla::LexAccessor styler(pAccess);
    styler.StartAt(startPos);
    styler.StartSegment(startPos);
    Lexilla::StyleContext sc(startPos, lengthDoc, initStyle, styler);

    for (; sc.More(); sc.Forward()) {
        switch (sc.state) {
        case +FBSciLexerState::Default:
            lexDefault(sc);
            break;
        case +FBSciLexerState::Comment:
            lexComment(sc);
            break;
        case +FBSciLexerState::MultilineComment:
            lexMultilineComment(sc);
            break;
        case +FBSciLexerState::Number:
            lexNumber(sc);
            break;
        case +FBSciLexerState::String:
            lexString(sc);
            break;
        case +FBSciLexerState::StringOpen:
            lexStringOpen(sc);
            break;
        case +FBSciLexerState::Identifier:
            lexIdentifier(sc);
            break;
        case +FBSciLexerState::Operator:
            sc.SetState(+FBSciLexerState::Default);
            break;
        case +FBSciLexerState::Preprocessor:
            lexPreprocessor(sc);
            break;
        default:
            break;
        }
    }

    sc.Complete();
}

void FBSciLexer::lexDefault(Lexilla::StyleContext& /*sc*/) const noexcept {
}

void FBSciLexer::lexComment(Lexilla::StyleContext& /*sc*/) const noexcept {
}

void FBSciLexer::lexMultilineComment(Lexilla::StyleContext& /*sc*/) const noexcept {
}

void FBSciLexer::lexNumber(Lexilla::StyleContext& /*sc*/) const noexcept {
}

void FBSciLexer::lexString(Lexilla::StyleContext& /*sc*/) const noexcept {
}

void FBSciLexer::lexStringOpen(Lexilla::StyleContext& /*sc*/) const noexcept {
}

void FBSciLexer::lexIdentifier(Lexilla::StyleContext& /*sc*/) const noexcept {
}

void FBSciLexer::lexPreprocessor(Lexilla::StyleContext& /*sc*/) const noexcept {
}

// endregion

// region ---------- Folding ----------

void SCI_METHOD FBSciLexer::Fold(
    Sci_PositionU /*startPos*/,
    Sci_Position /*lengthDoc*/,
    int /*initStyle*/,
    Scintilla::IDocument* /*pAccess*/
) {
    // Folding not implemented yet
}

// endregion
