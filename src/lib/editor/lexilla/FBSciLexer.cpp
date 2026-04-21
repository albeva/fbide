//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppDFALocalValueEscapesFunction
#include "FBSciLexer.hpp"
#include "CharCategory.hpp"
#include "SciLexer.h"
#include "Scintilla.h"
#include "StyleContext.h"
using namespace fbide;

namespace {

// region ---------- Metadata ----------

constexpr std::array lexicalClasses {
    Lexilla::LexicalClass { +ThemeCategory::Default, "state.default", "default", "Default" },
    Lexilla::LexicalClass { +ThemeCategory::Comment, "state.comment", "comment line", "Single-line comment" },
    Lexilla::LexicalClass { +ThemeCategory::MultilineComment, "state.comment.block", "comment", "Block comment" },
    Lexilla::LexicalClass { +ThemeCategory::Number, "state.number", "literal numeric", "Number" },
    Lexilla::LexicalClass { +ThemeCategory::String, "state.string", "literal string", "String literal" },
    Lexilla::LexicalClass { +ThemeCategory::StringOpen, "state.string.unclosed", "literal string unclosed", "Unclosed string" },
    Lexilla::LexicalClass { +ThemeCategory::Identifier, "state.identifier", "identifier", "Identifier" },
    Lexilla::LexicalClass { +ThemeCategory::Keyword1, "state.keyword", "keyword", "Keywords" },
    Lexilla::LexicalClass { +ThemeCategory::Keyword2, "state.keyword2", "keyword", "Types" },
    Lexilla::LexicalClass { +ThemeCategory::Keyword3, "state.keyword3", "keyword", "Operators" },
    Lexilla::LexicalClass { +ThemeCategory::Keyword4, "state.keyword4", "keyword", "Defines" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordCustom1, "state.custom1", "keyword", "User keywords 1" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordCustom2, "state.custom2", "keyword", "User keywords 2" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordPP, "state.keyword.preprocessor", "keyword", "Preprocessor keyword" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordAsm1, "state.keyword.asm1", "keyword", "Asm keywords 1" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordAsm2, "state.keyword.asm2", "keyword", "Asm keywords 2" },
    Lexilla::LexicalClass { +ThemeCategory::Operator, "state.operator", "operator", "Operator" },
    Lexilla::LexicalClass { +ThemeCategory::Label, "state.label", "label", "Label" },
    Lexilla::LexicalClass { +ThemeCategory::Preprocessor, "state.preprocessor", "preprocessor", "Preprocessor" },
    Lexilla::LexicalClass { +ThemeCategory::Error, "state.error", "errpr", "Syntax error" },
};
static_assert(lexicalClasses.size() == kThemeCategoryCount);

constexpr std::array<const char*, kThemeKeywordGroupsCount + 1> wordListDescriptions {
#define GROUPS(NAME) #NAME,
    DEFINE_THEME_KEYWORD_GROUPS(GROUPS)
#undef GROUPS
        nullptr
};

// endregion

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
            if (entry == nullptr)
                break;
            if (!result.empty())
                result += '\n';
            result += entry;
        }
        return result;
    }();
    return desc.c_str();
}

