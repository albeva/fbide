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
// Completion candidates from the Library / Constants / Preprocessor / Custom keyword groups
// (sorted, de-duplicated case-insensitively). Shared across editors; rebuilt
// when editor settings are applied.
std::vector<wxString> g_keywordCompletions;

// Sort case-insensitively (FreeBASIC is case-insensitive) and drop duplicates.
void sortUniqueCI(std::vector<wxString>& names) {
    std::ranges::sort(names, [](const wxString& lhs, const wxString& rhs) { return lhs.CmpNoCase(rhs) < 0; });
    names.erase(
        std::unique(names.begin(), names.end(),
            [](const wxString& lhs, const wxString& rhs) { return lhs.CmpNoCase(rhs) == 0; }),
        names.end()
    );
}

// Styles where a completion popup should not appear.
auto isCommentOrStringStyle(const int style) -> bool {
    switch (static_cast<ThemeCategory>(style)) {
    case ThemeCategory::Comment:
    case ThemeCategory::MultilineComment:
    case ThemeCategory::String:
    case ThemeCategory::StringOpen:
    case ThemeCategory::StringPP:
        return true;
    default:
        return false;
    }
}

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

// Indicator numbers for occurrence highlighting (identifier under the caret).
// Two indicators: a box behind the text for the background, TEXTFORE for the
// text colour. The container range is free — the FB lexer sets no decorations.
constexpr int kIndicOccurrenceBg = wxSTC_INDIC_CONTAINER;
constexpr int kIndicOccurrenceText = wxSTC_INDIC_CONTAINER + 1;
constexpr int kIndicKeywordBg = wxSTC_INDIC_CONTAINER + 2;
constexpr int kIndicKeywordText = wxSTC_INDIC_CONTAINER + 3;
constexpr int kIndicInactive = wxSTC_INDIC_CONTAINER + 4; ///< Dim recolor for inactive #if branches.

struct Constants final {
    static constexpr int edgeColumn = 80;
    static constexpr int foldMarginWidth = 16;
    static constexpr int changeMarginWidth = 5;
    static constexpr int analysesThrottle = 500;
};

auto isBrace(const int ch) -> bool {
    return ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}';
}

