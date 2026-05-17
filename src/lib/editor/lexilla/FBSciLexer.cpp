//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppDFALocalValueEscapesFunction
#include "FBSciLexer.hpp"
#include <algorithm>
#include <map>
#include "CharCategory.hpp"
// clang-format off
#include "Scintilla.h"
#include "SciLexer.h"
#include "StyleContext.h"
#include "OptionSet.h"
// clang-format on
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
    Lexilla::LexicalClass { +ThemeCategory::Keywords, "state.keyword", "keyword", "Keywords" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordTypes, "state.keyword.types", "keyword", "Types" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordOperators, "state.keyword.operators", "keyword", "Operators" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordConstants, "state.keyword.constants", "keyword", "Defines" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordLibrary, "state.library", "keyword", "FreeBASIC runtime library" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordCustom, "state.custom2", "keyword", "User keywords 2" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordPP, "state.keyword.preprocessor", "keyword", "Preprocessor keyword" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordAsm1, "state.keyword.asm1", "keyword", "Asm keywords 1" },
    Lexilla::LexicalClass { +ThemeCategory::KeywordAsm2, "state.keyword.asm2", "keyword", "Asm keywords 2" },
    Lexilla::LexicalClass { +ThemeCategory::Operator, "state.operator", "operator", "Operator" },
    Lexilla::LexicalClass { +ThemeCategory::Label, "state.label", "label", "Label" },
    Lexilla::LexicalClass { +ThemeCategory::Preprocessor, "state.preprocessor", "preprocessor", "Preprocessor" },
    Lexilla::LexicalClass { +ThemeCategory::NumberPP, "state.number.preprocessor", "literal numeric", "Number in preprocessor" },
    Lexilla::LexicalClass { +ThemeCategory::StringPP, "state.string.preprocessor", "literal string", "String in preprocessor" },
    Lexilla::LexicalClass { +ThemeCategory::OperatorPP, "state.operator.preprocessor", "operator", "Operator in preprocessor" },
    Lexilla::LexicalClass { +ThemeCategory::IdentifierPP, "state.identifier.preprocessor", "identifier", "Identifier in preprocessor" },
    Lexilla::LexicalClass { +ThemeCategory::Error, "state.error", "errpr", "Syntax error" },
};
static_assert(lexicalClasses.size() == kThemeCategoryCount);

constexpr std::array<const char*, kThemeKeywordGroupsCount + 1> wordListDescriptions {
#define GROUPS(NAME) #NAME,
    DEFINE_THEME_KEYWORD_GROUPS(GROUPS)
#undef GROUPS
        nullptr
};

struct OptionSet final : Lexilla::OptionSet<FBSciLexer::Options> {
    OptionSet() {
        DefineProperty("fold", &FBSciLexer::Options::fold);
    }
};
OptionSet kOptionSet;

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

