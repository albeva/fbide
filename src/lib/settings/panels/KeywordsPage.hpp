//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Panel.hpp"

namespace fbide {

/// Keywords settings tab — keyword group editor.
class KeywordsPage final : public Panel {
public:
    explicit KeywordsPage(Context& ctx, wxWindow* parent);
    void layout() override;
    void apply() override;

private:
    void onGroupChanged(wxCommandEvent& event);

    Unowned<wxChoice> m_chKeywordGroup;
    Unowned<wxTextCtrl> m_textKeywords;
    int m_groupOld = 0;
    std::array<wxString, 4> m_groups;
};

} // namespace fbide
