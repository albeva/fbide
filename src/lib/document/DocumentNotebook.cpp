//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentNotebook.hpp"
using namespace fbide;

namespace {
/// Matches the style flags `UIManager` historically used when it built
/// the document notebook in `createLayout()`. Centralised here so the
/// constant lives next to the class that owns the widget — and so any
/// future tweak (e.g. dropping `wxAUI_NB_TAB_SPLIT`) is a one-line
/// change with a clear owner.
constexpr long kNotebookStyle = wxAUI_NB_TOP
                              | wxAUI_NB_TAB_SPLIT
                              | wxAUI_NB_TAB_MOVE
                              | wxAUI_NB_SCROLL_BUTTONS
                              | wxAUI_NB_CLOSE_ON_ALL_TABS
                              | wxAUI_NB_MIDDLE_CLICK_CLOSE;
} // namespace

DocumentNotebook::DocumentNotebook(wxWindow* parent)
: wxAuiNotebook(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, kNotebookStyle) {}