Sci_Position FBSciLexer::PropertySet(const char* key, const char* val) {
    if (kOptionSet.PropertySet(&m_options, key, val)) {
        return 0;
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
            // Run PP first so its atLineEnd branch transitions state back to
            // Default before the bottom-of-body atLineEnd handler picks up
            // the line. Without this, `\n` (or `\r\n`) at the end of a `#`
            // directive line stays styled Preprocessor and m_inPpBody bleeds
            // into the next line's bottom-of-body re-dispatch.
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

        // Bottom-of-body re-dispatch: when the switch handler transitions
        // state to Default or Preprocessor mid-iteration, run that state's
        // handler in the SAME iteration so the current char gets processed
        // by the new state machine. Without this, the for-loop's Forward
        // would advance past the current char with only a state-change
        // recorded, never invoking the new handler on it.
        if (sc.atLineEnd) {
            lexLineEnd();
        } else if (sc.state == +Default) {
            lexDefault();
        } else if (sc.state == +Preprocessor) {
            lexPreprocessor();
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
        m_asmState = m_previousLineState.asmState;
    } else {
        m_isFirst = true;
        m_fieldAccess = false;
        // Block persists across non-continued lines. Undetermined persists
        // while we're still inside a multi-line block comment (the logical
        // line hasn't actually ended). Stmt and None reset.
        if (m_previousLineState.asmState == AsmState::Block) {
            m_asmState = AsmState::Block;
        } else if (m_previousLineState.asmState == AsmState::Undetermined
                   && m_previousLineState.commentNestLevel > 0) {
            m_asmState = AsmState::Undetermined;
        } else {
            m_asmState = AsmState::None;
        }
    }
    m_lineState.commentNestLevel = m_previousLineState.commentNestLevel;

    if (m_previousLineState.continuePP) {
        // Carry the directive-seen flag across `_` continuation and through
        // multi-line block comments so the first identifier rule survives:
        //   #  _
        //   /' comment '/  _
        //   include "x"
        // Here `include` is still the first identifier after `#`.
        m_ppDirectiveSeen = m_previousLineState.ppDirectiveSeen;
        if (m_lineState.commentNestLevel == 0) {
            m_inPpBody = true;
            m_sc->SetState(+ThemeCategory::Preprocessor);
        } else {
            m_lineState.continuePP = true;
        }
    } else {
        m_inPpBody = false;
        m_ppDirectiveSeen = false;
    }
}

void FBSciLexer::lexLineEnd() noexcept {
    // Resolve any pending asm-statement state at the end of the logical
    // line. The logical line is genuinely over only when there's no `_`
    // continuation AND we're not currently inside a multi-line comment
    // (which transparently spans physical lines without breaking the
    // statement). Resolution:
    //   Undetermined → Block (no significant content was found).
    //   Stmt         → None  (single-line statement is done).
    // Block persists; the `end asm` peek in identifyKeyword clears it.
    const bool logicalEol = !m_lineState.continueLine && m_lineState.commentNestLevel == 0;
    if (logicalEol) {
        if (m_asmState == AsmState::Undetermined) {
            m_asmState = AsmState::Block;
        } else if (m_asmState == AsmState::Stmt) {
            m_asmState = AsmState::None;
        }
    }

    m_lineState.isFirst = m_isFirst;
    m_lineState.fieldAccess = m_fieldAccess;
    m_lineState.asmState = m_asmState;
    m_lineState.ppDirectiveSeen = m_ppDirectiveSeen;
    m_styler->SetLineState(m_line, m_lineState.toInt());
}

void FBSciLexer::resetToDefault() noexcept {
    // Inside a `#`-directive body, the "default" return state is Preprocessor
    // so trailing whitespace and inter-token gaps stay PP-styled. At end of
    // the directive line the base state flips back to Default and the body
    // flag clears — the trailing newline must paint as Default so it doesn't
    // bleed PP styling into the next line.
    if (m_inPpBody) {
        if (m_sc->atLineEnd) {
            m_inPpBody = false;
            m_sc->SetState(+ThemeCategory::Default);
        } else {
            m_sc->SetState(+ThemeCategory::Preprocessor);
        }
    } else {
        m_sc->SetState(+ThemeCategory::Default);
    }
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

    // First significant token after `asm` keyword promotes to a single-line
    // statement; logical EOL while still Undetermined promotes to Block
    // (handled in lexLineEnd). Comments / continuation / whitespace do not
    // promote — they leave m_asmState alone.
    if (m_asmState == AsmState::Undetermined) {
        using enum ThemeCategory;
        switch (m_sc->state) {
        case +Identifier:
        case +Number:
        case +StringOpen:
        case +Operator:
            m_asmState = AsmState::Stmt;
            break;
        default:
            break;
        }
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
                if (m_lineState.continuePP) {
                    m_sc->ForwardSetState(+ThemeCategory::Preprocessor);
                    m_lineState.continuePP = false;
                } else {
                    m_sc->ForwardSetState(+ThemeCategory::Default); // no reset!
                }
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
            if (m_inPpBody) {
                m_sc->ChangeState(+ThemeCategory::NumberPP);
            }
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
        m_sc->ChangeState(m_inPpBody ? +ThemeCategory::StringPP : +ThemeCategory::String);
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
        return m_sc->atLineEnd;
    }

    if (m_isFirst) {
        if (m_asmState == AsmState::Block) {
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
                    m_asmState = AsmState::None;
                }
            }
        } else if (m_asmState == AsmState::None && strcmp("asm", m_identBuffer.data()) == 0) {
            // Defer the block-vs-single-line decision until logical EOL.
            // Undetermined steers wordlist selection until then; lexLineEnd
            // promotes to Block when no significant content follows, or the
            // first content token below promotes to Stmt.
            m_asmState = AsmState::Undetermined;
            m_sc->ChangeState(+ThemeCategory::Keywords);
            return true;
        }
    }

    const bool asmContext = m_asmState != AsmState::None;
    constexpr std::size_t pp = indexOfKeywordGroup(ThemeCategory::KeywordPP);
    const std::size_t first = asmContext ? pp + 1 : 0;
    const std::size_t last = asmContext ? kThemeKeywordGroupsCount : pp;

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
        if (m_inPpBody) {
            m_sc->ChangeState(+ThemeCategory::OperatorPP);
        }
        resetToDefault();
    }
}

