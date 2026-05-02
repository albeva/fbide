//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;

/// Output console panel for displaying compiler messages and errors.
class OutputConsole final : public wxListCtrl {
public:
    NO_COPY_AND_MOVE(OutputConsole)

    /// Construct without populating columns; `create()` does that later.
    OutputConsole(wxWindow* parent, Context& ctx);

    /// Set up columns and event handlers. Call after construction.
    void create();

    /// Add a log entry to the console.
    /// Pass -1 for lineNr or errorNr to leave that column blank.
    void addItem(int lineNr, int errorNr, const wxString& fileName, const wxString& message);

    /// Clear all items.
    void clear() { DeleteAllItems(); }

private:
    /// List row activated — jump to the error's source location.
    void onItemActivated(wxListEvent& event);

    Context& m_ctx; ///< Application context.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
