//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
// ReSharper disable CppParameterMayBeConstPtrOrRef
#include "Editor.hpp"
#include "CodeTransformer.hpp"
#include "analyses/symbols/SymbolTable.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/Theme.hpp"
#include "config/ThemeCategory.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "document/DocumentType.hpp"
#include "lexilla/FBSciLexer.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {
// Order matches Scintilla margin indices — Changes sits at the right,
// directly against the text edge, so the diff bar is the first thing
// the eye picks up next to the line content.
enum class Margins : std::uint8_t {
    LineNumbers = 0,
    Fold = 1,
    Changes = 2
};
constexpr auto operator+(const Margins& rhs) -> int {
    return static_cast<int>(rhs);
}

// Marker numbers come from `Editor::kAddedMarker` / `kModifiedMarker`
// (public so tests can query). Scintilla reserves the upper range
// (25–31) for fold markers via `wxSTC_MASK_FOLDERS`, so the low-range
// picks don't collide.
constexpr int kChangeMarkersMask
    = (1 << Editor::kAddedMarker) | (1 << Editor::kModifiedMarker);

struct Constants final {
    static constexpr int edgeColumn = 80;
    static constexpr int foldMarginWidth = 16;
    static constexpr int changeMarginWidth = 5;
    static constexpr int analysesThrottle = 500;
};

auto isBrace(const int ch) -> bool {
    return ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}';
}
} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(Editor, wxStyledTextCtrl)
    EVT_STC_MARGINCLICK (wxID_ANY,    Editor::onMarginClick)
    EVT_STC_MODIFIED(wxID_ANY,        Editor::onModified)
    EVT_STC_SAVEPOINTREACHED(wxID_ANY,Editor::onSavePointReached)
    EVT_STC_UPDATEUI(wxID_ANY,        Editor::onUpdateUI)
    EVT_STC_ZOOM(wxID_ANY,            Editor::onZoom)
    EVT_STC_CHARADDED(wxID_ANY,       Editor::onCharAdded)
    EVT_STC_HOTSPOT_CLICK(wxID_ANY,   Editor::onHotSpotClick)
    EVT_TIMER(wxID_ANY,               Editor::onIntellisenseTimer)
    EVT_KEY_DOWN(Editor::onKeyDown)
    EVT_KEY_UP(Editor::onKeyUp)
    EVT_KILL_FOCUS(Editor::onKillFocus)
    EVT_SET_FOCUS(Editor::onFocus)
wxEND_EVENT_TABLE()
// clang-format on

Editor::Editor(
    wxWindow* parent,
    ConfigManager& configManager,
    Theme& theme,
    DocumentManager* documentManager,
    UIManager* uiManager,
    CodeTransformer* transformer,
    const DocumentType type,
    const bool preview
)
: wxStyledTextCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
, m_configManager(configManager)
, m_theme(theme)
, m_documentManager(documentManager)
, m_uiManager(uiManager)
, m_transformer(transformer)
, m_docType(type)
, m_preview(preview) {
    applySettings();
    m_intellisenseTimer.SetOwner(this);

    if (!m_preview && m_changeTracking) {
        resnapshotChangeTracker();
    }
}

void Editor::applySettings() {
    StyleResetDefault();

    loadLexer();
    applyEditorSettings();
    applyTheme();
    defineFoldMargins();
    defineChangesMargin();
    updateLineNumberMarginWidth();

    Colourise(0, -1);
    Refresh();
}

void Editor::applyEditorSettings() {
    const auto& editor = m_configManager.config().at("editor");
    const auto tabSize = editor.get_or("tabSize", 4);
    if (m_transformer != nullptr) {
        m_transformer->applySettings();
    }
    m_changeTracking = editor.get_or("changeTracking", true);

    UsePopUp(wxSTC_POPUP_TEXT);
    SetTabWidth(tabSize);
    SetUseTabs(false);
    SetTabIndents(true);
    SetBackSpaceUnIndents(true);
    SetIndent(tabSize);

    SetScrollWidth(1);
    SetScrollWidthTracking(true);
    // EOL mode is applied per-document — see Document ctor + setEolMode.

    if (m_preview) {
        // Preview mode: hide all margins and decorations
        SetMarginWidth(+Margins::LineNumbers, 0);
        SetMarginWidth(+Margins::Changes, 0);
        SetMarginWidth(+Margins::Fold, 0);
        SetEdgeMode(wxSTC_EDGE_NONE);
        SetViewEOL(false);
        SetIndentationGuides(wxSTC_IV_NONE);
        SetViewWhiteSpace(wxSTC_WS_INVISIBLE);
        // Prevent horizontal scrollbar flashing on content changes
        SetScrollWidthTracking(false);
        return;
    }

    SetEdgeColumn(editor.get_or("edgeColumn", Constants::edgeColumn));
    SetViewEOL(editor.get_or("displayEOL", false));
    SetIndentationGuides(editor.get_or("indentGuide", false) ? wxSTC_IV_LOOKFORWARD : wxSTC_IV_NONE);
    SetEdgeMode(editor.get_or("longLine", false) ? wxSTC_EDGE_LINE : wxSTC_EDGE_NONE);
    SetViewWhiteSpace(editor.get_or("whiteSpace", false) ? wxSTC_WS_VISIBLEALWAYS : wxSTC_WS_INVISIBLE);

    // Line number margin
    SetMarginCount(3);
    SetMarginWidth(+Margins::LineNumbers, 0);
    SetMarginWidth(+Margins::Changes, 0);
    SetMarginWidth(+Margins::Fold, 0);
}