// Caret-movement keys — pressing one is treated as navigation and re-enables
// occurrence highlighting after it was suppressed by typing.
auto isNavigationKey(const int code) -> bool {
    switch (code) {
    case WXK_LEFT:
    case WXK_RIGHT:
    case WXK_UP:
    case WXK_DOWN:
    case WXK_HOME:
    case WXK_END:
    case WXK_PAGEUP:
    case WXK_PAGEDOWN:
    case WXK_NUMPAD_LEFT:
    case WXK_NUMPAD_RIGHT:
    case WXK_NUMPAD_UP:
    case WXK_NUMPAD_DOWN:
    case WXK_NUMPAD_HOME:
    case WXK_NUMPAD_END:
    case WXK_NUMPAD_PAGEUP:
    case WXK_NUMPAD_PAGEDOWN:
        return true;
    default:
        return false;
    }
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
    EVT_LEFT_DOWN(Editor::onLeftDown)
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
: wxStyledTextCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
, m_configManager(configManager)
, m_theme(theme)
, m_documentManager(documentManager)
, m_uiManager(uiManager)
, m_transformer(transformer)
, m_docType(type)
, m_preview(preview) {
    applySettings();
    m_intellisenseTimer.SetOwner(this);

    // Establish an initial baseline for the change tracker. Scintilla
    // only fires `SAVEPOINTREACHED` on a *transition* into the clean
    // state, so a brand-new editor (e.g. File → New) never gets one;
    // without this seed, modify events would land on an empty
    // `LineHistory` and produce no markers. A subsequent file load
    // re-baselines through the SAVEPOINTREACHED path.
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
        m_changeTracking = false;
        return;
    }

    m_changeTracking = editor.get_or("changeTracking", true);
    rebuildKeywordCompletions();
    // A custom context menu (built by the document manager) replaces Scintilla's
    // built-in popup so it can offer Go to Definition / Declaration.
    UsePopUp(wxSTC_POPUP_NEVER);
    Bind(wxEVT_CONTEXT_MENU, &Editor::onContextMenu, this);

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

    // Colour margin with only the two change markers visible. The mask
    // keeps fold and other markers from leaking onto this strip. The
    // type must be wxSTC_MARGIN_COLOUR (not _SYMBOL): Scintilla honours
    // SetMarginBackground only for a COLOUR margin — a SYMBOL margin is
    // painted with STYLE_LINENUMBER's background and ignores the call
    // (MarginView::PaintMargin). Markers draw on either type (mask-gated).
    SetMarginType(+Margins::Changes, wxSTC_MARGIN_COLOUR);
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

    // Occurrence highlight (identifier under the caret) and keyword-scope match
    // (for/next, sub/end sub, return) share the WordHighlight style — a box under
    // the text plus TEXTFORE — so the two read identically.
    configureMatchIndicators(kIndicOccurrenceBg, kIndicOccurrenceText);
    configureMatchIndicators(kIndicKeywordBg, kIndicKeywordText);

    // Inactive preprocessor branches recolor (TEXTFORE) to a dimmed tone — the
    // default foreground blended halfway to the background — so #if-excluded code
    // reads as greyed out without touching the lexer's styling.
    const auto& dimBase = defaultEntry.colors;
    const wxColour inactiveFg(
        static_cast<unsigned char>((dimBase.foreground.Red() + dimBase.background.Red()) / 2),
        static_cast<unsigned char>((dimBase.foreground.Green() + dimBase.background.Green()) / 2),
        static_cast<unsigned char>((dimBase.foreground.Blue() + dimBase.background.Blue()) / 2)
    );
    IndicatorSetStyle(kIndicInactive, wxSTC_INDIC_TEXTFORE);
    IndicatorSetForeground(kIndicInactive, inactiveFg);

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
    // Status bar (type label) and menu-enable state (Compile/Run, etc.)
    // both depend on the document type — refresh both now that it changed.
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

auto Editor::utf8Text() -> std::string {
    // Size from the buffer's own byte length — NOT GetTextLength(), which wx
    // documents as a *character* count and would truncate multi-byte UTF-8.
    const wxCharBuffer utf8 = GetTextRaw();
    return { utf8.data(), utf8.length() };
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

auto Editor::captureSelection() const -> SavedSelection {
    const bool rectangular = SelectionIsRectangle();
    const int anchor = rectangular ? GetRectangularSelectionAnchor() : GetAnchor();
    const int caret = rectangular ? GetRectangularSelectionCaret() : GetCurrentPos();
    return { rectangular, anchor, caret, LineFromPosition(anchor), LineFromPosition(caret) };
}

void Editor::applySelection(const SavedSelection& saved, const int newAnchor, const int newCaret) {
    if (saved.rectangular) {
        // SetSelection would flatten a block to a stream selection; rebuild the
        // rectangle instead (anchor/caret carry its direction).
        SetSelectionMode(wxSTC_SEL_RECTANGLE);
        SetRectangularSelectionAnchor(newAnchor);
        SetRectangularSelectionCaret(newCaret);
    } else {
        // SetSelection(from, to) collapses when from > to, dropping a backward
        // selection; set the anchor and caret directly to keep the direction.
        SetAnchor(newAnchor);
        SetCurrentPos(newCaret);
    }
}

void Editor::commentSelection() {
    const auto saved = captureSelection();
    const bool hadSelection = saved.anchor != saved.caret;
    const auto lineStart = LineFromPosition(GetSelectionStart());
    auto lineEnd = LineFromPosition(GetSelectionEnd());
    // A selection ending at the very start of a line doesn't cover that line, so
    // don't comment it (common when whole lines are selected).
    if (lineEnd > lineStart && GetSelectionEnd() == PositionFromLine(lineEnd)) {
        lineEnd--;
    }

    // Lock the case transformer for the duration: commenting must neither re-case
    // keywords nor let the caret-moved handler wipe the selection (issue #113).
    m_editorLocked = true;
    BeginUndoAction();
    for (auto line = lineStart; line <= lineEnd; line++) {
        InsertText(PositionFromLine(line), "'");
    }
    EndUndoAction();

    // Restore the selection (direction + type preserved): shift each end by the
    // number of commented lines starting at or before it. No selection → the
    // caret just follows.
    if (hadSelection) {
        const auto shift = [lineStart, lineEnd](const int pos, const int line) {
            return pos + (std::min(line, lineEnd) - lineStart + 1);
        };
        applySelection(saved, shift(saved.anchor, saved.anchorLine), shift(saved.caret, saved.caretLine));
    }
    m_editorLocked = false;
}

void Editor::uncommentSelection() {
    const auto saved = captureSelection();
    const bool hadSelection = saved.anchor != saved.caret;
    const auto lineStart = LineFromPosition(GetSelectionStart());
    auto lineEnd = LineFromPosition(GetSelectionEnd());
    // A selection ending at the very start of a line doesn't cover that line.
    if (lineEnd > lineStart && GetSelectionEnd() == PositionFromLine(lineEnd)) {
        lineEnd--;
    }

    // Capture each line's comment marker (absolute position + length) from the
    // original text, so both the removal and the selection fix-up use stable
    // positions. `len == 0` means the line isn't commented.
    struct Marker {
        int pos;
        int len;
    };
    std::vector<Marker> markers;
    markers.reserve(static_cast<std::size_t>(lineEnd - lineStart + 1));
    for (auto line = lineStart; line <= lineEnd; line++) {
        const auto indent = GetLineIndentation(line);
        const auto pos = PositionFromLine(line) + indent;
        const auto text = GetLine(line).Trim(false).Lower();
        if (text.StartsWith("rem ") || text.StartsWith("rem\t")) {
            markers.push_back({ pos, 3 });
        } else if (text.StartsWith("'")) {
            markers.push_back({ pos, 1 });
        } else {
            markers.push_back({ pos, 0 });
        }
    }

    // Lock the case transformer for the duration (issue #113), then remove
    // bottom-up so the captured positions stay valid as we go.
    m_editorLocked = true;
    BeginUndoAction();
    for (auto line = lineEnd; line >= lineStart; line--) {
        const auto& marker = markers[static_cast<std::size_t>(line - lineStart)];
        if (marker.len > 0) {
            SetTargetStart(marker.pos);
            SetTargetEnd(marker.pos + marker.len);
            ReplaceTarget("");
        }
    }
    EndUndoAction();

    // Restore the selection (direction + type preserved): shift each end left by
    // the markers removed before it, clamping if it fell inside one.
    if (hadSelection) {
        const auto shift = [&markers](const int pos, int /*line*/) {
            int delta = 0;
            for (const auto& marker : markers) {
                if (marker.len == 0) {
                    continue;
                }
                if (marker.pos + marker.len <= pos) {
                    delta -= marker.len;
                } else if (marker.pos < pos) {
                    delta -= pos - marker.pos;
                }
            }
            return pos + delta;
        };
        applySelection(saved, shift(saved.anchor, saved.anchorLine), shift(saved.caret, saved.caretLine));
    }
    m_editorLocked = false;
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
    updateOccurrenceHighlight();
    updateKeywordMatch();
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

    const int key = event.GetKey();
    if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == '_') {
        maybeShowCompletion();
    }
}

void Editor::onContextMenu(wxContextMenuEvent& event) {
    if (m_documentManager != nullptr) {
        m_documentManager->showEditorContextMenu(*this, event.GetPosition());
    } else {
        event.Skip();
    }
}

void Editor::maybeShowCompletion(const bool manual) {
    if (m_docType != DocumentType::FreeBASIC || m_documentManager == nullptr) {
        return;
    }
    if (!m_configManager.config().get_or("editor.codeCompletion", true)) {
        return;
    }
    if (AutoCompActive()) {
        return; // already open — Scintilla narrows the existing list as we type
    }

    const int pos = GetCurrentPos();
    const int wordStart = WordStartPosition(pos, true);
    if (!manual && wordStart >= pos) {
        return; // auto-trigger needs at least one typed character
    }

    // Trigger wherever a new identifier begins, except: while typing a number,
    // inside a string / comment, or after `.` / `->` (member access — we can't
    // resolve the member's type).
    if (wordStart < pos) {
        const auto firstCh = GetCharAt(wordStart);
        if (firstCh >= '0' && firstCh <= '9') {
            return; // a number literal, not an identifier
        }
    }
    if (isCommentOrStringStyle(GetStyleAt(wordStart))) {
        return;
    }
    int prev = wordStart - 1;
    while (prev >= 0 && (GetCharAt(prev) == ' ' || GetCharAt(prev) == '\t')) {
        prev--;
    }
    if (prev >= 0) {
        const auto before = GetCharAt(prev);
        if (before == '.' || (before == '>' && prev > 0 && GetCharAt(prev - 1) == '-')) {
            return; // member access — not a free identifier
        }
    }

    const auto* doc = m_documentManager->findByEditor(this);
    const auto symbols = doc != nullptr ? doc->getSymbolTable() : nullptr;

    // Global buckets combine the document with its `#include` closure, so key the
    // cache on a combined hash (own + each imported table's hash) — an edit to
    // any include then refreshes them, while typing within one file does not.
    std::size_t hash = symbols != nullptr ? symbols->getHash() : 0;
    if (symbols != nullptr) {
        for (const auto& imported : doc->getImportedTables()) {
            hash ^= imported->getHash() + 0x9e3779b9U + (hash << 6) + (hash >> 2);
        }
    }
    if (!m_globalCompletionsReady || hash != m_globalCompletionsHash) {
        m_globalSymbols.clear();
        m_globalVariables.clear();
        if (symbols != nullptr) {
            symbols->globalSymbolCompletions(m_globalSymbols);
            symbols->moduleVariableCompletions(m_globalVariables);
            // Include-provided symbols are global candidates too.
            for (const auto& imported : doc->getImportedTables()) {
                imported->globalSymbolCompletions(m_globalSymbols);
                imported->moduleVariableCompletions(m_globalVariables);
            }
        }
        sortUniqueCI(m_globalSymbols);
        sortUniqueCI(m_globalVariables);
        m_globalCompletionsHash = hash;
        m_globalCompletionsReady = true;
    }

    // Per-caret local buckets are scope-dependent, so they are not cached.
    m_localVariables.clear();
    m_localSymbols.clear();
    if (symbols != nullptr) {
        symbols->localCompletionsAt(pos, m_localVariables);
        symbols->memberCompletionsAt(pos, m_localSymbols, doc->getImportedTables());
    }
    sortUniqueCI(m_localVariables);
    sortUniqueCI(m_localSymbols);

    // Assemble in priority order; an earlier bucket shadows a later same-named
    // entry (a local hides a global, a user symbol hides a keyword, ...).
    m_completionItems.clear();
    m_completionSeen.clear(); // reuse the buckets; this runs per typed character
    const auto appendBucket = [this](const std::vector<wxString>& bucket) {
        for (const auto& name : bucket) {
            // Lowercase the utf8 form in place (one alloc) and move it into the
            // set on first sight — half the per-candidate churn of Lower().utf8_string().
            std::string key = name.utf8_string();
            std::ranges::transform(key, key.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (m_completionSeen.insert(std::move(key)).second) {
                m_completionItems.push_back(name);
            }
        }
    };
    appendBucket(m_localVariables);
    appendBucket(m_localSymbols);
    appendBucket(m_globalVariables);
    appendBucket(m_globalSymbols);
    appendBucket(g_keywordCompletions);
    if (m_completionItems.empty()) {
        return;
    }

    m_completionList.clear();
    for (const auto& name : m_completionItems) {
        if (!m_completionList.empty()) {
            m_completionList += ' ';
        }
        m_completionList += name;
    }

    // CUSTOM order keeps the bucket grouping and matches the prefix with a
    // linear, order-independent scan (so `__`-prefixed names match too).
    AutoCompSetIgnoreCase(true);
    AutoCompSetCaseInsensitiveBehaviour(wxSTC_CASEINSENSITIVEBEHAVIOUR_IGNORECASE);
    AutoCompSetOrder(wxSTC_ORDER_CUSTOM);
    AutoCompShow(pos - wordStart, m_completionList);
}

void Editor::rebuildKeywordCompletions() {
    g_keywordCompletions.clear();
    const auto& groups = m_configManager.keywords().at("groups");
    for (const auto category : { ThemeCategory::KeywordLibrary, ThemeCategory::KeywordConstants, ThemeCategory::KeywordPP, ThemeCategory::KeywordCustom }) {
        const std::string words(groups.get_or(wxString(getThemeCategoryName(category)), "").utf8_str());
        std::size_t idx = 0;
        while (idx < words.size()) {
            while (idx < words.size() && (std::isspace(static_cast<unsigned char>(words[idx])) != 0)) {
                idx++;
            }
            const std::size_t start = idx;
            while (idx < words.size() && (std::isspace(static_cast<unsigned char>(words[idx])) == 0)) {
                idx++;
            }
            if (idx > start) {
                g_keywordCompletions.push_back(wxString::FromUTF8(words.substr(start, idx - start)));
            }
        }
    }
    sortUniqueCI(g_keywordCompletions);
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

void Editor::configureMatchIndicators(const int bgIndic, const int textIndic) {
    const auto& wordHl = m_theme.getWordHighlight();
    IndicatorSetStyle(bgIndic, wxSTC_INDIC_STRAIGHTBOX);
    IndicatorSetUnder(bgIndic, true);
    IndicatorSetAlpha(bgIndic, wxSTC_ALPHA_OPAQUE);
    IndicatorSetOutlineAlpha(bgIndic, wxSTC_ALPHA_OPAQUE);
    if (wordHl.background.IsOk()) {
        IndicatorSetForeground(bgIndic, wordHl.background);
    }
    IndicatorSetStyle(textIndic, wxSTC_INDIC_TEXTFORE);
    if (wordHl.foreground.IsOk()) {
        IndicatorSetForeground(textIndic, wordHl.foreground);
    }
}

void Editor::updateOccurrenceHighlight() {
    if (!m_configManager.config().get_or("editor.highlightOccurrences", true)
        || m_docType != DocumentType::FreeBASIC) {
        clearOccurrenceHighlight();
        return;
    }
    // Suppressed after a text edit until the next navigation (arrow key / mouse
    // click). Keeps typing from re-highlighting the word being edited; a styling
    // or caret-settle tick can't lift it (only real navigation input does).
    if (m_matchSuppressed) {
        clearOccurrenceHighlight();
        return;
    }
    const wxString word = occurrenceWordAtCaret();
    // Caret still within the same identifier (case-insensitive, as FB is) — the
    // existing highlights are already correct, so skip the whole-document scan.
    if (word.IsSameAs(m_lastHighlightedWord, false)) {
        return;
    }
    clearOccurrenceHighlight();
    if (word.empty()) {
        return;
    }
    fillOccurrences(word);
    m_lastHighlightedWord = word;
}

void Editor::clearOccurrenceHighlight() {
    if (m_lastHighlightedWord.empty()) {
        return; // nothing of ours is painted
    }
    const int len = GetLength();
    SetIndicatorCurrent(kIndicOccurrenceBg);
    IndicatorClearRange(0, len);
    SetIndicatorCurrent(kIndicOccurrenceText);
    IndicatorClearRange(0, len);
    m_lastHighlightedWord.clear();
}

void Editor::applyInactiveRanges(const std::vector<std::pair<int, int>>& ranges) {
    SetIndicatorCurrent(kIndicInactive);
    IndicatorClearRange(0, GetLength());
    if (m_docType != DocumentType::FreeBASIC
        || !m_configManager.config().get_or("editor.dimInactiveCode", true)) {
        return;
    }
    const int len = GetLength();
    for (const auto& [start, end] : ranges) {
        const int from = std::clamp(start, 0, len);
        const int to = std::clamp(end, 0, len);
        if (to > from) {
            IndicatorFillRange(from, to - from);
        }
    }
}

auto Editor::occurrenceWordAtCaret() -> wxString {
    int start = 0;
    int end = 0;
    if (GetSelectionEmpty()) {
        const int pos = GetCurrentPos();
        start = WordStartPosition(pos, true);
        end = WordEndPosition(pos, true);
    } else {
        // A selection highlights its matches only when it spans exactly one whole
        // identifier; partial or multi-token selections highlight nothing.
        start = GetSelectionStart();
        end = GetSelectionEnd();
        if (WordStartPosition(start, true) != start || WordEndPosition(start, true) != end) {
            return {};
        }
    }
    if (end - start < 2) {
        return {}; // not on a word, or a single-character one
    }
    // Identifiers only — never keywords, comments, strings, numbers, operators.
    const int style = GetStyleAt(start);
    if (style != +ThemeCategory::Identifier && style != +ThemeCategory::IdentifierPP) {
        return {};
    }
    return GetTextRange(start, end);
}

void Editor::fillOccurrences(const wxString& word) {
    const auto& wordHl = m_theme.getWordHighlight();
    const bool hasBg = wordHl.background.IsOk();
    const bool hasFg = wordHl.foreground.IsOk();
    if (!hasBg && !hasFg) {
        return;
    }
    // Whole-word, case-insensitive — FreeBASIC identifiers are case-insensitive.
    SetSearchFlags(wxSTC_FIND_WHOLEWORD);
    const int len = GetLength();
    SetTargetStart(0);
    SetTargetEnd(len);
    while (SearchInTarget(word) != -1) {
        const int matchStart = GetTargetStart();
        const int matchEnd = GetTargetEnd();
        if (matchEnd <= matchStart) {
            break; // defensive — never advance backwards
        }
        if (hasBg) {
            SetIndicatorCurrent(kIndicOccurrenceBg);
            IndicatorFillRange(matchStart, matchEnd - matchStart);
        }
        if (hasFg) {
            SetIndicatorCurrent(kIndicOccurrenceText);
            IndicatorFillRange(matchStart, matchEnd - matchStart);
        }
        SetTargetStart(matchEnd);
        SetTargetEnd(len);
    }
}

void Editor::updateKeywordMatch() {
    clearKeywordMatch();
    if (m_matchSuppressed || m_docType != DocumentType::FreeBASIC || m_documentManager == nullptr) {
        return;
    }
    const auto* doc = m_documentManager->findByEditor(this);
    if (doc == nullptr) {
        return;
    }
    const auto symbols = doc->getSymbolTable();
    if (symbols == nullptr) {
        return;
    }
    const int pos = GetCurrentPos();
    const std::vector<std::pair<int, int>>* spans = &symbols->matchBlockAt(pos);
    if (spans->empty()) {
        // `Return` is not a block keyword — detect it on the caret and match the
        // enclosing procedure (opener + closer + the Return keyword itself).
        const int start = WordStartPosition(pos, true);
        const int end = WordEndPosition(pos, true);
        if (end > start && isKeywordCategory(static_cast<ThemeCategory>(GetStyleAt(start)))
            && GetTextRange(start, end).Lower() == "return") {
            spans = &symbols->matchProcedureAt(pos, std::pair { start, end });
        }
    }
    if (spans->empty()) {
        return;
    }
    const auto& wordHl = m_theme.getWordHighlight();
    const bool hasBg = wordHl.background.IsOk();
    const bool hasFg = wordHl.foreground.IsOk();
    for (const auto& [from, to] : *spans) {
        if (to <= from) {
            continue;
        }
        if (hasBg) {
            SetIndicatorCurrent(kIndicKeywordBg);
            IndicatorFillRange(from, to - from);
        }
        if (hasFg) {
            SetIndicatorCurrent(kIndicKeywordText);
            IndicatorFillRange(from, to - from);
        }
    }
}

void Editor::clearKeywordMatch() {
    const int len = GetLength();
    SetIndicatorCurrent(kIndicKeywordBg);
    IndicatorClearRange(0, len);
    SetIndicatorCurrent(kIndicKeywordText);
    IndicatorClearRange(0, len);
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
    const auto state = m_docType == DocumentType::FreeBASIC
                         ? UIState::FocusedValidSourceFile
                         : UIState::FocusedUnknownFile;
    m_uiManager->setDocumentState(state);
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
    m_documentManager->submitIntellisense(doc, utf8Text());
}

void Editor::onKeyDown(wxKeyEvent& event) {
    if (m_docType == DocumentType::FreeBASIC && event.ControlDown() && event.GetKeyCode() == WXK_SPACE) {
        maybeShowCompletion(true);
        return; // consume Ctrl+Space — do not insert a space
    }
    event.Skip();
    if (isNavigationKey(event.GetKeyCode())) {
        m_matchSuppressed = false; // navigation re-enables match highlighting
    }
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

void Editor::onLeftDown(wxMouseEvent& event) {
    event.Skip();
    m_matchSuppressed = false; // a click is navigation — re-enable match highlighting
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

    const auto symbols = doc->getSymbolTable();
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

    // A genuine user edit disables match highlighting until the next navigation
    // (arrow key / mouse click). Edits made by the on-type transformer run with
    // `m_editorLocked` set — notably the keyword case conversion, which fires
    // from caret moves; suppressing on those would hide the match the same
    // navigation should reveal. So let the transform run, then match.
    if (!m_editorLocked) {
        m_matchSuppressed = true;
    }

    // A user edit resolves any pending external-change notification — they've
    // chosen to keep working on their version (no-op when nothing is pending,
    // including our own reload, which clears the pending state first).
    if (m_documentManager != nullptr) {
        if (auto* doc = m_documentManager->findByEditor(this)) {
            doc->dismissExternalNotification();
        }
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
