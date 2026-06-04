//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AboutDialog.hpp"
#include "app/Context.hpp"
#include "cmake/config.hpp"
#include "config/ConfigManager.hpp"
#include "document/DocumentManager.hpp"
#include "document/DocumentPath.hpp"
#include "ui/controls/BBCodeText.hpp"
#include "workspace/WorkspaceManager.hpp"
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
        currentParent(), wxID_ANY, wxBitmap(XPM::fbide_xpm),
        wxDefaultPosition, wxSize(300, 75)
    );

    add(banner);

    vbox("FBIde information", { .margin = false }, [&] {
        const auto info = label(
            wxString::Format(
                "Version:       %s\n"
                "Build date:    %s\n"
                "wxWidgets:     %d.%d.%d",
                cmake::project.version,
                __DATE__,
                wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER
            ),
            {}
        );
        info->SetFont(infoFont);

        separator();

        const auto text = make_unowned<BBCodeText>(
            currentParent(), wxID_ANY, loadReadme(),
            wxDefaultPosition, wxSize(-1, 200)
        );
        add(text, { .proportion = 1 });
    });

    // Licence links — click to open the file in the editor and close
    // the dialog. The URL field of wxHyperlinkCtrl is unused (we
    // bind the click ourselves), but it must be non-empty or wx
    // refuses to construct the control on some platforms; we pass
    // the file name as a harmless placeholder.
    hbox({}, [&] {
        const auto addLink = [&](const wxString& label, const wxString& file) {
            const auto link = make_unowned<wxHyperlinkCtrl>(
                currentParent(), wxID_ANY, label, file
            );
            link->Bind(wxEVT_HYPERLINK, [this, file](wxHyperlinkEvent&) {
                const auto path = m_ctx.getConfigManager().absolute(toFsPath(file));
                m_ctx.getWorkspaceManager().openFile(path);
                EndModal(wxID_OK);
            });
            add(link);
        };
        addLink("License", "LICENSE");
        addLink("Third-party licenses", "THIRD_PARTY_LICENSES.txt");
    });

    add(CreateStdDialogButtonSizer(wxOK));
    SetSizerAndFit(currentSizer());
    Centre();
}

auto AboutDialog::loadReadme() const -> wxString {
    const auto readmePath = toWxString(m_ctx.getConfigManager().absolute("readme.txt"));
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
