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
 * Program URL: https://github.com/albeva/fbide
 */

#ifndef __WXMSW__
#include "rc/bitmaps/fbide.xpm"
#endif

#include "inc/AboutDialog.h"
#include "wx/statline.h"
#include "inc/FBIdeMainFrame.h"

//IMPLEMENT_DYNAMIC_CLASS( AboutDialog, wxDialog )

BEGIN_EVENT_TABLE(AboutDialog, wxDialog)
        EVT_CLOSE(AboutDialog::OnCloseWindow)
        EVT_BUTTON(wxID_OK, AboutDialog::OnOkClick)
END_EVENT_TABLE()

AboutDialog::AboutDialog(wxWindow *parent, wxWindowID id, const wxString &caption, const wxPoint &pos, const wxSize &size,
                         long style) {
    Parent = (FBIdeMainFrame *) parent;
    Create(parent, id, caption, pos, size, style);
}


bool AboutDialog::Create(wxWindow *parent, wxWindowID id, const wxString &caption, const wxPoint &pos, const wxSize &size,
                         long style) {
    SetExtraStyle(GetExtraStyle() | wxWS_EX_BLOCK_EVENTS);
    wxDialog::Create(parent, id, caption, pos, size, style);

    CreateControls();
    GetSizer()->Fit(this);
    GetSizer()->SetSizeHints(this);
    Centre();
    return TRUE;
}


