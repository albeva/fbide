//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SymbolBrowserPanel.hpp"
#include <wx/srchctrl.h>
#include "SymbolBrowser.hpp"
#include "app/Context.hpp"
using namespace fbide;

SymbolBrowserPanel::SymbolBrowserPanel(Context& ctx, wxWindow* parent)
: wxPanel(parent, wxID_ANY)
, m_ctx(ctx) {
    m_search = make_unowned<wxSearchCtrl>(
        this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER
    );
    m_search->ShowSearchButton(true);
    m_search->ShowCancelButton(true);
    m_search->SetDescriptiveText(m_ctx.tr("sidebar.symbols.searchHint"));

    m_tree = make_unowned<SymbolBrowser>(m_ctx, this);

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    sizer->Add(m_search, 0, wxEXPAND | wxALL, 2);
    sizer->Add(m_tree, 1, wxEXPAND);
    SetSizer(sizer);

    // Real-time filtering: wxEVT_TEXT fires on every keystroke; the search
    // and cancel buttons route through the same handler. The cancel button
    // only clears the box — the resulting wxEVT_TEXT empties the filter.
    m_search->Bind(wxEVT_TEXT, &SymbolBrowserPanel::onSearch, this);
    m_search->Bind(wxEVT_SEARCH, &SymbolBrowserPanel::onSearch, this);
    m_search->Bind(wxEVT_SEARCH_CANCEL, [this](wxCommandEvent&) { m_search->Clear(); });
}

SymbolBrowserPanel::~SymbolBrowserPanel() = default;

void SymbolBrowserPanel::setSymbols(const Document* doc) {
    m_tree->setSymbols(doc);
}

void SymbolBrowserPanel::focusSearch() {
    m_search->SetFocus();
}

void SymbolBrowserPanel::onSearch(wxCommandEvent& /*event*/) {
    m_tree->setFilter(m_search->GetValue());
}