void FBSciLexer::lexPreprocessor() noexcept {
    using enum ThemeCategory;

    m_inPpBody = true;

    if (m_sc->atLineEnd) {
        m_inPpBody = false;
        m_sc->SetState(+Default); // no reset
        return;
    }

    // continuation `_`
    if (m_sc->ch == '_'
        && not isIdentifier(m_sc->chPrev)
        && not isIdentifier(m_sc->chNext)) {
        m_lineState.continuePP = true;
        m_sc->SetState(+Comment);
        return;
    }

    // single-line comment `'`
    if (m_sc->ch == '\'') {
        m_sc->SetState(+Comment);
        return;
    }

    // nested block comment `/'`
    if (m_sc->ch == '/' && m_sc->chNext == '\'') {
        m_lineState.continuePP = true;
        m_sc->SetState(+MultilineComment);
        m_sc->Forward();
        m_lineState.commentNestLevel++;
        return;
    }

    // whitespace stays Preprocessor
    if (isSpace(m_sc->ch)) {
        if (m_sc->state != +Preprocessor) {
            m_sc->SetState(+Preprocessor);
        }
        return;
    }

    // numeric literal (decimal start) — must precede the identifier check,
    // since `isIdentifier` also accepts digits.
    if (isDigit(m_sc->ch)) {
        m_numberForm = NumberForm::Decimal;
        m_sc->SetState(+Number);
        return;
    }

    // `.` followed by digit → fractional number
    if (m_sc->ch == '.' && isDigit(m_sc->chNext)) {
        m_numberForm = NumberForm::Fraction;
        m_sc->SetState(+Number);
        return;
    }

    // identifier — accumulate, then classify against KeywordPP wordlist;
    // matched directives / modifiers paint as KeywordPP, everything else
    // paints as IdentifierPP. The internal Forward loop leaves `sc` on the
    // first non-identifier char of the body, which must still be dispatched
    // by the same iteration (the main loop will only advance past it once);
    // fall through to the digit / operator / string / etc. branches below
    // instead of returning, so chars like `(` after `LOG` are routed to
    // their proper state machine.
    if (isIdentifier(m_sc->ch)) {
        m_sc->SetState(+Preprocessor);
        while (isIdentifier(m_sc->ch) && m_sc->More()) {
            m_sc->Forward();
        }

        m_sc->GetCurrentLowered(m_identBuffer.data(), m_identBuffer.size());

        if (strcmp("rem", m_identBuffer.data()) == 0) {
            m_sc->ChangeState(+Comment);
            if (m_sc->atLineEnd) {
                m_inPpBody = false;
                m_sc->SetState(+Default);
            }
            return;
        }

        // Only the first identifier after `#` is a directive — classify
        // against the KeywordPP wordlist. Every subsequent identifier in
        // the directive's body is an IdentifierPP regardless of wordlist
        // match, so things like `#define X if`, `#define foo include`,
        // `#include once "x"` paint correctly. The `directive seen` flag
        // is preserved across `_` continuation + nested block comments
        // via LineState.
        if (!m_ppDirectiveSeen) {
            constexpr std::size_t pp = indexOfKeywordGroup(KeywordPP);
            if (m_wordLists[pp].InList(m_identBuffer.data())) {
                m_sc->ChangeState(+KeywordPP);
            } else {
                m_sc->ChangeState(+IdentifierPP);
            }
            m_ppDirectiveSeen = true;
        } else {
            m_sc->ChangeState(+IdentifierPP);
        }
        if (m_sc->atLineEnd) {
            m_inPpBody = false;
            m_sc->SetState(+Default);
            return;
        }
        m_sc->SetState(+Preprocessor);
        // fall through — current char is a non-identifier body char.
    }

    // No further work for whitespace / comments — they were already
    // handled above. Re-handle from the end of the identifier branch.
    if (m_sc->atLineEnd) {
        m_inPpBody = false;
        m_sc->SetState(+Default);
        return;
    }
    if (isSpace(m_sc->ch)) {
        if (m_sc->state != +Preprocessor) {
            m_sc->SetState(+Preprocessor);
        }
        return;
    }

    // `&H/&O/&B` numeric prefixes
    if (m_sc->ch == '&') {
        const auto lcn = fastUnsafeLowerCase(m_sc->chNext);
        if (lcn == 'h') {
            m_numberForm = NumberForm::Hexadecimal;
            m_sc->SetState(+Number);
            m_sc->Forward();
            return;
        }
        if (lcn == 'o') {
            m_numberForm = NumberForm::Octal;
            m_sc->SetState(+Number);
            m_sc->Forward();
            return;
        }
        if (lcn == 'b') {
            m_numberForm = NumberForm::Binary;
            m_sc->SetState(+Number);
            m_sc->Forward();
            return;
        }
        m_sc->SetState(+Operator);
        return;
    }

    // `!"..."` / `$"..."` — escapable / interpolated string forms
    if (m_sc->ch == '!' && m_sc->chNext == '"') {
        m_slashEscapableString = true;
        m_sc->SetState(+StringOpen);
        m_sc->Forward();
        return;
    }
    if (m_sc->ch == '$' && m_sc->chNext == '"') {
        m_sc->SetState(+StringOpen);
        m_sc->Forward();
        return;
    }

    // plain string
    if (m_sc->ch == '"') {
        m_sc->SetState(+StringOpen);
        return;
    }

    // operator / punctuation
    if (isOperator(m_sc->ch)) {
        m_sc->SetState(+Operator);
        return;
    }
}

