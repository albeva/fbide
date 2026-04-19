//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ui/Panel.hpp"
#include "app/Context.hpp"

namespace fbide {

/// Keywords settings tab — keyword group editor.
class KeywordsPage final : public Panel {
public:
    NO_COPY_AND_MOVE(KeywordsPage)

    static constexpr std::size_t GROUP_COUNT = 4;

    explicit KeywordsPage(Context& ctx, wxWindow* parent);
    void create() override;
    void apply() override;

private:
    auto tr(const wxString& path) const -> wxString {
        return getContext().getConfigManager().locale().get_or(path, "");
    }

    void onGroupChanged(const wxCommandEvent& event);

    Unowned<wxChoice> m_groupChoice;
    Unowned<wxTextCtrl> m_textKeywords;
    std::size_t m_selectedGroup = 0;
    std::array<wxString, GROUP_COUNT> m_groups {};
};

} // namespace fbide
