//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "lib/ui/Panel.hpp"
#include "lib/config/Keywords.hpp"

namespace fbide {

/// Keywords settings tab — keyword group editor.
class KeywordsPage final : public Panel {
public:
    NO_COPY_AND_MOVE(KeywordsPage)

    explicit KeywordsPage(Context& ctx, wxWindow* parent);
    void create() override;
    void apply() override;

private:
    void onGroupChanged(const wxCommandEvent& event);

    Unowned<wxChoice> m_groupChoice;
    Unowned<wxTextCtrl> m_textKeywords;
    std::size_t m_selectedGroup = 0;
    std::array<wxString, Keywords::GROUP_COUNT> m_groups {};
};

} // namespace fbide
