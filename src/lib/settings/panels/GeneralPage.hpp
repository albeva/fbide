//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "app/Context.hpp"
#include "ui/controls/Panel.hpp"

namespace fbide {
class Context;

/// General settings tab — editor preferences, language selection.
class GeneralPage final : public Panel {
public:
    NO_COPY_AND_MOVE(GeneralPage)

    /// Construct without populating widgets; `create()` builds the UI.
    explicit GeneralPage(Context& ctx, wxWindow* parent);
    /// Build the panel widgets.
    void create() override;
    /// Commit edits back into `ConfigManager`.
    void apply() override;

private:
    /// Locale lookup with empty default — sugar over `ConfigManager::locale().get_or`.
    auto tr(const wxString& path) const -> wxString {
        return getContext().getConfigManager().locale().get_or(path, "");
    }

    // Left column
    bool m_autoIndent;        ///< Auto-indent on Enter.
    bool m_transformKeywords; ///< Master on/off for the per-group case transform.
    bool m_indentGuide;       ///< Show indent guides.
    bool m_showWhiteSpaces;   ///< Visualise whitespace characters.
    bool m_showLineEndings;   ///< Visualise line-ending characters.
    bool m_braceHighlight;    ///< Highlight matching braces.

    // Right column
    bool m_syntaxHighlight;   ///< Enable syntax highlighting.
    bool m_showLineNumbers;   ///< Show line-number margin.
    bool m_showRightMargin;   ///< Show right margin guide line.
    bool m_foldMargin;        ///< Show fold margin.
    bool m_splashScreen;      ///< Show splash screen on startup.

    // Bottom row
    int m_edgeColumn;     ///< Right margin column.
    int m_tabSize;        ///< Tab width in columns.
    wxString m_encoding;  ///< Default `TextEncoding` config key (e.g. "UTF-8").
    wxString m_eolMode;   ///< Default `EolMode` config key ("LF" / "CRLF" / "CR").
    wxString m_language;  ///< Selected locale identifier (filename stem).
};

} // namespace fbide