void Editor::defineFoldMargins() {
    if (m_preview) {
        return;
    }

    const auto& editor = m_configManager.config().at("editor");
    if (m_docType == DocumentType::Text || not editor.get_or("folderMargin", false)) {
        SetProperty("fold", "0");
        return;
    }

    SetMarginType(+Margins::Fold, wxSTC_MARGIN_SYMBOL);
    SetMarginMask(+Margins::Fold, static_cast<int>(wxSTC_MASK_FOLDERS));
    SetMarginWidth(+Margins::Fold, Constants::foldMarginWidth);
    SetMarginSensitive(+Margins::Fold, true);

    const auto& theme = m_theme;
    const auto foldFg = theme.foreground(theme.getFoldMargin().foreground);
    const auto foldBg = theme.background(theme.getFoldMargin().background);

    SetFoldFlags(wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);
    MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_BOXPLUSCONNECTED, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_BOXMINUSCONNECTED, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_VLINE, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_LCORNER, foldBg, foldFg);

    SetMarginBackground(+Margins::Fold, foldBg);
    SetFoldMarginColour(true, foldBg);
    SetFoldMarginHiColour(true, foldBg);
    SetProperty("fold", "1");
}

void Editor::defineChangesMargin() {
    if (m_preview) {
        return;
    }

    // Disabled via `editor.changeTracking` — hide the margin entirely
    // and drop any leftover markers from a prior session. The other
    // handlers also short-circuit on `m_changeTracking == false`, so
    // no per-modify work runs while it's off.
    if (!m_changeTracking) {
        SetMarginWidth(+Margins::Changes, 0);
        MarkerDeleteAll(kAddedMarker);
        MarkerDeleteAll(kModifiedMarker);
        return;
    }

    // Symbol margin with only the two change markers visible. The mask
    // keeps fold and other markers from leaking onto this strip.
    SetMarginType(+Margins::Changes, wxSTC_MARGIN_SYMBOL);
    SetMarginMask(+Margins::Changes, kChangeMarkersMask);
    SetMarginWidth(+Margins::Changes, Constants::changeMarginWidth);
    SetMarginSensitive(+Margins::Changes, false);

    // Full-rectangle markers fill the margin cell — the VS Code / JetBrains
    // change-bar look. Colours come from the theme; when the loaded theme
    // doesn't define them (legacy files) we fall back to the diff palette
    // the default theme ships with — see `Theme::loadDefaults`.
    const wxColour added = m_theme.getChangesAdded();
    const wxColour modified = m_theme.getChangesModified();
    MarkerDefine(kAddedMarker, wxSTC_MARK_FULLRECT, added, added);
    MarkerDefine(kModifiedMarker, wxSTC_MARK_FULLRECT, modified, modified);

    // `ChangesBackground` is seeded from the fold-margin background at
    // load time (see `Theme::seedChangesPaletteDefaults`), so by the
    // time we read it here it always carries a concrete colour — no
    // runtime fallback chain.
    SetMarginBackground(+Margins::Changes, m_theme.getChangesBackground());
}

void Editor::applyTheme() {
    const auto& theme = m_theme;
    const auto& defaultEntry = theme.get(ThemeCategory::Default);
    const auto& [foreground, background] = defaultEntry.colors;

    m_font = theme.getResolvedFont();

    StyleSetForeground(wxSTC_STYLE_DEFAULT, foreground);
    StyleSetBackground(wxSTC_STYLE_DEFAULT, background);
    StyleSetFont(wxSTC_STYLE_DEFAULT, m_font);

    // Propagate the themed STYLE_DEFAULT to every other style index. Any
    // index not overridden below (control-char, calltip, fold-display, any
    // style a future lexer might add, and — critically — style 0 used by
    // LEX_NULL for plain text) inherits sensible theme defaults.
    StyleClearAll();

    StyleSetBackground(wxSTC_STYLE_INDENTGUIDE, background);
    StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, theme.getSeparator());

    // Line numbers
    applyColors(wxSTC_STYLE_LINENUMBER, theme.getLineNumber(), theme);
    StyleSetFont(wxSTC_STYLE_LINENUMBER, m_font);

    // Caret — no dedicated field, use default foreground.
    SetCaretForeground(foreground);

    // Selection
    const auto& sel = theme.getSelection();
    SetSelForeground(true, theme.foreground(sel.foreground));
    SetSelBackground(true, theme.background(sel.background));

    // Brace matching
    applyStyle(wxSTC_STYLE_BRACELIGHT, theme.getBrace(), theme);
    applyStyle(wxSTC_STYLE_BRACEBAD, theme.getBadBrace(), theme);

    // separator lines
    SetEdgeColour(theme.foreground(theme.getSeparator()));

    loadLexerTheme();
}

void Editor::applyStyle(const int stcId, const Theme::Entry& style, const Theme& theme) {
    applyColors(stcId, style.colors, theme);
    StyleSetFont(stcId, m_font);
    StyleSetBold(stcId, style.bold);
    StyleSetItalic(stcId, style.italic);
    StyleSetUnderline(stcId, style.underlined);
}

void Editor::applyColors(const int stcId, const Theme::Colors& colors, const Theme& theme) {
    StyleSetForeground(stcId, theme.foreground(colors.foreground));
    StyleSetBackground(stcId, theme.background(colors.background));
}

void Editor::loadLexer() {
    if (m_configManager.config().get_or("editor.syntaxHighlight", true)) {
        switch (m_docType) {
        case DocumentType::FreeBASIC:
            SetILexer(FBSciLexer::Create());
            break;
        case DocumentType::HTML:
            SetLexer(wxSTC_LEX_HTML);
            break;
        case DocumentType::Properties:
            SetLexer(wxSTC_LEX_PROPERTIES);
            break;
        case DocumentType::Markdown:
            SetLexer(wxSTC_LEX_MARKDOWN);
            break;
        case DocumentType::Batch:
            SetLexer(wxSTC_LEX_BATCH);
            break;
        case DocumentType::Bash:
            SetLexer(wxSTC_LEX_BASH);
            break;
        case DocumentType::Makefile:
            SetLexer(wxSTC_LEX_MAKEFILE);
            break;
        case DocumentType::Json:
            SetLexer(wxSTC_LEX_JSON);
            break;
        case DocumentType::Css:
            SetLexer(wxSTC_LEX_CSS);
            break;
        case DocumentType::Text:
            SetLexer(wxSTC_LEX_NULL);
            break;
        }
    } else {
        SetLexer(wxSTC_LEX_NULL);
    }
}

