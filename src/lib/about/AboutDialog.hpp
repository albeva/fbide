//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ui/controls/Layout.hpp"

namespace fbide {
class Context;

/// Modal "About FBIde" dialog: the app logo beside a markdown info page
/// (version, links, licenses), rendered on a uniform brand-blue background.
class AboutDialog final : public Layout<wxDialog> {
public:
    NO_COPY_AND_MOVE(AboutDialog)

    /// Construct without populating widgets; `create()` builds the UI.
    AboutDialog(wxWindow* parent, Context& ctx);
    ~AboutDialog() override = default;
    /// Build the dialog widgets.
    void create();

private:
    /// Load the bundled `readme.md` and substitute the version / build date /
    /// platform / wxWidgets placeholders.
    [[nodiscard]] auto loadAbout() const -> wxString;
    /// Route a markdown link click: web / mailto links fall through to the
    /// browser, `fbide:check-updates` triggers a manual update check, and any
    /// other URL is a bundled file (a license) opened in an editor tab.
    void onLink(wxCommandEvent& event);
    /// Close the dialog on Esc — there is no Close button, only the title-bar
    /// close box, so the key is handled explicitly.
    void onCharHook(wxKeyEvent& event);

    Context& m_ctx; ///< Application context.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
