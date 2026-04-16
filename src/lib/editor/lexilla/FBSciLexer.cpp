//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppDFALocalValueEscapesFunction
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
    m_styler = &styler;
    styler.StartAt(startPos);
    styler.StartSegment(startPos);

    Lexilla::StyleContext sc(startPos, lengthDoc, initStyle, styler);
    m_sc = &sc;

    // unknown line
    m_line = INVALID_LINE;

    for (; sc.More(); sc.Forward()) {
        using enum FBSciLexerState;

        if (m_sc->atLineStart) {
            lexLineStart();
        }

        // Lex non-default states
        switch (sc.state) {
        case +Comment:
            lexComment();
            break;
        case +MultilineComment:
            lexMultilineComment();
            break;
        case +Number:
            lexNumber();
            break;
        case +StringOpen:
            lexStringOpen();
            break;
        case +Identifier:
            lexIdentifier();
            break;
        case +Operator:
            lexOperator();
            break;
        case +Preprocessor:
            lexPreprocessor();
            break;
        default:
            break;
        }

        // we in default?
        if (sc.atLineEnd) {
            styler.SetLineState(m_line, m_lineState.toInt());
        } else if (sc.state == +Default) {
            lexDefault();
        }
    }

    m_sc = nullptr;
    m_styler = nullptr;
    sc.Complete();
}

void FBSciLexer::lexLineStart() noexcept{
    const auto newLine = m_sc->currentLine;

    // is the next line?
    if (newLine - 1 == m_line) {
        m_previousLineState = m_lineState;
    }
    // initial, or jumped ahead
    else if (newLine > 0) {
        m_previousLineState = LineState::fromInt(m_styler->GetLineState(newLine - 1));
    }
    // very first line
    else {
        m_previousLineState = {};
    }

    m_line = newLine;
    m_lineState = {};

    m_isFirst = m_previousLineState.getLineContinuation();
    m_lineState.commentNestLevel = m_previousLineState.commentNestLevel;
}

void FBSciLexer::resetToDefault() noexcept {
    m_sc->SetState(+FBSciLexerState::Default);
    m_isFirst = false;
}

void FBSciLexer::lexDefault() noexcept {
    using enum FBSciLexerState;

    // white space
    if (isSpace(m_sc->ch)) {
        return;
    }

    // single line comment
    if (m_sc->ch == '\'') {
        m_sc->SetState(+Comment);
    }
    // multi line comment
    else if (m_sc->ch == '/' && m_sc->chNext == '\'') {
        m_sc->SetState(+MultilineComment);
        m_sc->Forward();
        m_lineState.commentNestLevel++;
    }
    // preprocessor? operator?
    else if (m_sc->ch == '#') {
        m_sc->SetState(m_isFirst ? +Preprocessor : +Operator);
    }
    // Numbers
    else if (isDigit(m_sc->ch)) {
        m_sc->SetState(+Number);
    }
    // operators
    else if (isOperator(m_sc->ch)) {
        m_sc->SetState(+Operator);
    }
    // line continuation
    else if (m_sc->ch == '_' && !isIdentifier(m_sc->chNext)) {
        m_lineState.setLineContinuation(true);
        m_sc->SetState(+Comment);
    }
    // identifier
    else if (isIdentifier(m_sc->ch)) {
        m_sc->SetState(+Identifier);
    }
    // String literal
    else if (m_sc->ch == '"') {
        m_sc->SetState(+StringOpen);
    }
}

void FBSciLexer::lexComment() noexcept {
    if (m_sc->atLineEnd) {
        m_sc->SetState(+FBSciLexerState::Default); // no reset!
    }
}

void FBSciLexer::lexMultilineComment() noexcept {
    switch (m_sc->ch) {
    case '\'':
        if (m_sc->chNext == '/') {
            m_sc->Forward();
            m_lineState.commentNestLevel--;
            if (m_lineState.commentNestLevel == 0) {
                m_sc->ForwardSetState(+FBSciLexerState::Default); // no reset!
            }
        }
        break;
    case '/':
        if (m_sc->chNext == '\'') {
            m_sc->Forward();
            m_lineState.commentNestLevel++;
        }
        break;
    default:
        break;
    }
}

void FBSciLexer::lexNumber() noexcept {
    if (!isDigit(m_sc->ch)) {
        resetToDefault();
    }
}

void FBSciLexer::lexStringOpen() noexcept {
    // closing string
    if (m_sc->ch == '"') {
        m_sc->Forward();
        if (m_sc->ch == '\"') {
            return;
        }
        m_sc->ChangeState(+FBSciLexerState::String);
        resetToDefault();
    }
    if (m_sc->atLineEnd) {
        m_sc->SetState(+FBSciLexerState::Default); // no reset
    }
}

void FBSciLexer::lexIdentifier() noexcept {
    if (!isIdentifier(m_sc->ch)) {
        if (m_sc->ch == ':' && m_isFirst) {
            m_sc->ChangeState(+FBSciLexerState::Label);
        }
        resetToDefault();
    }
}

void FBSciLexer::lexOperator() noexcept {
    if (!isOperator(m_sc->ch)) {
        resetToDefault();
    }
}

void FBSciLexer::lexPreprocessor() noexcept {
    if (m_sc->atLineEnd) {
        m_sc->SetState(+FBSciLexerState::Default); // no reset
    }
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