void Editor::loadLexerTheme() {
    if (m_configManager.config().get_or("editor.syntaxHighlight", true)) {
        switch (m_docType) {
        case DocumentType::FreeBASIC:
            applyFreebasicTheme();
            break;
        case DocumentType::HTML:
            applyHtmlTheme();
            break;
        case DocumentType::Properties:
            applyPropertiesTheme();
            break;
        case DocumentType::Markdown:
            applyMarkdownTheme();
            break;
        case DocumentType::Batch:
            applyBatchTheme();
            break;
        case DocumentType::Bash:
            applyBashTheme();
            break;
        case DocumentType::Makefile:
            applyMakefileTheme();
            break;
        case DocumentType::Json:
            applyJsonTheme();
            break;
        case DocumentType::Css:
            applyCssTheme();
            break;
        case DocumentType::Text:
            applyTextTheme();
            break;
        }
    } else {
        applyTextTheme();
    }
}

void Editor::applyFreebasicTheme() {
    const auto& theme = m_theme;

    // Keywords are classified from FBSciLexer's shared table (built at startup
    // and on settings change via lexer::setFbKeywords) — no per-editor wordlists.
    for (const auto cat : kThemeCategories) {
        applyStyle(+cat, theme.get(cat), theme);
    }
}

void Editor::applyHtmlTheme() {
    const auto& theme = m_theme;
    applyStyle(wxSTC_H_DEFAULT, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_H_TAG, theme.get(ThemeCategory::Keywords), theme);
    applyStyle(wxSTC_H_TAGUNKNOWN, theme.get(ThemeCategory::Keywords), theme);
    applyStyle(wxSTC_H_ATTRIBUTE, theme.get(ThemeCategory::KeywordTypes), theme);
    applyStyle(wxSTC_H_ATTRIBUTEUNKNOWN, theme.get(ThemeCategory::KeywordTypes), theme);
    applyStyle(wxSTC_H_NUMBER, theme.get(ThemeCategory::Number), theme);
    applyStyle(wxSTC_H_SINGLESTRING, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_H_DOUBLESTRING, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_H_OTHER, theme.get(ThemeCategory::KeywordConstants), theme);
    applyStyle(wxSTC_H_COMMENT, theme.get(ThemeCategory::Comment), theme);
    applyStyle(wxSTC_H_ENTITY, theme.get(ThemeCategory::KeywordOperators), theme);
}

void Editor::applyPropertiesTheme() {
    const auto& theme = m_theme;
    applyStyle(wxSTC_PROPS_DEFAULT, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_PROPS_COMMENT, theme.get(ThemeCategory::Comment), theme);
    applyStyle(wxSTC_PROPS_SECTION, theme.get(ThemeCategory::Preprocessor), theme);
    applyStyle(wxSTC_PROPS_ASSIGNMENT, theme.get(ThemeCategory::Operator), theme);
    applyStyle(wxSTC_PROPS_DEFVAL, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_PROPS_KEY, theme.get(ThemeCategory::Keywords), theme);
}

void Editor::applyMarkdownTheme() {
    const auto& theme = m_theme;
    applyStyle(wxSTC_MARKDOWN_DEFAULT, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_MARKDOWN_LINE_BEGIN, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_MARKDOWN_STRONG1, theme.get(ThemeCategory::Keywords), theme);
    applyStyle(wxSTC_MARKDOWN_STRONG2, theme.get(ThemeCategory::Keywords), theme);
    applyStyle(wxSTC_MARKDOWN_EM1, theme.get(ThemeCategory::KeywordTypes), theme);
    applyStyle(wxSTC_MARKDOWN_EM2, theme.get(ThemeCategory::KeywordTypes), theme);
    applyStyle(wxSTC_MARKDOWN_HEADER1, theme.get(ThemeCategory::KeywordPP), theme);
    applyStyle(wxSTC_MARKDOWN_HEADER2, theme.get(ThemeCategory::KeywordPP), theme);
    applyStyle(wxSTC_MARKDOWN_HEADER3, theme.get(ThemeCategory::IdentifierPP), theme);
    applyStyle(wxSTC_MARKDOWN_HEADER4, theme.get(ThemeCategory::IdentifierPP), theme);
    applyStyle(wxSTC_MARKDOWN_HEADER5, theme.get(ThemeCategory::IdentifierPP), theme);
    applyStyle(wxSTC_MARKDOWN_HEADER6, theme.get(ThemeCategory::IdentifierPP), theme);
    applyStyle(wxSTC_MARKDOWN_PRECHAR, theme.get(ThemeCategory::Operator), theme);
    applyStyle(wxSTC_MARKDOWN_ULIST_ITEM, theme.get(ThemeCategory::Operator), theme);
    applyStyle(wxSTC_MARKDOWN_OLIST_ITEM, theme.get(ThemeCategory::Operator), theme);
    applyStyle(wxSTC_MARKDOWN_BLOCKQUOTE, theme.get(ThemeCategory::Comment), theme);
    applyStyle(wxSTC_MARKDOWN_STRIKEOUT, theme.get(ThemeCategory::Comment), theme);
    applyStyle(wxSTC_MARKDOWN_HRULE, theme.get(ThemeCategory::Preprocessor), theme);
    applyStyle(wxSTC_MARKDOWN_LINK, theme.get(ThemeCategory::KeywordConstants), theme);
    applyStyle(wxSTC_MARKDOWN_CODE, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_MARKDOWN_CODE2, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_MARKDOWN_CODEBK, theme.get(ThemeCategory::String), theme);
}

