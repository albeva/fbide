//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentInfoBar.hpp"
#include "DocumentManager.hpp"
#include "app/Context.hpp"
using namespace fbide;

namespace {
// Static button ids — caught by this panel's own event table.
constexpr wxWindowID kReloadId = wxID_HIGHEST + 1001;
constexpr wxWindowID kKeepId = wxID_HIGHEST + 1002;

constexpr int kBarPad = 6;
} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(DocumentInfoBar, wxPanel)
    EVT_BUTTON(kReloadId, DocumentInfoBar::onReload)
    EVT_BUTTON(kKeepId,   DocumentInfoBar::onKeep)
wxEND_EVENT_TABLE()
// clang-format on

DocumentInfoBar::DocumentInfoBar(wxWindow* parent, Context& ctx, Document& doc)
: wxPanel(parent)
, m_ctx(ctx)
, m_doc(doc) {
    SetDoubleBuffered(true);

    // Use the system "info" colours — the conventional notification palette,
    // and theme-aware (light / dark) for free.
    const wxColour infoBg = wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK);
    const wxColour infoText = wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT);
    SetBackgroundColour(infoBg);

    const auto sizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);

    const auto icon = make_unowned<wxStaticBitmap>(this, wxID_ANY, wxArtProvider::GetBitmapBundle(wxART_WARNING, wxART_BUTTON));
    sizer->Add(icon, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(kBarPad + 2));

    m_message = make_unowned<wxStaticText>(this, wxID_ANY, wxEmptyString);
    m_message->SetForegroundColour(infoText);
    sizer->Add(m_message, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(kBarPad + 2));

    // Both buttons share the same look — FlatButton's default base derives from
    // the environment (system button colours), no per-button tinting.
    m_reload = make_unowned<FlatButton>(this, kReloadId, m_ctx.tr("messages.externalReload"));
    m_keep = make_unowned<FlatButton>(this, kKeepId, m_ctx.tr("messages.externalKeep"));

    for (auto* button : { m_reload.get(), m_keep.get() }) {
        sizer->Add(button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(kBarPad));
    }

    SetSizer(sizer);
    Hide(); // shown on demand via showConflict / showDeleted
}

void DocumentInfoBar::present(const wxString& message) {
    m_message->SetLabel(message);
    Show();
    Layout();
    if (auto* parent = GetParent()) {
        parent->Layout(); // give the bar its row, shrinking the editor below
    }
}

void DocumentInfoBar::showConflict() {
    m_errorMode = false;
    m_reload->Show(); // a changed-on-disk file can be reloaded
    present(m_ctx.tr("messages.externalChangedConflict"));
}

void DocumentInfoBar::showDeleted() {
    m_errorMode = false;
    m_reload->Hide(); // nothing to reload — only Dismiss
    present(m_ctx.tr("messages.externalDeleted"));
}

void DocumentInfoBar::showError(const wxString& message) {
    m_errorMode = true;
    m_reload->Hide(); // an error has nothing to reload — only Dismiss
    present(message);
}

void DocumentInfoBar::dismiss() {
    m_errorMode = false;
    Hide();
    if (auto* parent = GetParent()) {
        parent->Layout();
    }
}

void DocumentInfoBar::dismissError() {
    if (m_errorMode) {
        dismiss();
    }
}

void DocumentInfoBar::onReload(wxCommandEvent& /*event*/) {
    dismiss();
    m_doc.setPendingExternal(Document::ExternalChange::None);
    // Keep undo history so the user can undo the reload and recover the
    // unsaved changes they chose to discard.
    m_ctx.getDocumentManager().applyReload(m_doc, /*keepUndo*/ true);
}

void DocumentInfoBar::onKeep(wxCommandEvent& /*event*/) {
    if (m_errorMode) {
        dismiss(); // a save error: just hide it, there's no external state to settle
        return;
    }
    // Dismiss: keep the in-editor version, re-baseline, hide the bar.
    m_doc.dismissExternalNotification();
}
