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
#include "lib/config/Lang.hpp"
#include "lib/ui/BBCodeText.hpp"
#include "lib/ui/Panel.hpp"
#ifndef __WXMSW__
namespace XPM {
#include "rc/fbide.xpm"
}
#endif
using namespace fbide;

namespace {

class AboutPanel final : public Panel {
public:
    NO_COPY_AND_MOVE(AboutPanel)

    AboutPanel(Context& ctx, wxWindow* parent)
    : Panel(ctx, wxID_ANY, parent) {}

    void create() override {
        const auto infoFont = wxFont(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE));

        const auto banner = make_unowned<wxStaticBitmap>(
            this, wxID_ANY, wxBITMAP(XPM::fbide),
            wxDefaultPosition, wxSize(300, 75)
        );
        add(banner, { .flag = wxALIGN_CENTER_HORIZONTAL });

        vbox("FBIde information", { .flag = wxEXPAND | wxALL }, [&] {
            const auto info = make_unowned<wxStaticText>(
                this, wxID_ANY,
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
            add(info, { .flag = wxEXPAND | wxLEFT | wxRIGHT, .border = 0 });
            separator();

            const auto text = make_unowned<BBCodeText>(this, wxID_ANY, getAbout(), wxDefaultPosition, wxSize(-1, 150));
            add(text, { .proportion = 1, .flag = wxEXPAND | wxALL, .border = 0 });
        });
    }

    void apply() override {}

private:
    auto getAbout() -> wxString {
        const auto readmePath = getConfig().getAppSettingsPath() + "readme.txt";
        wxString content;
        wxFile file(readmePath);
        if (!file.IsOpened()) {
            return "";
        }
        file.ReadAll(&content);

        // Lines ending with ':' are titles — wrap them in [bold] tags
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
};

} // namespace

AboutDialog::AboutDialog(wxWindow* parent, Context& ctx)
: wxDialog(
      parent, wxID_ANY, "About",
      wxDefaultPosition, wxSize(300, -1),
      wxDEFAULT_DIALOG_STYLE
  )
, m_ctx(ctx) {}

void AboutDialog::create() {
    const auto panel = make_unowned<AboutPanel>(m_ctx, this);
    panel->create();

    auto* btnSizer = CreateStdDialogButtonSizer(wxOK);

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    sizer->Add(panel, 1, wxEXPAND | wxALL);
    sizer->Add(btnSizer, 0, wxEXPAND | wxBOTTOM, 5);

    SetSizer(sizer);
    Fit();
    Centre();
}