void Editor::applyBatchTheme() {
    const auto& theme = m_theme;

    // Keyword lists live in keywords.ini under [batch]:
    //   words    → SCE_BAT_WORD (flow keywords)
    //   commands → SCE_BAT_COMMAND (built-in / external commands)
    // The lexer matches case-insensitively.
    const auto& batch = m_configManager.keywords().at("batch");
    SetKeyWords(0, batch.get_or("words", ""));
    SetKeyWords(1, batch.get_or("commands", ""));

    applyStyle(wxSTC_BAT_DEFAULT, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_BAT_COMMENT, theme.get(ThemeCategory::Comment), theme);
    applyStyle(wxSTC_BAT_WORD, theme.get(ThemeCategory::Keywords), theme);
    applyStyle(wxSTC_BAT_LABEL, theme.get(ThemeCategory::Label), theme);
    applyStyle(wxSTC_BAT_HIDE, theme.get(ThemeCategory::KeywordOperators), theme);
    applyStyle(wxSTC_BAT_COMMAND, theme.get(ThemeCategory::KeywordTypes), theme);
    applyStyle(wxSTC_BAT_IDENTIFIER, theme.get(ThemeCategory::Identifier), theme);
    applyStyle(wxSTC_BAT_OPERATOR, theme.get(ThemeCategory::Operator), theme);
}

void Editor::applyBashTheme() {
    const auto& theme = m_theme;

    // Keyword list lives in keywords.ini under [bash]:
    //   words → SCE_SH_WORD (reserved words + common built-ins)
    const auto& bash = m_configManager.keywords().at("bash");
    SetKeyWords(0, bash.get_or("words", ""));

    applyStyle(wxSTC_SH_DEFAULT, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_SH_ERROR, theme.get(ThemeCategory::Error), theme);
    applyStyle(wxSTC_SH_COMMENTLINE, theme.get(ThemeCategory::Comment), theme);
    applyStyle(wxSTC_SH_NUMBER, theme.get(ThemeCategory::Number), theme);
    applyStyle(wxSTC_SH_WORD, theme.get(ThemeCategory::Keywords), theme);
    applyStyle(wxSTC_SH_STRING, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_SH_CHARACTER, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_SH_OPERATOR, theme.get(ThemeCategory::Operator), theme);
    applyStyle(wxSTC_SH_IDENTIFIER, theme.get(ThemeCategory::Identifier), theme);
    applyStyle(wxSTC_SH_SCALAR, theme.get(ThemeCategory::Identifier), theme);
    applyStyle(wxSTC_SH_PARAM, theme.get(ThemeCategory::Identifier), theme);
    applyStyle(wxSTC_SH_BACKTICKS, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_SH_HERE_DELIM, theme.get(ThemeCategory::Preprocessor), theme);
    applyStyle(wxSTC_SH_HERE_Q, theme.get(ThemeCategory::String), theme);
}

void Editor::applyMakefileTheme() {
    const auto& theme = m_theme;
    applyStyle(wxSTC_MAKE_DEFAULT, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_MAKE_COMMENT, theme.get(ThemeCategory::Comment), theme);
    applyStyle(wxSTC_MAKE_PREPROCESSOR, theme.get(ThemeCategory::Preprocessor), theme);
    applyStyle(wxSTC_MAKE_IDENTIFIER, theme.get(ThemeCategory::Identifier), theme);
    applyStyle(wxSTC_MAKE_OPERATOR, theme.get(ThemeCategory::Operator), theme);
    applyStyle(wxSTC_MAKE_TARGET, theme.get(ThemeCategory::Label), theme);
    applyStyle(wxSTC_MAKE_IDEOL, theme.get(ThemeCategory::Error), theme);
}

void Editor::applyJsonTheme() {
    const auto& theme = m_theme;

    // Keyword lists live in keywords.ini under [json]:
    //   keywords   → SCE_JSON_KEYWORD (true / false / null)
    //   ldkeywords → SCE_JSON_LDKEYWORD (JSON-LD `@id`, `@context`, ...)
    const auto& json = m_configManager.keywords().at("json");
    SetKeyWords(0, json.get_or("keywords", ""));
    SetKeyWords(1, json.get_or("ldkeywords", ""));

    applyStyle(wxSTC_JSON_DEFAULT, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_JSON_NUMBER, theme.get(ThemeCategory::Number), theme);
    applyStyle(wxSTC_JSON_STRING, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_JSON_STRINGEOL, theme.get(ThemeCategory::Error), theme);
    applyStyle(wxSTC_JSON_PROPERTYNAME, theme.get(ThemeCategory::KeywordTypes), theme);
    applyStyle(wxSTC_JSON_ESCAPESEQUENCE, theme.get(ThemeCategory::KeywordOperators), theme);
    applyStyle(wxSTC_JSON_LINECOMMENT, theme.get(ThemeCategory::Comment), theme);
    applyStyle(wxSTC_JSON_BLOCKCOMMENT, theme.get(ThemeCategory::MultilineComment), theme);
    applyStyle(wxSTC_JSON_OPERATOR, theme.get(ThemeCategory::Operator), theme);
    applyStyle(wxSTC_JSON_URI, theme.get(ThemeCategory::KeywordConstants), theme);
    applyStyle(wxSTC_JSON_COMPACTIRI, theme.get(ThemeCategory::KeywordConstants), theme);
    applyStyle(wxSTC_JSON_KEYWORD, theme.get(ThemeCategory::Keywords), theme);
    applyStyle(wxSTC_JSON_LDKEYWORD, theme.get(ThemeCategory::KeywordPP), theme);
    applyStyle(wxSTC_JSON_ERROR, theme.get(ThemeCategory::Error), theme);
}

