/*
 * This file is part of FBIde, an open-source (cross-platform) IDE for
 * FreeBasic compiler.
 * Copyright (C) 2020  Albert Varaksin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Contact e-mail: Albert Varaksin <albeva@me.com>
 * Program URL   : http://fbide.sourceforge.net
 */

#include "inc/main.h"
#include "inc/AboutDialog.h"
#include "inc/fbedit.h"
#include "inc/wxmynotebook.h"

//------------------------------------------------------------------------------
// Show AboutDialog dialog
void FBIdeMainFrame::OnAbout(wxCommandEvent & WXUNUSED(event)) {
    AboutDialog dlg(this);
    dlg.ShowModal();
}

void FBIdeMainFrame::OnHelp(wxCommandEvent &event) {

#ifdef __WXMSW__
    if (!stc) {
        help.DisplayContents();
        return;
    }

    int start = 0, end = 0;
    wxString strKw = GetTextUnderCursor(start, end).Trim(false).Trim(true).MakeLower();

    if (strKw.Len()) {
        if (stc->GetCharAt(start - 1) == '#')
            strKw = "#" + strKw;

        help.KeywordSearch(strKw);
        //help.DisplayTextPopup( strKw, wxPoint(300, 200 ) );
        //help.DisplayContextPopup( 1 );
    } else
        help.DisplayContents();
#endif
}


void FBIdeMainFrame::OnQuickKeys(wxCommandEvent &event) {
    wxString FileName(EditorPath + "IDE/quickkeys.txt");
    if (bufferList.FileLoaded(FileName) == -1) {
        NewSTCPage(FileName, true);
        SetTitle("FBIde - " + bufferList[FBNotebook->GetSelection()]->GetFileName());
    }
}


void FBIdeMainFrame::OnReadMe(wxCommandEvent &event) {
    wxString FileName(EditorPath + "IDE/readme.txt");
    if (bufferList.FileLoaded(FileName) == -1) {
        NewSTCPage(FileName, true);
        SetTitle("FBIde - " + bufferList[FBNotebook->GetSelection()]->GetFileName());
    }
}


void FBIdeMainFrame::OnFpp(wxCommandEvent &event) {
    wxString FileName(EditorPath + "IDE/fpp.txt");
    if (bufferList.FileLoaded(FileName) == -1) {
        NewSTCPage(FileName, true);
        SetTitle("FBIde - " + bufferList[FBNotebook->GetSelection()]->GetFileName());
    }
}