// endregion

// region ---------- Folding ----------

namespace {

// Tab width used when measuring indentation for folding. Scintilla's default;
// the lexer cannot see the editor's tab setting through IDocument, and fold
// comparisons are relative, so a consistently-indented file folds either way.
constexpr int kFoldTabWidth = 8;

// Largest indentation a fold level number can hold without overlapping the
// fold flag bits.
constexpr int kMaxFoldIndent = SC_FOLDLEVELNUMBERMASK - SC_FOLDLEVELBASE;

// Visual indentation of `line`: the column of its first non-whitespace
// character. Returns -1 when the line is blank (empty or whitespace-only).
auto lineIndent(Lexilla::LexAccessor& styler, const Sci_Position line) -> int {
    const Sci_Position start = styler.LineStart(line);
    const Sci_Position end = styler.LineStart(line + 1);
    int indent = 0;
    for (Sci_Position pos = start; pos < end; pos++) {
        const char ch = styler.SafeGetCharAt(pos);
        if (ch == ' ') {
            indent++;
        } else if (ch == '\t') {
            indent += kFoldTabWidth - (indent % kFoldTabWidth);
        } else if (ch == '\r' || ch == '\n') {
            return -1; // end of line reached before any content
        } else {
            return std::min(indent, kMaxFoldIndent);
        }
    }
    return -1; // ran to end of document with no content
}

// `/'` nesting depth at the end of `line`, read back from the per-line state
// the lexer already persisted (LineState::commentNestLevel) — no re-lexing.
auto commentNestAtEnd(Lexilla::LexAccessor& styler, const Sci_Position line) -> int {
    return FBSciLexer::LineState::fromInt(styler.GetLineState(line)).commentNestLevel;
}

// True when the first non-whitespace character of `line` is styled as a
// multiline comment — i.e. a `/'` opens the line with only whitespace before
// it. Relies on the styling the lexer already produced.
auto lineStartsWithMlComment(Lexilla::LexAccessor& styler, const Sci_Position line) -> bool {
    const Sci_Position start = styler.LineStart(line);
    const Sci_Position end = styler.LineStart(line + 1);
    for (Sci_Position pos = start; pos < end; pos++) {
        const char ch = styler.SafeGetCharAt(pos);
        if (ch == ' ' || ch == '\t') {
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            return false; // blank line
        }
        return styler.StyleIndexAt(pos) == +ThemeCategory::MultilineComment;
    }
    return false;
}

} // namespace

