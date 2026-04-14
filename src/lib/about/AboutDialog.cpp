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
#include "lib/ui/Panel.hpp"
using namespace fbide;

namespace {

class AboutPanel final : public Panel {
public:
    NO_COPY_AND_MOVE(AboutPanel)

    AboutPanel(Context& ctx, wxWindow* parent)
    : Panel(ctx, wxID_ANY, parent) {}

    void create() override {
        const auto infoFont = wxFont(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE));

        // Banner image (embedded as BITMAP resource on Windows)
        const auto banner = make_unowned<wxStaticBitmap>(
            this, wxID_ANY, wxBITMAP(fbide),
            wxDefaultPosition, wxSize(300, 75)
        );
        add(banner, { .flag = wxALIGN_CENTER_HORIZONTAL });

        // Version information
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

            // Load readme.txt
            const auto text = make_unowned<wxTextCtrl>(
                this, wxID_ANY, getReadme(),
                wxDefaultPosition, wxDefaultSize,
                wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxTE_RICH2
            );
            text->SetFont(infoFont);
            add(text, { .proportion = 1, .flag = wxEXPAND | wxALL, .border = 0 });
        });
    }

    void apply() override {}

    [[nodiscard]] auto getReadme() const -> wxString {
        const auto readmePath = getConfig().getAppSettingsPath() + "readme.txt";
        wxString content;
        wxFile file(readmePath);
        if (file.IsOpened()) {
            file.ReadAll(&content);
        }
        return content;
    }
};

} // namespace

AboutDialog::AboutDialog(wxWindow* parent, Context& ctx)
: wxDialog(
      parent, wxID_ANY, ctx.getLang()[LangId::HelpAbout],
      wxDefaultPosition, wxSize(300, -1),
      wxDEFAULT_DIALOG_STYLE
  )
, m_ctx(ctx) {}

void AboutDialog::create() {
    const auto panel = make_unowned<AboutPanel>(m_ctx, this);
    panel->create();

    auto* btnSizer = CreateStdDialogButtonSizer(wxOK);

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    sizer->Add(panel, 1, wxEXPAND);
    sizer->Add(btnSizer, 0, wxEXPAND | wxALL, 10);

    SetSizer(sizer);
    Fit();
    Centre();
}