void Editor::applyCssTheme() {
    const auto& theme = m_theme;

    // Keyword lists live in keywords.ini under [css]. The Scintilla CSS
    // lexer uses up to 8 lists; we populate the three that matter for a
    // basic setup — properties, pseudo-classes, pseudo-elements. The rest
    // stay empty so unknown identifiers fall to UNKNOWN_IDENTIFIER (Error).
    const auto& css = m_configManager.keywords().at("css");
    SetKeyWords(0, css.get_or("properties", ""));
    SetKeyWords(1, css.get_or("pseudoclasses", ""));
    SetKeyWords(3, css.get_or("properties", ""));     // CSS3 properties — share the list
    SetKeyWords(4, css.get_or("pseudoelements", "")); // CSS2 single-colon ::-style
    SetKeyWords(7, css.get_or("pseudoelements", "")); // CSS3 double-colon ::before

    applyStyle(wxSTC_CSS_DEFAULT, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_CSS_TAG, theme.get(ThemeCategory::Identifier), theme);
    applyStyle(wxSTC_CSS_CLASS, theme.get(ThemeCategory::KeywordTypes), theme);
    applyStyle(wxSTC_CSS_ID, theme.get(ThemeCategory::KeywordConstants), theme);
    applyStyle(wxSTC_CSS_PSEUDOCLASS, theme.get(ThemeCategory::Label), theme);
    applyStyle(wxSTC_CSS_EXTENDED_PSEUDOCLASS, theme.get(ThemeCategory::Label), theme);
    applyStyle(wxSTC_CSS_PSEUDOELEMENT, theme.get(ThemeCategory::Label), theme);
    applyStyle(wxSTC_CSS_EXTENDED_PSEUDOELEMENT, theme.get(ThemeCategory::Label), theme);
    applyStyle(wxSTC_CSS_UNKNOWN_PSEUDOCLASS, theme.get(ThemeCategory::Error), theme);
    applyStyle(wxSTC_CSS_OPERATOR, theme.get(ThemeCategory::Operator), theme);
    applyStyle(wxSTC_CSS_IDENTIFIER, theme.get(ThemeCategory::Keywords), theme);
    applyStyle(wxSTC_CSS_IDENTIFIER2, theme.get(ThemeCategory::Keywords), theme);
    applyStyle(wxSTC_CSS_IDENTIFIER3, theme.get(ThemeCategory::Keywords), theme);
    applyStyle(wxSTC_CSS_EXTENDED_IDENTIFIER, theme.get(ThemeCategory::Keywords), theme);
    applyStyle(wxSTC_CSS_UNKNOWN_IDENTIFIER, theme.get(ThemeCategory::Identifier), theme);
    applyStyle(wxSTC_CSS_VALUE, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_CSS_COMMENT, theme.get(ThemeCategory::MultilineComment), theme);
    applyStyle(wxSTC_CSS_IMPORTANT, theme.get(ThemeCategory::KeywordPP), theme);
    applyStyle(wxSTC_CSS_DIRECTIVE, theme.get(ThemeCategory::Preprocessor), theme);
    applyStyle(wxSTC_CSS_DOUBLESTRING, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_CSS_SINGLESTRING, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_CSS_ATTRIBUTE, theme.get(ThemeCategory::KeywordOperators), theme);
    applyStyle(wxSTC_CSS_GROUP_RULE, theme.get(ThemeCategory::Preprocessor), theme);
    applyStyle(wxSTC_CSS_VARIABLE, theme.get(ThemeCategory::Identifier), theme);
}

void Editor::applyTextTheme() {
    applyStyle(0, m_theme.get(ThemeCategory::Default), m_theme);
}

void Editor::updateLineNumberMarginWidth() {
    if (m_configManager.config().get_or("editor.lineNumbers", true)) {
        const auto lineNrWidth = TextWidth(wxSTC_STYLE_LINENUMBER, "00001");
        SetMarginWidth(+Margins::LineNumbers, lineNrWidth);
    } else {
        SetMarginWidth(+Margins::LineNumbers, 0);
    }
}

void Editor::setDocType(const DocumentType type) {
    m_docType = type;
    applySettings();
    updateStatusBar();
    updateDocumentState();
}

void Editor::selectLine() {
    const auto line = GetCurrentLine();
    SetSelection(PositionFromLine(line), PositionFromLine(line + 1));
}

auto Editor::getWordAtCursor() -> wxString {
    if (const auto sel = GetSelectedText(); !sel.empty()) {
        return sel;
    }
    const auto pos = GetCurrentPos();
    const auto start = WordStartPosition(pos, true);
    const auto end = WordEndPosition(pos, true);
    if (start < end) {
        return GetTextRange(start, end);
    }
    return {};
}

auto Editor::getKeywordAtCursor() const -> wxString {
    // wxSTC's position / word boundary helpers aren't const, so cast away
    // const for the duration of this read-only query.
    auto* self = const_cast<Editor*>(this);
    const auto pos = self->GetCurrentPos();
    const auto start = self->WordStartPosition(pos, true);
    const auto end = self->WordEndPosition(pos, true);
    auto keyword = self->GetTextRange(start, end).Strip(wxString::both);
    keyword.MakeLower();

    if (keyword.empty()) {
        return {};
    }

    // Include '#' for preprocessor directives like #IFDEF
    if (m_docType == DocumentType::FreeBASIC && start > 0 && self->GetCharAt(start - 1) == '#') {
        keyword = "#" + keyword;
    }

    return keyword;
}

