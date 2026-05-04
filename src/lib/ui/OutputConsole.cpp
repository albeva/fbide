//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "OutputConsole.hpp"
#include "app/Context.hpp"
#include "compiler/CompilerManager.hpp"
using namespace fbide;

// clang-format off
wxBEGIN_EVENT_TABLE(OutputConsole, wxListCtrl)
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY,   OutputConsole::onItemActivated)
    EVT_LIST_ITEM_RIGHT_CLICK(wxID_ANY, OutputConsole::onRightClick)
    EVT_LIST_COL_RIGHT_CLICK(wxID_ANY,  OutputConsole::onRightClick)
wxEND_EVENT_TABLE()
// clang-format on

OutputConsole::OutputConsole(wxWindow* parent, Context& ctx)
: wxListCtrl(
      parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES
  )
, m_ctx(ctx) {}

void OutputConsole::create() {
    SetFont(wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

    wxListItem col;
    col.SetAlign(wxLIST_FORMAT_LEFT);

    col.SetText(m_ctx.tr("panels.results.columns.line"));
    InsertColumn(0, col);
    SetColumnWidth(0, 60);

    col.SetText(m_ctx.tr("panels.results.columns.file"));
    InsertColumn(1, col);
    SetColumnWidth(1, 150);

    col.SetText(m_ctx.tr("panels.results.columns.errorNr"));
    InsertColumn(2, col);
    SetColumnWidth(2, 100);

    col.SetText(m_ctx.tr("panels.results.columns.message"));
    InsertColumn(3, col);
    SetColumnWidth(3, 600);
}

void OutputConsole::onItemActivated(wxListEvent& event) {
    const auto idx = event.GetIndex();
    const auto lineStr = GetItemText(idx);
    unsigned long lineNr = 0;
    if (!lineStr.empty() && lineStr.ToULong(&lineNr)) {
        wxListItem item;
        item.SetId(idx);
        item.SetColumn(1);
        item.SetMask(wxLIST_MASK_TEXT);
        GetItem(item);
        m_ctx.getCompilerManager().goToError(static_cast<int>(lineNr), item.GetText());
    }
}

void OutputConsole::onRightClick(wxListEvent& /*event*/) {
    m_ctx.getCompilerManager().showCompilerLog();
}

void OutputConsole::addItem(const int lineNr, const int errorNr, const wxString& fileName, const wxString& message) {
    wxString lnrStr;
    if (lineNr != -1) {
        lnrStr << lineNr;
    }

    const auto itemCount = GetItemCount();
    const long idx = InsertItem(itemCount, lnrStr, 0);
    SetItemData(idx, 0);
    SetItem(itemCount, 1, fileName);

    wxString errStr;
    if (errorNr != -1) {
        errStr << errorNr;
    }
    SetItem(itemCount, 2, errStr);
    SetItem(itemCount, 3, message);
}