void SCI_METHOD FBSciLexer::Fold(
    const Sci_PositionU startPos,
    const Sci_Position lengthDoc,
    [[maybe_unused]] int initStyle,
    Scintilla::IDocument* pAccess
) {
    if (not m_options.fold || lengthDoc == 0) {
        return;
    }

    Lexilla::LexAccessor styler(pAccess);
    const Sci_Position lineCount = styler.GetLine(styler.Length()) + 1;
    const auto start = static_cast<Sci_Position>(startPos);

    // `/'` nesting at the start of a line == nesting at the end of the line above.
    const auto nestStartOf = [&](const Sci_Position line) -> int {
        return line > 0 ? commentNestAtEnd(styler, line - 1) : 0;
    };

    // Editing a line can flip the header flag of the line above it. Begin at
    // the nearest non-blank line that also starts outside any comment, so the
    // forward pass resumes from a clean state (no comment region carried in).
    Sci_Position firstLine = styler.GetLine(start);
    while (firstLine > 0) {
        firstLine--;
        if (nestStartOf(firstLine) == 0 && lineIndent(styler, firstLine) >= 0) {
            break;
        }
    }
    const Sci_Position lastTouched = styler.GetLine(start + lengthDoc - 1);

    // Forward pass. `pending*` buffers the most recent code line until the next
    // code line reveals whether it opens an indentation fold. Multiline
    // comments fold as an exception: a `/'` alone on its line opens a region
    // whose interior is swallowed one level below the opener.
    //
    // Indent is clamped below the flag bits, so `+` composes the level number
    // and the flags exactly as `|` would, without a signed-bitwise op.
    int prevIndent = 0;
    Sci_Position pendingLine = -1;
    int pendingIndent = 0;
    bool reachedEnd = true;

    bool mlActive = false;      // inside a foldable multiline-comment region
    int mlHeaderLevel = 0;      // indentation level of that region's `/'` opener
    Sci_Position mlCloser = -1; // line carrying the matching outer `'/`

    for (Sci_Position line = firstLine; line < lineCount; line++) {
        // --- Interior / closer of an active comment region -----------------
        if (mlActive) {
            if (line < mlCloser) {
                // Swallowed comment body — fixed level, opens no sub-folds.
                const int body = std::min(mlHeaderLevel + 1, kMaxFoldIndent);
                styler.SetLevel(line, SC_FOLDLEVELBASE + body);
                if (line > lastTouched) { // nothing buffered inside a region
                    reachedEnd = false;
                    break;
                }
                continue;
            }
            // Closer line: a code line sitting at the region's structural level.
            mlActive = false;
            if (pendingLine >= 0) {
                const int header = mlHeaderLevel > pendingIndent ? SC_FOLDLEVELHEADERFLAG : 0;
                styler.SetLevel(pendingLine, SC_FOLDLEVELBASE + pendingIndent + header);
            }
            prevIndent = mlHeaderLevel;
            pendingLine = line;
            pendingIndent = mlHeaderLevel;
            if (line > lastTouched) {
                reachedEnd = false;
                break;
            }
            continue;
        }

        const int nestStart = nestStartOf(line);

        // --- Multiline-comment region opener -------------------------------
        if (nestStart == 0 && commentNestAtEnd(styler, line) > 0
            && lineStartsWithMlComment(styler, line)) {
            // Matching outer `'/`: the first later line back at nesting 0.
            // Absent (unterminated comment) → the region runs to end of file.
            Sci_Position closer = lineCount;
            for (Sci_Position scan = line + 1; scan < lineCount; scan++) {
                if (commentNestAtEnd(styler, scan) == 0) {
                    closer = scan;
                    break;
                }
            }
            // Foldable only with at least one interior line.
            if (closer >= line + 2) {
                const int level = lineIndent(styler, line); // `/'` is the content
                if (pendingLine >= 0) {
                    const int header = level > pendingIndent ? SC_FOLDLEVELHEADERFLAG : 0;
                    styler.SetLevel(pendingLine, SC_FOLDLEVELBASE + pendingIndent + header);
                }
                styler.SetLevel(line, SC_FOLDLEVELBASE + level + SC_FOLDLEVELHEADERFLAG);
                prevIndent = level;
                pendingLine = -1;
                mlActive = true;
                mlHeaderLevel = level;
                mlCloser = closer;
                if (line > lastTouched) {
                    reachedEnd = false;
                    break;
                }
                continue;
            }
            // Not foldable (no interior) — fall through, treat as a code line.
        }

        // --- Stray comment-covered line ------------------------------------
        // Begins inside a comment but is not part of a foldable region (the
        // comment's `/'` was not alone on its line). Freeze at the ambient
        // level — never a header, invisible to the indentation buffer.
        if (nestStart > 0) {
            styler.SetLevel(line, SC_FOLDLEVELBASE + prevIndent);
            continue;
        }

        // --- Plain indentation folding -------------------------------------
        const int indent = lineIndent(styler, line);
        if (indent < 0) {
            styler.SetLevel(line, SC_FOLDLEVELBASE + prevIndent + SC_FOLDLEVELWHITEFLAG);
            continue;
        }
        if (pendingLine >= 0) {
            const int header = indent > pendingIndent ? SC_FOLDLEVELHEADERFLAG : 0;
            styler.SetLevel(pendingLine, SC_FOLDLEVELBASE + pendingIndent + header);
        }
        prevIndent = indent;
        pendingLine = line;
        pendingIndent = indent;

        // Lines up to lastTouched are settled once the first code line beyond
        // it has resolved the buffered line.
        if (line > lastTouched) {
            reachedEnd = false;
            break;
        }
    }

    // End-of-document tail: the last buffered code line has no following code
    // line, so its next indent is 0 — it can never be a header.
    if (reachedEnd && pendingLine >= 0) {
        styler.SetLevel(pendingLine, SC_FOLDLEVELBASE + pendingIndent);
    }
}

// endregion