auto Editor::findNext(const wxString& text, const int flags, const bool forward) -> bool {
    if (text.empty()) {
        return false;
    }

    // Convert wxFindReplaceDialog flags to wxSTC search flags
    int stcFlags = 0;
    if ((flags & wxFR_WHOLEWORD) != 0) {
        stcFlags |= wxSTC_FIND_WHOLEWORD;
    }
    if ((flags & wxFR_MATCHCASE) != 0) {
        stcFlags |= wxSTC_FIND_MATCHCASE;
    }

    if (forward) {
        SetTargetStart(GetCurrentPos());
        SetTargetEnd(GetLength());
    } else {
        // Search backward: from before current selection to start of document
        SetTargetStart(GetCurrentPos() - 1);
        SetTargetEnd(0);
    }
    SetSearchFlags(stcFlags);

    if (SearchInTarget(text) != -1) {
        SetSelection(GetTargetStart(), GetTargetEnd());
        EnsureCaretVisible();
        return true;
    }

    // Wrap around
    if (forward) {
        SetTargetStart(0);
        SetTargetEnd(GetCurrentPos());
    } else {
        SetTargetStart(GetLength());
        SetTargetEnd(GetCurrentPos());
    }
    SetSearchFlags(stcFlags);

    if (SearchInTarget(text) != -1) {
        SetSelection(GetTargetStart(), GetTargetEnd());
        EnsureCaretVisible();
        return true;
    }

    return false;
}

auto Editor::replaceNext(const wxString& findText, const wxString& replaceText, const int flags) -> bool {
    if (findText.empty()) {
        return false;
    }

    // Check if current selection matches the find text
    const auto selected = GetSelectedText();
    bool matches = false;
    if ((flags & wxFR_MATCHCASE) != 0) {
        matches = (selected == findText);
    } else {
        matches = (selected.Lower() == findText.Lower());
    }

    if (matches) {
        ReplaceSelection(replaceText);
    }

    return findNext(findText, flags, true);
}

auto Editor::replaceAll(const wxString& findText, const wxString& replaceText, const int flags) -> int {
    if (findText.empty()) {
        return 0;
    }

    int stcFlags = 0;
    if ((flags & wxFR_WHOLEWORD) != 0) {
        stcFlags |= wxSTC_FIND_WHOLEWORD;
    }
    if ((flags & wxFR_MATCHCASE) != 0) {
        stcFlags |= wxSTC_FIND_MATCHCASE;
    }

    int count = 0;
    BeginUndoAction();

    SetTargetStart(0);
    SetTargetEnd(GetLength());
    SetSearchFlags(stcFlags);

    while (SearchInTarget(findText) != -1) {
        ReplaceTarget(replaceText);
        count++;
        SetTargetStart(GetTargetEnd());
        SetTargetEnd(GetLength());
    }

    EndUndoAction();
    return count;
}

void Editor::gotoLine(const wxString& input) {
    if (input.empty()) {
        return;
    }

    const auto colonPos = input.Find(':');
    const auto linePart = colonPos != wxNOT_FOUND ? input.Left(static_cast<size_t>(colonPos)) : input;
    const auto colPart = colonPos != wxNOT_FOUND ? input.Mid(static_cast<size_t>(colonPos) + 1) : wxString {};

    // Parse line — "e" means last line, otherwise 1-based number
    int line = 0;
    if (linePart.Lower() == "e") {
        line = GetLineCount() - 1;
    } else {
        long val = 0;
        if (!linePart.ToLong(&val) || val < 1) {
            return;
        }
        line = static_cast<int>(val) - 1;
    }

    GotoLine(line);

    // Parse column if present
    if (!colPart.empty()) {
        int col = 0;
        if (colPart.Lower() == "e") {
            col = GetLineEndPosition(line) - PositionFromLine(line);
        } else {
            long val = 0;
            if (colPart.ToLong(&val) && val >= 1) {
                col = static_cast<int>(val) - 1;
            }
        }
        GotoPos(PositionFromLine(line) + col);
    }

    EnsureCaretVisible();
}

void Editor::navigateToLine(const int line) {
    const int target = line - 1;
    if (GetCurrentLine() != target) {
        ScrollToLine(target - (LinesOnScreen() / 2));
        GotoLine(target);
    }
    SetFocus();
    EnsureCaretVisible();
}

void Editor::commentSelection() {
    const auto lineStart = LineFromPosition(GetSelectionStart());
    const auto lineEnd = LineFromPosition(GetSelectionEnd());

    BeginUndoAction();
    for (auto line = lineStart; line <= lineEnd; line++) {
        InsertText(PositionFromLine(line), "'");
    }
    EndUndoAction();

    SetSelection(PositionFromLine(lineStart), GetLineEndPosition(lineEnd));
}

void Editor::uncommentSelection() {
    const auto lineStart = LineFromPosition(GetSelectionStart());
    const auto lineEnd = LineFromPosition(GetSelectionEnd());

    BeginUndoAction();
    for (auto line = lineStart; line <= lineEnd; line++) {
        const auto indent = GetLineIndentation(line);
        const auto pos = PositionFromLine(line) + indent;
        const auto text = GetLine(line).Trim(false).Lower();

        if (text.StartsWith("rem ") || text.StartsWith("rem\t")) {
            SetTargetStart(pos);
            SetTargetEnd(pos + 3);
            ReplaceTarget("");
        } else if (text.StartsWith("'")) {
            SetTargetStart(pos);
            SetTargetEnd(pos + 1);
            ReplaceTarget("");
        }
    }
    EndUndoAction();

    SetSelection(PositionFromLine(lineStart), GetLineEndPosition(lineEnd));
}

void Editor::onUpdateUI(wxStyledTextEvent& event) {
    event.Skip();

    if (m_editorLocked) {
        return;
    }

    const bool selection = (event.GetUpdated() & wxSTC_UPDATE_SELECTION) != 0;
    const bool content = (event.GetUpdated() & wxSTC_UPDATE_CONTENT) != 0;

    if (not m_callPostUpdate && (selection || content)) {
        m_callPostUpdate = true;
        CallAfter(&Editor::postUpdateUI);
    }

    if (m_docType != DocumentType::FreeBASIC) {
        return;
    }

    const int curr = GetCurrentPos();
    if (selection) {
        if (m_transformer != nullptr && curr != m_lastCaretPos) {
            m_editorLocked = true;
            m_transformer->onCaretMoved(*this, m_lastCaretPos);
            m_editorLocked = false;
        }
    }
    m_lastCaretPos = curr;
}

