/*
 * This file is part of FBIde, an open-source (cross-platform) IDE for
 * FreeBasic compiler.
 * Copyright (C) 2005  Albert Varaksin
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
 * Contact e-mail: Albert Varaksin <vongodric@hotmail.com>
 * Program URL   : http://fbide.sourceforge.net
 */

#include "inc/main.h"
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/listbox.h>
#include "inc/fbedit.h"
#include "inc/browser.h"

BEGIN_EVENT_TABLE(SFBrowser, wxDialog)
        EVT_CLOSE (SFBrowser::OnClose)
        EVT_TEXT(SearchBoxId, SFBrowser::OnCharAdded)
        EVT_TEXT_ENTER(SearchBoxId, SFBrowser::OnEnter)
        EVT_LIST_ITEM_SELECTED(-1, SFBrowser::OnSelect)
        EVT_LIST_ITEM_ACTIVATED(-1, SFBrowser::OnActivate)
#ifdef __WXMSW__
        EVT_HOTKEY(1985, SFBrowser::OnKeyUp)
#endif
END_EVENT_TABLE()

SFBrowser::SFBrowser(wxWindow *parent,
                     wxWindowID id,
                     const wxString &title,
                     long style,
                     const wxString &name) {

    ChangePos = false;
    Parent = (MyFrame *) parent;
    Create(parent, id, title, wxDefaultPosition, wxSize(300, 400), style, name);

    Panel = new wxPanel(this, wxID_ANY,
                        wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN);

    SearchLabel = new wxStaticText(Panel, -1, wxT(""), wxPoint(5, 7), wxSize(60, 13), wxST_NO_AUTORESIZE);
    SearchLabel->SetLabel(Parent->Lang[226]); //Search

    SearchBox = new wxTextCtrl(Panel, SearchBoxId, wxT(""), wxPoint(70, 5), wxSize(220, 21), wxTE_PROCESS_ENTER);
    wxBoxSizer *Sizer = new wxBoxSizer(wxVERTICAL);

    SFList = new wxListCtrl(Panel,
                            wxID_ANY,
                            wxDefaultPosition,
                            wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);

    wxFont LbFont(10, wxMODERN, wxNORMAL, wxNORMAL, false);
    SFList->SetFont(LbFont);


    //    wxFont LbFont (12, wxMODERN, wxNORMAL, wxNORMAL, false);
    // SFList->SetFont(LbFont);

    Sizer->Add(SearchLabel, 0, 0, 0);
    Sizer->Add(SearchBox, 0, wxGROW | wxRIGHT, 0);
    Sizer->Add(SFList, 3, wxGROW | (wxALL & ~wxTOP), 0);

    Panel->SetSizer(Sizer);

    SetMinSize(wxSize(510, 300));

    Sizer->Fit(this);
    Sizer->SetSizeHints(this);


    wxListItem itemCol;
    itemCol.SetText(Parent->Lang[165]);
    itemCol.SetAlign(wxLIST_FORMAT_LEFT);
    SFList->InsertColumn(0, itemCol);

    itemCol.SetText(Parent->Lang[227]);
    itemCol.SetAlign(wxLIST_FORMAT_LEFT);
    SFList->InsertColumn(1, itemCol);

    itemCol.SetText(Parent->Lang[228]);
    itemCol.SetAlign(wxLIST_FORMAT_LEFT);
    SFList->InsertColumn(2, itemCol);

    SFList->SetColumnWidth(0, 50);
    SFList->SetColumnWidth(1, 50);
    SFList->SetColumnWidth(2, 400);

    Refresh();
#ifdef __WXMSW__
    RegisterHotKey(1985, 0, 27);
#endif
    Rebuild();
}

