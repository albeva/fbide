//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AboutDialog.hpp"
#include "cmake/config.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/ui/BBCodeText.hpp"
namespace XPM {
#include "rc/fbide.xpm"
}
using namespace fbide;

AboutDialog::AboutDialog(wxWindow* parent, Context& ctx)
: Layout(
      parent, wxID_ANY, "About",
      wxDefaultPosition, wxSize(300, -1),
      wxDEFAULT_DIALOG_STYLE
  )
, m_ctx(ctx) {}

void AboutDialog::create() {
    const auto infoFont = wxFont(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE));

    const auto banner = make_unowned<wxStaticBitmap>(
        this, wxID_ANY, wxBitmap(XPM::fbide_xpm),
        wxDefaultPosition, wxSize(300, 75)
    );
    add(banner, {});

    vbox("FBIde information", {}, [&] {
        const auto info = label(
            wxString::Format(
                "Version:       %s\n"
                "Build date:    %s\n"
                "wxWidgets:     %d.%d.%d",
                cmake::project.version,
                __DATE__,
                wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER
            )
        );
        info->SetFont(infoFont);

        separator();

        const auto text = make_unowned<BBCodeText>(this, wxID_ANY, loadReadme(), wxDefaultPosition, wxSize(-1, 150));
        add(text, { .proportion = 1 });
    });

    add(CreateStdDialogButtonSizer(wxOK), { .flag = wxEXPAND | wxBOTTOM, .margin = 5 });
    Fit();
    Centre();
}

auto AboutDialog::loadReadme() const -> wxString {
    const auto readmePath = m_ctx.getConfig().getAppSettingsPath() + "readme.txt";
    wxString content;
    wxFile file(readmePath);
    if (!file.IsOpened()) {
        return {};
    }
    file.ReadAll(&content);

    wxString bbcode;
    wxStringTokenizer lines(content, "\n", wxTOKEN_RET_EMPTY);
    while (lines.HasMoreTokens()) {
        auto line = lines.GetNextToken();
        line.Trim();
        if (!line.empty() && line.Last() == ':') {
            bbcode += "[bold]" + line + "[/bold]\n";
        } else {
            bbcode += line + "\n";
        }
    }
    return bbcode;
}