Sci_Position SCI_METHOD FBSciLexer::WordListSet(const int n, const char* wl) {
    const auto idx = static_cast<std::size_t>(n);
    if (idx < kThemeKeywordGroupsCount) {
        if (m_wordLists[idx].Set(wl)) {
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

    Lexilla::StyleContext sc(startPos, static_cast<Sci_PositionU>(lengthDoc), initStyle, styler);
    m_sc = &sc;

    // unknown line
    m_line = INVALID_LINE;

    for (; sc.More(); sc.Forward()) {
        using enum ThemeCategory;

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
        case +Error:
            if (isValidAfterNumOrWord(sc.ch)) {
                resetToDefault();
            }
            break;
        default:
            break;
        }

        // we in default?
        if (sc.atLineEnd) {
            lexLineEnd();
        } else if (sc.state == +Default) {
            lexDefault();
        }
    }

    m_sc = nullptr;
    m_styler = nullptr;
    sc.Complete();
}

void FBSciLexer::lexLineStart() noexcept {
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

    if (m_previousLineState.continueLine) {
        m_isFirst = m_previousLineState.isFirst;
        m_fieldAccess = m_previousLineState.fieldAccess;
    } else {
        m_isFirst = true;
        m_fieldAccess = false;
    }
    m_lineState.commentNestLevel = m_previousLineState.commentNestLevel;
    m_asmBlock = m_previousLineState.asmBlock;

    if (m_previousLineState.continuePP) {
        m_sc->SetState(+ThemeCategory::Preprocessor);
    }
}

void FBSciLexer::lexLineEnd() noexcept {
    m_lineState.isFirst = m_isFirst;
    m_lineState.fieldAccess = m_fieldAccess;
    m_lineState.asmBlock = m_asmBlock;
    m_styler->SetLineState(m_line, m_lineState.toInt());
}

void FBSciLexer::resetToDefault() noexcept {
    m_sc->SetState(+ThemeCategory::Default);
    m_isFirst = false;
}

bool FBSciLexer::canAccessMember() noexcept {
    using enum ThemeCategory;
    const auto st = m_sc->state;
    if (st == +Comment) {
        return m_lineState.continueLine;
    }
    return st == +Identifier || st == +MultilineComment;
}

void FBSciLexer::lexDefault() noexcept {
    using enum ThemeCategory;

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
    // .
    else if (m_sc->ch == '.') {
        if (isDigit(m_sc->chNext)) {
            m_numberForm = NumberForm::Fraction;
            m_sc->SetState(+Number);
        } else if (m_sc->chNext == '.') {
            m_sc->SetState(+Operator);
            m_sc->Forward();
            if (m_sc->chNext == '.') {
                m_sc->Forward();
                if (m_sc->chNext == '.') {
                    m_sc->ChangeState(+Error);
                    while (m_sc->chNext == '.') {
                        m_sc->Forward();
                    }
                }
            }
        } else {
            m_sc->SetState(+Operator);
            m_fieldAccess = true;
            return; // short circuit!
        }
    }
    // ->
    else if (m_sc->ch == '-' && m_sc->chNext == '>') {
        m_sc->SetState(+Operator);
        m_sc->Forward();
        m_fieldAccess = true;
        return; // short circuit!
    }
    // Numbers
    else if (isDigit(m_sc->ch)) {
        m_numberForm = NumberForm::Decimal;
        m_sc->SetState(+Number);
    }
    // number format?
    else if (m_sc->ch == '&') {
        const auto lcn = fastUnsafeLowerCase(m_sc->chNext);
        if (lcn == 'h') {
            m_numberForm = NumberForm::Hexadecimal;
            m_sc->SetState(+Number);
            m_sc->Forward();
        } else if (lcn == 'o') {
            m_numberForm = NumberForm::Octal;
            m_sc->SetState(+Number);
            m_sc->Forward();
        } else if (lcn == 'b') {
            m_numberForm = NumberForm::Binary;
            m_sc->SetState(+Number);
            m_sc->Forward();
        } else {
            m_sc->SetState(+Operator);
        }
    }
    // !"string literal"
    else if (m_sc->ch == '!' && m_sc->chNext == '"') {
        m_slashEscapableString = true;
        m_sc->SetState(+StringOpen);
        m_sc->Forward();
    }
    // $"string literal"
    else if (m_sc->ch == '$' && m_sc->chNext == '"') {
        m_sc->SetState(+StringOpen);
        m_sc->Forward();
    }
    // operators
    else if (isOperator(m_sc->ch)) {
        m_sc->SetState(+Operator);
    }
    // line continuation
    else if (m_sc->ch == '_' && !isIdentifier(m_sc->chNext)) {
        m_lineState.continueLine = true;
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

    if (m_fieldAccess && !canAccessMember()) {
        m_fieldAccess = false;
    }
}

void FBSciLexer::lexComment() noexcept {
    if (m_sc->atLineEnd) {
        m_sc->SetState(+ThemeCategory::Default); // no reset!
    }
}

void FBSciLexer::lexMultilineComment() noexcept {
    switch (m_sc->ch) {
    case '\'':
        if (m_sc->chNext == '/') {
            m_sc->Forward();
            m_lineState.commentNestLevel--;
            if (m_lineState.commentNestLevel == 0) {
                m_sc->ForwardSetState(+ThemeCategory::Default); // no reset!
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
    const auto finish = [&] {
        if (isValidAfterNumOrWord(m_sc->ch)) {
            resetToDefault();
        } else {
            m_sc->ChangeState(+ThemeCategory::Error);
        }
    };

    // %, &, u[l[l]], l[l]
    const auto integralSuffixes = [&] {
        const auto lc = fastUnsafeLowerCase(m_sc->ch);
        if (m_sc->ch == '%' || m_sc->ch == '&') {
            m_sc->Forward();
        } else if (lc == 'u') {
            m_sc->Forward();
            if (fastUnsafeLowerCase(m_sc->ch) == 'l') {
                m_sc->Forward();
                if (fastUnsafeLowerCase(m_sc->ch) == 'l') {
                    m_sc->Forward();
                }
            }
        } else if (lc == 'l') {
            m_sc->Forward();
            if (fastUnsafeLowerCase(m_sc->ch) == 'l') {
                m_sc->Forward();
            }
        }
        finish();
    };

    // !, #, F, D
    const auto fpSuffixes = [&] {
        const auto lc = fastUnsafeLowerCase(m_sc->ch);
        if (m_sc->ch == '!' || m_sc->ch == '#' || lc == 'f' || lc == 'd') {
            m_sc->Forward();
        }
        finish();
    };

    // Try to enter exponent: (D|E)[+|-]
    const auto tryExponent = [&] -> bool {
        const auto lc = fastUnsafeLowerCase(m_sc->ch);
        if ((lc == 'e' || lc == 'd') && (isDigit(m_sc->chNext) || m_sc->chNext == '+' || m_sc->chNext == '-')) {
            m_numberForm = NumberForm::Exponent;
            m_sc->Forward();
            if (m_sc->ch == '+' || m_sc->ch == '-') {
                m_sc->Forward();
            }
            return true;
        }
        return false;
    };

    switch (m_numberForm) {
    case NumberForm::Decimal:
        if (m_sc->ch == '.') {
            m_numberForm = NumberForm::Fraction;
        } else if (!isDigit(m_sc->ch)) {
            if (!tryExponent()) {
                const auto lc = fastUnsafeLowerCase(m_sc->ch);
                if (m_sc->ch == '!' || m_sc->ch == '#' || lc == 'f' || lc == 'd') {
                    m_sc->Forward();
                    finish();
                } else {
                    integralSuffixes();
                }
            }
        }
        break;
    case NumberForm::Fraction:
        if (!isDigit(m_sc->ch)) {
            if (!tryExponent()) {
                fpSuffixes();
            }
        }
        break;
    case NumberForm::Exponent:
        if (!isDigit(m_sc->ch)) {
            fpSuffixes();
        }
        break;
    case NumberForm::Hexadecimal:
        if (!isHexDigit(m_sc->ch)) {
            integralSuffixes();
        }
        break;
    case NumberForm::Octal:
        if (!isOctDigit(m_sc->ch)) {
            integralSuffixes();
        }
        break;
    case NumberForm::Binary:
        if (!isBinDigit(m_sc->ch)) {
            integralSuffixes();
        }
        break;
    }
}

void FBSciLexer::lexStringOpen() noexcept {
    // closing string
    if (m_sc->ch == '"') {
        m_sc->Forward();
        if (m_sc->ch == '\"') {
            return;
        }
        m_sc->ChangeState(+ThemeCategory::String);
        resetToDefault();
        m_slashEscapableString = false;
    } else if (m_sc->ch == '\\' && m_sc->chNext == '\"' && m_slashEscapableString) {
        m_sc->Forward();
    } else if (m_sc->atLineEnd) {
        m_sc->SetState(+ThemeCategory::Default); // no reset
        m_slashEscapableString = false;
    }
}

void FBSciLexer::lexIdentifier() noexcept {
    if (!isIdentifier(m_sc->ch)) {
        if (m_sc->ch == ':' && m_isFirst) {
            m_sc->Forward();
            m_sc->ChangeState(+ThemeCategory::Label);
        } else if (m_fieldAccess) {
            m_fieldAccess = false;
        } else {
            if (!identifyKeyword()) {
                return;
            }
        }
        resetToDefault();
    }
}

auto FBSciLexer::identifyKeyword() noexcept -> bool {
    m_sc->GetCurrentLowered(m_identBuffer.data(), m_identBuffer.size());
    if (strcmp("rem", m_identBuffer.data()) == 0) {
        m_sc->ChangeState(+ThemeCategory::Comment);
        return false;
    }

    if (m_isFirst) {
        if (m_asmBlock) {
            // "end asm" terminates the block. Peek past the trailing
            // whitespace to confirm "asm" follows as a separate token, so
            // bare "end" (not a valid FB statement in asm, but guard anyway)
            // or something like "endasm" does not exit the block early.
            if (strcmp("end", m_identBuffer.data()) == 0) {
                auto pos = static_cast<Sci_Position>(m_sc->currentPos);
                while (true) {
                    const char c = m_styler->SafeGetCharAt(pos, '\0');
                    if (c != ' ' && c != '\t') {
                        break;
                    }
                    pos++;
                }
                if (fastUnsafeLowerCase(m_styler->SafeGetCharAt(pos, '\0')) == 'a'
                    && fastUnsafeLowerCase(m_styler->SafeGetCharAt(pos + 1, '\0')) == 's'
                    && fastUnsafeLowerCase(m_styler->SafeGetCharAt(pos + 2, '\0')) == 'm'
                    && !isIdentifier(static_cast<unsigned char>(m_styler->SafeGetCharAt(pos + 3, '\0')))) {
                    m_asmBlock = false;
                }
            }
        } else if (strcmp("asm", m_identBuffer.data()) == 0) {
            m_asmBlock = true;
            m_sc->ChangeState(+ThemeCategory::Keyword1);
            return true;
        }
    }

    constexpr std::size_t pp = indexOfKeywordGroup(ThemeCategory::KeywordPP);
    const std::size_t first = m_asmBlock ? pp + 1 : 0;
    const std::size_t last = m_asmBlock ? kThemeKeywordGroupsCount : pp;

    for (std::size_t index = first; index < last; index++) {
        if (m_wordLists[index].InList(m_identBuffer.data())) {
            m_sc->ChangeState(+kThemeKeywordCategories[index]);
            break;
        }
    }
    return true;
}

void FBSciLexer::lexOperator() noexcept {
    if (!isOperator(m_sc->ch)) {
        resetToDefault();
    }
}

void FBSciLexer::lexPreprocessor() noexcept {
    if (m_sc->atLineEnd) {
        m_sc->SetState(+ThemeCategory::Default); // no reset
        return;
    }

    if (m_sc->ch == '_') {
        if (not isIdentifier(m_sc->chPrev) and not isIdentifier(m_sc->chNext)) {
            m_lineState.continuePP = true;
            m_sc->SetState(+ThemeCategory::Comment);
            return;
        }
    }

    if (isIdentifier(m_sc->ch)) {
        m_sc->SetState(+ThemeCategory::Preprocessor);
        while (isIdentifier(m_sc->ch) && m_sc->More()) {
            m_sc->Forward();
        }

        m_sc->GetCurrentLowered(m_identBuffer.data(), m_identBuffer.size());
        constexpr std::size_t pp = indexOfKeywordGroup(ThemeCategory::KeywordPP);
        if (m_wordLists[pp].InList(m_identBuffer.data())) {
            m_sc->ChangeState(+kThemeKeywordCategories[pp]);
        }
        m_sc->SetState(+ThemeCategory::Preprocessor);
    }
}

// endregion

// region ---------- Folding ----------

namespace {

/// Returns true if every non-whitespace character on the line has MultilineComment style.
/// Blank lines return false.
auto isPureMultilineCommentLine(Lexilla::LexAccessor& styler, const Sci_Position line) -> bool {
    const Sci_Position lineStart = styler.LineStart(line);
    const Sci_Position nextStart = styler.LineStart(line + 1);
    bool hasContent = false;
    for (Sci_Position p = lineStart; p < nextStart; p++) {
        const char c = styler.SafeGetCharAt(p, '\0');
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0') {
            continue;
        }
        hasContent = true;
        if (styler.StyleIndexAt(p) != +ThemeCategory::MultilineComment) {
            return false;
        }
    }
    return hasContent;
}

/// Returns true if the line starts inside an already-open multi-line comment.
/// Detected via the style of the previous line's trailing newline character.
auto lineStartsInsideMultilineComment(Lexilla::LexAccessor& styler, const Sci_Position line) -> bool {
    if (line <= 0) {
        return false;
    }
    const Sci_Position prevEnd = styler.LineStart(line) - 1;
    if (prevEnd < 0) {
        return false;
    }
    return styler.StyleIndexAt(prevEnd) == +ThemeCategory::MultilineComment;
}

/// Compute the raw indentation (in columns) of a line. Tabs round up to the next
/// multiple of 8. Sets `blank` to true when the line has no non-whitespace content.
auto computeLineIndent(Lexilla::LexAccessor& styler, const Sci_Position line, bool& blank) -> int {
    const Sci_Position lineStart = styler.LineStart(line);
    const Sci_Position nextStart = styler.LineStart(line + 1);

    int indent = 0;
    Sci_Position pos = lineStart;
    while (pos < nextStart) {
        const char c = styler.SafeGetCharAt(pos, '\0');
        if (c == ' ') {
            indent++;
            pos++;
            continue;
        }
        if (c == '\t') {
            indent = (indent / 8 + 1) * 8;
            pos++;
            continue;
        }
        break;
    }

    const char c = styler.SafeGetCharAt(pos, '\0');
    blank = (pos >= nextStart || c == '\r' || c == '\n' || c == '\0');
    return indent;
}

/// Locate the opening line of a foldable multi-line comment block containing `line`.
/// Returns -1 when the block is not foldable (either the line is not pure ML content,
/// or the opener also has non-comment content).
auto foldableMultilineCommentOpener(Lexilla::LexAccessor& styler, const Sci_Position line) -> Sci_Position {
    if (!isPureMultilineCommentLine(styler, line)) {
        return -1;
    }
    if (!lineStartsInsideMultilineComment(styler, line)) {
        return line;
    }
    Sci_Position opener = line - 1;
    while (opener > 0 && lineStartsInsideMultilineComment(styler, opener)) {
        opener--;
    }
    if (!isPureMultilineCommentLine(styler, opener)) {
        return -1;
    }
    return opener;
}

/// Compute the fold level for a line.
/// Returns SC_FOLDLEVELBASE + indent columns, OR'd with WHITEFLAG for blank lines.
/// Pure multi-line-comment continuation lines are bumped past the opener's indent so
/// the opener becomes a fold header and the continuation lines fold inside it.
auto computeFoldIndent(Lexilla::LexAccessor& styler, const Sci_Position line) -> int {
    bool blank = false;
    const int indent = computeLineIndent(styler, line, blank);
    int level = SC_FOLDLEVELBASE + indent;
    if (blank) {
        return level | SC_FOLDLEVELWHITEFLAG;
    }

    const Sci_Position opener = foldableMultilineCommentOpener(styler, line);
    if (opener >= 0 && opener != line) {
        bool dummy = false;
        const int openerIndent = computeLineIndent(styler, opener, dummy);
        return SC_FOLDLEVELBASE + openerIndent + 1;
    }
    return level;
}

} // namespace

void SCI_METHOD FBSciLexer::Fold(
    Sci_PositionU startPos,
    const Sci_Position lengthDoc,
    int /*initStyle*/,
    Scintilla::IDocument* pAccess
) {
    Lexilla::LexAccessor styler(pAccess);
    const Sci_PositionU endPos = startPos + static_cast<Sci_PositionU>(lengthDoc);

    // Backtrack one line so the previous line's header flag can be revised
    // in light of the newly-lexed range.
    Sci_Position lineCurrent = styler.GetLine(static_cast<Sci_Position>(startPos));
    if (startPos > 0 && lineCurrent > 0) {
        lineCurrent--;
        startPos = static_cast<Sci_PositionU>(styler.LineStart(lineCurrent));
    }

    int indentCurrent = computeFoldIndent(styler, lineCurrent);
    char chNext = styler.SafeGetCharAt(static_cast<Sci_Position>(startPos), '\0');
    for (Sci_PositionU i = startPos; i < endPos; i++) {
        const char ch = chNext;
        chNext = styler.SafeGetCharAt(static_cast<Sci_Position>(i + 1), '\0');

        const bool atEOL = (ch == '\r' && chNext != '\n') || ch == '\n' || (i + 1 == endPos);
        if (!atEOL) {
            continue;
        }

        int lev = indentCurrent;
        const int indentNext = computeFoldIndent(styler, lineCurrent + 1);
        if (!(indentCurrent & SC_FOLDLEVELWHITEFLAG)) {
            if ((indentCurrent & SC_FOLDLEVELNUMBERMASK) < (indentNext & SC_FOLDLEVELNUMBERMASK)) {
                lev |= SC_FOLDLEVELHEADERFLAG;
            } else if (indentNext & SC_FOLDLEVELWHITEFLAG) {
                // Look ahead past whitespace-only lines so a header above blank
                // gaps still gets marked when real content follows at deeper indent.
                const int indentNext2 = computeFoldIndent(styler, lineCurrent + 2);
                if ((indentCurrent & SC_FOLDLEVELNUMBERMASK) < (indentNext2 & SC_FOLDLEVELNUMBERMASK)) {
                    lev |= SC_FOLDLEVELHEADERFLAG;
                }
            }
        }
        indentCurrent = indentNext;
        styler.SetLevel(lineCurrent, lev);
        lineCurrent++;
    }
}

// endregion