void SFBrowser::Rebuild() {
    if (ChangePos)
        return;
    wxString Temp;
    bool type = false;
    wxString Arg;
    bool Add = false;
    FB_Edit *stc = Parent->stc;
    char ch;

    Original.Clear();
    OriginalArg.Clear();

    OrigLineNr.clear();
    OrigType.clear();

    for (int i = 0; i < stc->GetLineCount(); i++) {
        //Temp = stc->GetLine(i);
        Temp = stc->ClearCmdLine(i);
        if (Temp.empty()) {
            continue;
        }
        ch = Temp.GetChar(0);
        if (ch == 'p' || ch == 's' || ch == 'f') {
            auto keywords = stc->GetKeywords(Temp);
            auto fkw = keywords[0];
            auto skw = keywords[1];

            if (fkw == kw::PRIVATE || fkw == kw::STATIC) {
                if (skw == kw::SUB) {
                    type = false;
                    Add = true;
                } else if (skw == kw::FUNCTION) {
                    type = true;
                    Add = true;
                }
                Arg = Temp.Mid(Temp.Find(' '));
                Arg = Arg.Trim(false).Trim(true);
                Arg = Arg.Mid(Arg.Find(' '));
            } else if (fkw == kw::SUB) {
                type = false;
                Add = true;
                Arg = Temp.Mid(Temp.Find(' '));
                Arg = Arg.Trim(false).Trim(true);
            } else if (fkw == kw::FUNCTION && Temp.Mid(8).Trim(false).Mid(0, 1) != '=') {
                type = true;
                Add = true;
                Arg = Temp.Mid(Temp.Find(' '));
                Arg = Arg.Trim(false).Trim(true);
            }

            if (Add) {
                Add = false;
                Temp = "";
                if (type)
                    Temp << "func";
                else
                    Temp << "sub";
                Temp << " " << Arg;

                Original.Add(Temp);
                OriginalArg.Add(Arg);

                OrigLineNr.push_back(i);
                OrigType.push_back(type);
            }
        }
    }
    GenerateList(SearchString);
}

void SFBrowser::AddListItem(int Linenr, bool Type, wxString Message) {
    Message = Message.Trim(true).Trim(false);
    wxString lnr;
    lnr << Linenr;
    int Itemcount = SFList->GetItemCount();
    long tmp = SFList->InsertItem(Itemcount, lnr, 0);
    SFList->SetItemData(tmp, 0);

    wxListItem t;
    t.SetColumn(1);
    t.SetId(Itemcount);
    if (Type) {
        t.SetText("Func");
        t.SetTextColour(wxColour(0, 128, 0));
    } else {
        t.SetTextColour(wxColour(0, 0, 128));
        t.SetText("Sub");
    }

    SFList->SetItem(t);
    SFList->SetItem(Itemcount, 2, Message.Trim(true).Trim(false));
}


void SFBrowser::OnCharAdded(wxCommandEvent &event) {
    SearchString = event.GetString().Lower();
    GenerateList(SearchString);
}

void SFBrowser::OnKeyUp(wxKeyEvent &event) {
    if (event.GetKeyCode() == 27)
        Close(true);
}

void SFBrowser::OnEnter(wxCommandEvent &event) {
    ChangePos = true;
    FB_Edit *stc = Parent->stc;
    if (SFList->GetItemCount()) {
        unsigned long linnr = 0;
        SFList->GetItemText(0).ToULong(&linnr);
        stc->GotoLine(stc->GetLineCount());
        stc->GotoLine(linnr - 1);
    }
    ChangePos = false;
    Close(true);
}


void SFBrowser::OnSelect(wxListEvent &event) {
    ChangePos = true;
    int index = event.GetIndex();
    unsigned long linnr = 0;
    SFList->GetItemText(index).ToULong(&linnr);
    ChangePos = false;
}

void SFBrowser::OnActivate(wxListEvent &event) {
    ChangePos = true;
    FB_Edit *stc = Parent->stc;
    int index = event.GetIndex();
    unsigned long linnr = 0;
    SFList->GetItemText(index).ToULong(&linnr);
    stc->GotoLine(stc->GetLineCount());
    stc->GotoLine(linnr - 1);
    ChangePos = false;
    Close(true);
}


void SFBrowser::GenerateList(wxString Search) {
    this->Freeze();
    SFList->DeleteAllItems();
    if (Search.Len()) {
        for (unsigned int i = 0; i < Original.Count(); i++) {
            if (Original[i].Contains(Search)) {
                AddListItem(OrigLineNr[i] + 1, OrigType[i], OriginalArg[i]);
            }
        }
    } else {
        for (unsigned int i = 0; i < Original.Count(); i++) {
            AddListItem(OrigLineNr[i] + 1, OrigType[i], OriginalArg[i]);
        }
    }
    this->Thaw();
}


SFBrowser::~SFBrowser() {
    delete Panel;
    Parent->SFDialog = 0;
#ifdef __WXMSW__

    UnregisterHotKey(1985);
#endif

    return;
}

void SFBrowser::OnClose(wxCloseEvent &event) {
    Destroy();
}
