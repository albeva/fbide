//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AboutDialog.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/Version.hpp"
#include "document/DocumentManager.hpp"
#include "markdown/MarkdownView.hpp"
#include "ui/controls/SmartBoxSizer.hpp"

using namespace fbide;

wxBEGIN_EVENT_TABLE(AboutDialog, Layout)
    EVT_CHAR_HOOK(AboutDialog::onCharHook)
    EVT_COMMAND(wxID_ANY, fbide::markdown::MARKDOWN_LINK_CLICKED, AboutDialog::onLink)
wxEND_EVENT_TABLE()

namespace {
// Deep navy sampled-dark from the splash artwork. The About dialog uses it
// uniformly, independent of the system light / dark theme; text and link
// colours are derived to stay legible on it.
const wxColour kBrandBlue { 18, 32, 58 };
const wxColour kBrandText { 228, 233, 245 };
const wxColour kBrandLink { 125, 165, 255 };
// Uniform padding around the content and the logo->text gap — one value, so the
// outer padding and the gap match.
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

    // The root sizer runs with no margin or gap; the content hbox below carries
    // the uniform kPad inset and gap, so one value wraps the logo and info page.
    static_cast<SmartBoxSizer*>(currentSizer())->setOptions({ .gap = 0, .margin = false });

    hbox({ .proportion = 1, .margin = true, .gap = kPad }, [&] {
        // Logo (left): logo.svg is cropped tight to the horse head, so render
        // at its exact intrinsic size (portrait — a square would distort it).
        // The bitmap edges are the head edges, so it places precisely; the SVG
        // bundle still rasterises crisply at any HiDPI scale.
        constexpr int size = 112;
        constexpr double ratio = 177.985 / 210.777;
        const wxSize logoSize(size, static_cast<int>(std::round(size / ratio))); // matches logo.svg's width / height
        const auto logo = wxBitmapBundle::FromSVGFile(
            m_ctx.getConfigManager().absolute("logo.svg"), logoSize
        );
        if (logo.IsOk()) {
            const auto bitmap = make_unowned<wxStaticBitmap>(currentParent(), wxID_ANY, logo);
            add(bitmap, { .proportion = 0, .expand = false });
        }

        // Info page (right) — all content comes from the markdown template.
        // No scroll style — sized to its content, so it never needs a scroll
        // bar — plus wxBORDER_NONE so wxGTK doesn't wrap it in a sunken border.
        const auto md = make_unowned<markdown::MarkdownView>(currentParent(), m_ctx, wxID_ANY, wxBORDER_NONE);
        md->SetMinSize(wxSize(400, -1)); // fixed width; height follows the content
        md->setSelectable(false);
        md->ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_NEVER);
        md->setContentBackground(kBrandBlue);
        md->setTextColour(kBrandText);
        md->setLinkColour(kBrandLink);
        md->setContentPadding(0);
        md->setTableStyle({ .borders = false, .columnSpacing = 16, .rowSpacing = 0 });
        md->refreshTheme();
        md->setMarkdown(loadAbout());
        add(md, { .proportion = 1 });
    });

    // One fit suffices: MarkdownView reports its content height for the pinned
    // width via DoGetBestSize, so the dialog sizes to fit with no scroll bar.
    SetSizerAndFit(currentSizer());
    Centre();
}

auto AboutDialog::loadAbout() const -> wxString {
    // ide/readme.md ships with {{...}} placeholders filled here at load time, so
    // the running binary — not a possibly-stale generated file — is the source
    // of truth (someone may swap fbide and keep an old ide/ folder beside it).
    const auto path = m_ctx.getConfigManager().absolute("readme.md");
    wxString content;
    if (wxFile file(path); file.IsOpened()) {
        file.ReadAll(&content);
    }
    content.Replace("{{version}}", Version::fbide().asString());
    content.Replace("{{variant}}", sizeof(void*) == 8 ? "64-bit" : "32-bit");
    content.Replace("{{buildDate}}", __DATE__);
    content.Replace("{{wxVersion}}", Version::wxWidgets().asString());
    content.Replace("{{wxPort}}", wxPlatformInfo::Get().GetPortIdName());
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

void AboutDialog::onCharHook(wxKeyEvent& event) {
    if (event.GetKeyCode() == WXK_ESCAPE) {
        EndModal(wxID_CANCEL);
        return;
    }
    event.Skip();
}
