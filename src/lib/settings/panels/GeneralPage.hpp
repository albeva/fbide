//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "../../ui/controls/Panel.hpp"
#include "app/Context.hpp"

namespace fbide {
class Context;

/// General settings tab — editor preferences, language selection.
class GeneralPage final : public Panel {
public:
    NO_COPY_AND_MOVE(GeneralPage)

    explicit GeneralPage(Context& ctx, wxWindow* parent);
    void create() override;
    void apply() override;

private:
    auto tr(const wxString& path) const -> wxString {
        return getContext().getConfigManager().locale().get_or(path, "");
    }

    // Left column
    bool m_autoIndent;
    bool m_indentGuide;
    bool m_showWhiteSpaces;
    bool m_showLineEndings;
    bool m_braceHighlight;

    // Right column
    bool m_syntaxHighlight;
    bool m_showLineNumbers;
    bool m_showRightMargin;
    bool m_foldMargin;
    bool m_splashScreen;

    // Bottom row
    int m_edgeColumn;
    int m_tabSize;
    wxString m_language;
};

} // namespace fbide
