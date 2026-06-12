//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AboutDialog.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "document/DocumentManager.hpp"
#include "markdown/MarkdownView.hpp"
#include "ui/controls/SmartBoxSizer.hpp"

using namespace fbide;

namespace {
// Deep navy sampled-dark from the splash artwork. The About dialog uses it
// uniformly, independent of the system light / dark theme; text and link
// colours are derived to stay legible on it.
const wxColour kBrandBlue { 18, 32, 58 };
const wxColour kBrandText { 228, 233, 245 };
const wxColour kBrandLink { 125, 165, 255 };
// Uniform padding around the content and the logo->text gap — one value, so the
// outer padding and the gap match. The button row keeps the platform-default
// spacing below this.
constexpr int kPad = 20;
} // namespace

AboutDialog::AboutDialog(wxWindow* parent, Context& ctx)
: Layout(
      parent, wxID_ANY, "About FBIde",
      wxDefaultPosition, wxDefaultSize,
      wxDEFAULT_DIALOG_STYLE
  )
, m_ctx(ctx) {}

void AboutDialog::create() {
    SetBackgroundColour(kBrandBlue);

    hbox({ .proportion = 1, .gap = kPad }, [&] {
        // Logo (left), rendered from logo.svg via NanoSVG (wx is built with SVG).
        constexpr int logoSize = 112; // logical px; the bundle scales for HiDPI
        const auto logo = wxBitmapBundle::FromSVGFile(
            m_ctx.getConfigManager().absolute("logo.svg"), wxSize(logoSize, logoSize)
        );
        if (logo.IsOk()) {
            const auto bitmap = make_unowned<wxStaticBitmap>(currentParent(), wxID_ANY, logo);
            add(bitmap, { .proportion = 0, .expand = false });
        }

        // Info page (right) — all content comes from the markdown template.
        const auto md = make_unowned<markdown::MarkdownView>(currentParent(), m_ctx);
        md->SetMinSize(wxSize(430, -1)); // fixed width; height follows the content
        md->setSelectable(false);
        md->setContentBackground(kBrandBlue);
        md->setTextColour(kBrandText);
        md->setLinkColour(kBrandLink);
        md->setContentPadding(0);
        md->setTableStyle({ .borders = false, .columnSpacing = 10, .rowSpacing = 0 });
        md->refreshTheme();
        md->setMarkdown(loadAbout());
        md->Bind(markdown::MARKDOWN_LINK_CLICKED, &AboutDialog::onLink, this);
        add(md, { .proportion = 1 });
    });

    add(CreateStdDialogButtonSizer(wxOK));

    // The content hbox provides the uniform padding; drop the root sizer's own
    // outer margin so it isn't doubled (the button row keeps default spacing).
    static_cast<SmartBoxSizer*>(currentSizer())->setOptions({ .margin = false });

    // Single fit: the view reports its content height at its pinned width via
    // DoGetBestSize, so the (430, -1) min size sizes the dialog to fit with no
    // scroll bar — no explicit height, no second pass. Min-locked after.
    SetSizerAndFit(currentSizer());
    Centre();
}

auto AboutDialog::loadAbout() const -> wxString {
    // Generated at configure time with the real version / build / wxWidgets
    // values baked in, so it only needs reading.
    const auto path = m_ctx.getConfigManager().absolute("readme.md");
    wxString content;
    if (wxFile file(path); file.IsOpened()) {
        file.ReadAll(&content);
    }
    return content;
}

void AboutDialog::onLink(wxCommandEvent& event) {
    const wxString url = event.GetString();
    if (url.StartsWith("http://") || url.StartsWith("https://") || url.StartsWith("mailto:")) {
        event.Skip(); // fall through to wxLaunchDefaultBrowser
        return;
    }
    // A bundled file (a license) — open it in an editor tab and close the dialog.
    m_ctx.getDocumentManager().openFile(m_ctx.getConfigManager().absolute(url));
    EndModal(wxID_OK);
}