void Editor::postUpdateUI() {
    updateStatusBar();
    updateBraceMatch();
    if (m_documentManager != nullptr) {
        m_documentManager->syncEditCommands();
        // Refresh the tab's `[*]` dirty marker. Coalesced here — a bulk
        // edit fires EVT_STC_MODIFIED per line, but the dirty state (and
        // thus the title) changes at most once per burst.
        m_documentManager->updateActiveTabTitle();
    }
    m_callPostUpdate = false;
}

void Editor::onCharAdded(wxStyledTextEvent& event) {
    if (m_docType != DocumentType::FreeBASIC || m_editorLocked || m_transformer == nullptr) {
        return;
    }
    m_editorLocked = true;
    m_transformer->onCharAdded(*this, event.GetKey());
    m_editorLocked = false;
    m_insertHandled = true;
}

void Editor::onZoom(wxStyledTextEvent& /*event*/) {
    updateLineNumberMarginWidth();
}

void Editor::updateBraceMatch() {
    if (!m_configManager.config().get_or("editor.braceHighlight", true)) {
        return;
    }

    const auto pos = GetCurrentPos();
    const auto ch = GetCharAt(pos);

    if (isBrace(ch)) {
        if (const auto match = BraceMatch(pos); match != wxSTC_INVALID_POSITION) {
            BraceHighlight(pos, match);
        } else {
            BraceBadLight(pos);
        }
    } else {
        BraceHighlight(wxSTC_INVALID_POSITION, wxSTC_INVALID_POSITION);
    }
}

void Editor::updateStatusBar() const {
    if (m_uiManager == nullptr) {
        return;
    }
    auto& bar = m_uiManager->getStatusBar();
    const Document* doc = m_documentManager != nullptr
                            ? m_documentManager->findByEditor(this)
                            : nullptr;
    if (doc != nullptr) {
        const auto pos = GetCurrentPos();
        bar.setCursor(LineFromPosition(pos) + 1, GetColumn(pos) + 1);
        bar.setDocumentFields(*doc);
    } else {
        bar.clearDocumentFields();
    }
}

void Editor::disableTransforms(const bool state) {
    m_editorLocked = state;
}

void Editor::updateDocumentState() const {
    if (m_uiManager == nullptr) {
        return;
    }
    // Doc-bound commands (Save / Comment / Subs / Format / …) follow
    // the active document's type. Build commands follow the active
    // *project*'s capabilities — refresh both because focus changes
    // can collide with active-tab switches that didn't fire their
    // own page-change event yet.
    m_uiManager->syncDocCommands();
    m_uiManager->syncBuildCommands();
}

void Editor::onFocus(wxFocusEvent& event) {
    event.Skip();
    updateStatusBar();
    updateDocumentState();
}

void Editor::onIntellisenseTimer(wxTimerEvent& /*event*/) {
    if (m_documentManager == nullptr) {
        return;
    }
    auto* doc = m_documentManager->findByEditor(this);
    if (doc == nullptr) {
        return;
    }
    m_documentManager->submitIntellisense(doc, GetText());
}

void Editor::onKeyDown(wxKeyEvent& event) {
    event.Skip();
    if (m_docType == DocumentType::FreeBASIC && event.GetKeyCode() == WXK_CONTROL) {
        setIncludeHotspots(true);
    }
}

void Editor::onKeyUp(wxKeyEvent& event) {
    event.Skip();
    if (event.GetKeyCode() == WXK_CONTROL) {
        setIncludeHotspots(false);
    }
}

void Editor::onKillFocus(wxFocusEvent& event) {
    event.Skip();
    setIncludeHotspots(false);
}

void Editor::setIncludeHotspots(const bool active) {
    if (m_includeHotspotsActive == active) {
        return;
    }
    m_includeHotspotsActive = active;
    SetMouseDownCaptures(!active);
    StyleSetHotSpot(+ThemeCategory::StringPP, active);
}

void Editor::onHotSpotClick(wxStyledTextEvent& event) {
    if (m_docType != DocumentType::FreeBASIC || m_documentManager == nullptr) {
        return;
    }

    auto& docMgr = *m_documentManager;
    const auto* doc = docMgr.findByEditor(this);
    if (doc == nullptr) {
        return;
    }

    const auto symbols = getSymbolTable();
    if (symbols == nullptr) {
        return;
    }

    const auto line = LineFromPosition(event.GetPosition());
    const auto* inc = symbols->findIncludeAt(line);
    if (inc == nullptr) {
        return;
    }

    docMgr.openInclude(*doc, inc->path);
}

void Editor::onMarginClick(wxStyledTextEvent& event) {
    if (event.GetMargin() == +Margins::Fold) {
        const auto lineClick = LineFromPosition(event.GetPosition());
        const auto levelClick = GetFoldLevel(lineClick);
        if ((levelClick & wxSTC_FOLDLEVELHEADERFLAG) > 0) {
            ToggleFold(lineClick);
        }
    }
}

