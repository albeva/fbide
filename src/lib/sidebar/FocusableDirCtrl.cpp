//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FocusableDirCtrl.hpp"
using namespace fbide;

void FocusableDirCtrl::SetupSections() {
    if (m_focusRoot.IsEmpty()) {
        wxGenericDirCtrl::SetupSections(); // drives / home
        return;
    }
    wxString name = wxFileName(m_focusRoot).GetFullName();
    if (name.IsEmpty()) {
        name = m_focusRoot; // volume root etc. — fall back to the path
    }
    AddSection(m_focusRoot, name, static_cast<int>(wxFileIconsTable::folder));
}

void FocusableDirCtrl::setFocusRoot(const wxString& path) {
    m_focusRoot = path;
    ReCreateTree(); // re-runs SetupSections, so the new root takes effect
    if (!m_focusRoot.IsEmpty()) {
        ExpandPath(m_focusRoot); // open the focused folder to reveal its contents
    }
}