void AboutDialog::CreateControls() {
    AboutDialog *itemDialog1 = this;
    wxString temp;

    auto *itemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
    itemDialog1->SetSizer(itemBoxSizer2);

    auto courierNew = wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Courier New");

    wxBitmap itemStaticBitmap3Bitmap(wxBITMAP(fbide));
    auto *itemStaticBitmap3 = new wxStaticBitmap(itemDialog1, wxID_STATIC, itemStaticBitmap3Bitmap,
                                                           wxDefaultPosition, wxSize(300, 75), 0);
    itemBoxSizer2->Add(itemStaticBitmap3, 0, wxALIGN_CENTER_HORIZONTAL, 5);

    auto *itemStaticBoxSizer4Static = new wxStaticBox(itemDialog1, wxID_ANY, "FBIde information");
    auto *itemStaticBoxSizer4 = new wxStaticBoxSizer(itemStaticBoxSizer4Static, wxVERTICAL);
    itemBoxSizer2->Add(itemStaticBoxSizer4, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    temp = "";
    temp << "Version:    " << VER_MAJOR << "." << VER_MINOR << "." << VER_RELEASE;
    auto *itemStaticText5 = new wxStaticText(itemDialog1, wxID_STATIC, temp, wxDefaultPosition, wxDefaultSize,
                                                     0);
    itemStaticText5->SetFont(courierNew);
    itemStaticBoxSizer4->Add(itemStaticText5, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT, 5);

    temp = "";
    temp << "Build date: " << _(__DATE__);
    auto *itemStaticText7 = new wxStaticText(itemDialog1, wxID_STATIC, temp, wxDefaultPosition, wxDefaultSize,
                                                     0);
    itemStaticText7->SetFont(courierNew);
    itemStaticBoxSizer4->Add(itemStaticText7, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT, 5);

    temp = "";
    temp << "wxWidgets:  " << wxMAJOR_VERSION << "." << wxMINOR_VERSION << "." << wxRELEASE_NUMBER;
    auto *itemStaticText8 = new wxStaticText(itemDialog1, wxID_STATIC, temp, wxDefaultPosition, wxDefaultSize,
                                                     0);
    itemStaticText8->SetFont(courierNew);
    itemStaticBoxSizer4->Add(itemStaticText8, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT, 5);

    auto *itemStaticLine9 = new wxStaticLine(itemDialog1, wxID_STATIC, wxDefaultPosition, wxDefaultSize,
                                                     wxLI_HORIZONTAL);
    itemStaticBoxSizer4->Add(itemStaticLine9, 0, wxGROW | wxTOP | wxBOTTOM, 5);

    auto *txm7 = new wxTextCtrl(itemDialog1, ID_TEXTCTRL,
                                      _(""),
                                      wxDefaultPosition, wxSize(-1, 200),
                                      wxVSCROLL | wxHSCROLL | wxTE_READONLY | wxTE_RICH2 | wxTE_DONTWRAP |
                                      wxTE_MULTILINE);
    txm7->SetLabel("");
    itemStaticBoxSizer4->Add(txm7, 0, wxGROW, 5);
    wxArrayString myarr;
    myarr.Add(wxString::Format("[bold]FBIde %d.%d.%d[/bold]", VER_MAJOR, VER_MINOR, VER_RELEASE));
    myarr.Add(Parent->Lang[204]);
    myarr.Add(Parent->Lang[205]);
    myarr.Add(Parent->Lang[206]);
    myarr.Add("");
    myarr.Add("[bold]" + Parent->Lang[207]);
    myarr.Add(Parent->Lang[208] + "[/bold]");
    myarr.Add("VonGodric - " + Parent->Lang[209]);
    //    myarr.Add("dilyias - "+Parent->Lang[211]);
    myarr.Add("dumbledore - " + Parent->Lang[212]);
    myarr.Add("Madedog - " + Parent->Lang[213]);
    myarr.Add("");
    myarr.Add("[bold]" + Parent->Lang[214] + "[/bold]");
    myarr.Add("aetherFox - " + Parent->Lang[215]);
    myarr.Add("Mecki - " + Parent->Lang[216]);
    myarr.Add("");
    myarr.Add("[bold]" + Parent->Lang[217] + "[/bold]");
    myarr.Add("Shadowolf\tAetherFox");
    myarr.Add("Z!re\t\tak00ma");
    myarr.Add("Nodtveidt\t\tWhitetiger");
    myarr.Add("DrV\t\tshiftLynx");
    myarr.Add("SJ Zero\t\tNexinarus");
    myarr.Add("");
    myarr.Add("[bold]Language files by[/bold]"); //Language files by:
    myarr.Add("v!ct0r\t\t- portuguese");
    myarr.Add("Mecki\t\t- german");
    myarr.Add("MystikShadows\t- french");
    myarr.Add("Dutchtux\t\t- dutch");
    myarr.Add("E. Gerfanow \t- russian");
    myarr.Add("Rojalus Kele \t- chinese");
    myarr.Add("Drakontas\t- greek");
    myarr.Add("Shion\t\t- japanese");
    myarr.Add("Nicolae Panaitoiu \t- romanian");
    myarr.Add("Lurah \t\t- finnish");
    myarr.Add("Etko \t\t- slovak");
    myarr.Add("");
    myarr.Add(Parent->Lang[218]);
    wxString tag = "";

    wxTextAttr normal = txm7->GetDefaultStyle();
    wxTextAttr bold = wxTextAttr(normal);
    bold.SetFont(bold.GetFont().Bold());

    bool nesting = false;
    for (int i = 0; i < (int) myarr.Count(); i++) {
        for (int j = 0; j < (int) myarr[i].Len(); j++) {
            wxString thechar = myarr[i].Mid(j, 1);
            if (thechar == '[' && !nesting) {
                nesting = true;
            } else if (thechar == ']' && nesting) {
                nesting = false;
                tag = tag.MakeLower();
                if (tag == "bold") {
                    txm7->SetDefaultStyle(bold);
                } else if (tag == "/bold") {
                    txm7->SetDefaultStyle(normal);
                }
                tag = "";
            } else if (nesting) {
                tag += thechar;
            } else {
                txm7->WriteText(thechar);
            }
        }
        txm7->WriteText("\r\n");
    }
    txm7->SetInsertionPoint(txm7->XYToPosition(0, 0));
    Centre();

    auto *itemButton11 = new wxButton(itemDialog1, wxID_OK, "&OK", wxDefaultPosition, wxDefaultSize, 0);
    itemBoxSizer2->Add(itemButton11, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 5);
}

wxBitmap AboutDialog::GetBitmapResource(const wxString &name) {
    if (name == wxT("ide/fbide.bmp")) {
        wxBitmap bitmap(_T("ide/fbide.bmp"), wxBITMAP_TYPE_BMP);
        return bitmap;
    }
    return wxNullBitmap;
}

void AboutDialog::OnOkClick(wxCommandEvent &event) {
    this->EndModal(true);
}


void AboutDialog::OnCloseWindow(wxCloseEvent &event) {
    this->EndModal(true);
}