void Editor::onModified(wxStyledTextEvent& event) {
    event.Skip();

    const auto mod = event.GetModificationType();
    if ((mod & (wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT | wxSTC_PERFORMED_UNDO | wxSTC_PERFORMED_REDO)) == 0) {
        return;
    }

    // Change tracking runs for every document type that paints a margin
    // (preview panes opt out via m_preview). Done first so the markers
    // are accurate even when the rest of `onModified` returns early
    // for non-FreeBASIC docs.
    updateChangeTracking(event);

    if (m_docType != DocumentType::FreeBASIC) {
        return;
    }
    if (m_editorLocked) {
        return;
    }

    // Throttle a background reparse: restart the one-shot timer; if the
    // user keeps typing, only the final pause triggers a submit.
    m_intellisenseTimer.StartOnce(Constants::analysesThrottle);

    // Genuine text insert not handled by char-add (paste, multi-line
    // indent, ...). A bulk edit fires one EVT_STC_MODIFIED per modified
    // line — accumulate the union span and defer a *single* transformer
    // pass rather than one per event (which is O(events) relexes).
    if (m_transformer != nullptr && (mod & wxSTC_MOD_INSERTTEXT) != 0) {
        m_insertHandled = false;
        const int pos = event.GetPosition();
        const int end = pos + event.GetLength();
        if (m_pendingInsertStart < 0) {
            m_pendingInsertStart = pos;
            m_pendingInsertEnd = end;
            CallAfter(&Editor::flushPendingInsert);
        } else {
            m_pendingInsertStart = std::min(m_pendingInsertStart, pos);
            m_pendingInsertEnd = std::max(m_pendingInsertEnd, end);
        }
    }
}

void Editor::flushPendingInsert() {
    const int start = m_pendingInsertStart;
    const int end = std::min(m_pendingInsertEnd, GetLength());
    m_pendingInsertStart = -1;
    m_pendingInsertEnd = -1;

    // Char-add already transformed this insert, or the editor is locked
    // (load / reload) — nothing to do.
    if (start < 0 || end <= start || m_insertHandled || m_transformer == nullptr || m_editorLocked) {
        return;
    }

    m_editorLocked = true;
    m_transformer->onTextInserted(*this, start, end - start);
    m_editorLocked = false;
}

void Editor::onSavePointReached(wxStyledTextEvent& event) {
    event.Skip();
    if (!m_changeTracking) {
        return;
    }
    // Fires from SetSavePoint (save) and from undo back to the saved
    // state — in both cases the buffer now matches "on-disk", so the
    // change tracker re-baselines and every margin marker is dropped.
    resnapshotChangeTracker();
}

void Editor::updateChangeTracking(wxStyledTextEvent& event) {
    if (m_preview || !m_changeTracking) {
        return;
    }
    const auto mod = event.GetModificationType();
    const bool isInsert = (mod & wxSTC_MOD_INSERTTEXT) != 0;
    const bool isDelete = (mod & wxSTC_MOD_DELETETEXT) != 0;
    if (!isInsert && !isDelete) {
        return;
    }

    const int pos = event.GetPosition();
    const int line = LineFromPosition(pos);
    const int linesAdded = event.GetLinesAdded();
    // Splice at the line itself when the edit lands at column 0 — the
    // new lines (or removed lines) belong above the existing line L;
    // otherwise the edit splits / merges within L and the new / removed
    // lines belong after it.
    const bool atLineStart = (pos == PositionFromLine(line));
    const int spliceAt = atLineStart ? line : line + 1;

    if (linesAdded > 0) {
        m_lineHistory.applyInsert(spliceAt, linesAdded);
    } else if (linesAdded < 0) {
        m_lineHistory.applyDelete(spliceAt, -linesAdded);
    }

    // Sync sizes when Scintilla now has more lines than the tracker.
    // Happens when an empty-document baseline (zero lines) absorbs its
    // first in-place character: linesAdded is 0 so no splice ran above,
    // but Scintilla's line count is 1 — pad the tracker with -1
    // (Added) entries so the new line reads as Added rather than
    // sliding off the end of `m_originIndex`.
    const int liveCount = GetLineCount();
    const int trackerCount = m_lineHistory.lineCount();
    if (trackerCount < liveCount) {
        m_lineHistory.applyInsert(trackerCount, liveCount - trackerCount);
    }

    // Refresh markers on the directly-affected range. For inserts that
    // grew the document, include all newly-added lines; deletes leave
    // only the absorbing line to re-check.
    const int rangeEnd = (isInsert && linesAdded > 0) ? (line + linesAdded) : line;
    remarkChangedLines(line, rangeEnd);
}

void Editor::remarkChangedLines(const int from, const int to) {
    const int lineCount = GetLineCount();
    for (int lineNum = std::max(0, from); lineNum <= to && lineNum < lineCount; lineNum++) {
        MarkerDelete(lineNum, kAddedMarker);
        MarkerDelete(lineNum, kModifiedMarker);
        wxString text = GetLine(lineNum);
        while (!text.empty() && (text[text.length() - 1] == '\n' || text[text.length() - 1] == '\r')) {
            text.RemoveLast();
        }
        switch (m_lineHistory.stateOf(lineNum, text)) {
        case LineHistory::State::Added:
            MarkerAdd(lineNum, kAddedMarker);
            break;
        case LineHistory::State::Modified:
            MarkerAdd(lineNum, kModifiedMarker);
            break;
        case LineHistory::State::Unchanged:
            break;
        }
    }
}

void Editor::resnapshotChangeTracker() {
    std::vector<wxString> lines;
    // An empty document has no lines from the user's POV — Scintilla
    // surfaces one empty trailing line as internal bookkeeping, but
    // treating that as a real baseline would make the first typed
    // character look like a Modified line (amber) instead of the
    // expected Added line (green). Leave `lines` empty in that case
    // so the next edit reads as Added.
    if (GetTextLength() > 0) {
        const int lineCount = GetLineCount();
        lines.reserve(static_cast<std::size_t>(lineCount));
        for (int lineNum = 0; lineNum < lineCount; lineNum++) {
            wxString text = GetLine(lineNum);
            while (!text.empty() && (text[text.length() - 1] == '\n' || text[text.length() - 1] == '\r')) {
                text.RemoveLast();
            }
            lines.push_back(std::move(text));
        }
    }
    m_lineHistory.snapshot(std::move(lines));
    MarkerDeleteAll(kAddedMarker);
    MarkerDeleteAll(kModifiedMarker);
}
