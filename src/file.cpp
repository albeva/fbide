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
#include "inc/FBIdeMainFrame.h"
#include "inc/fbedit.h"
#include "inc/browser.h"
#include "inc/FormatDialog.h"
#include "inc/wxmynotebook.h"
#include "inc/FBIdeApp.h"

void FBIdeMainFrame::OnNew(wxCommandEvent & WXUNUSED(event)) {
    NewSTCPage("", true);
    SetTitle("FBIde - " + bufferList[FBNotebook->GetSelection()]->GetFileName());
}

void FBIdeMainFrame::OnOpen(wxCommandEvent & WXUNUSED(event)) {
    wxFileDialog dlg(
        this,
        Lang[186],//Load File
        "",
        ".bas",
        Lang[187],//Types
        wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

    if (dlg.ShowModal() != wxID_OK)
        return;

    wxArrayString File;
    dlg.GetPaths(File);
    for (int i = 0; i < (int) File.Count(); i++) {
        int result = bufferList.FileLoaded(File[i]);

        if (result != -1)
            FBNotebook->SetSelection(result);
        else {
            m_FileHistory->AddFileToHistory(File[i]);
            NewSTCPage(File[i], true);
        }
    }
}

void FBIdeMainFrame::OnFileHistory(wxCommandEvent &event) {
    wxString file = m_FileHistory->GetHistoryFile(event.GetId() - wxID_FILE1);
    if (::wxFileExists(file)) {
        int result = bufferList.FileLoaded(file);
        if (result != -1)
            FBNotebook->SetSelection(result);
        else
            NewSTCPage(file, true);
    }
}

void FBIdeMainFrame::OnSave(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    int index = FBNotebook->GetSelection();
    if (SaveFile(bufferList[index]))
        SetModified(index, false);
}


void FBIdeMainFrame::OnSaveAs(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;

    int index = FBNotebook->GetSelection();
    Buffer *buff = bufferList[index];
    wxString OldName = buff->GetFileName();
    if (SaveFile(buff, true)) {
        if (OldName != buff->GetFileName())
            SetModified(index, false);
    }
}

void FBIdeMainFrame::OnSaveAll(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;

    int selectpage = FBNotebook->GetSelection();
    Buffer *buff;

    for (int index = 0; index < FBNotebook->GetPageCount(); index++) {
        buff = bufferList[index];
        if (buff->GetModified()) {
            FBNotebook->SetSelection(index);
            if (SaveFile(buff)) {
                SetModified(index, false);
            }
        }
    }

    FBNotebook->SetSelection(selectpage);
}

void FBIdeMainFrame::OnCloseAll_(wxCommandEvent & WXUNUSED(event)) {
    OnCloseAll();
}

void FBIdeMainFrame::OnCloseAll() {
    if (stc == 0)
        return;

    Buffer *buff;

    while (FBNotebook) {
        buff = bufferList[0];
        FBNotebook->SetSelection(0);
        if (buff->GetModified()) {
            int result = wxMessageBox(Lang[188] + buff->GetFileName() + Lang[189],
                                      Lang[184],
                                      wxYES_NO | wxCANCEL | wxICON_EXCLAMATION);
            if (result == wxCANCEL)
                return;
            else if (result == wxYES) {
                if (SaveFile(buff))
                    CloseFile(0);
                else
                    return;
            } else if (result == wxNO)
                CloseFile(0);
            bufferList.DecrModCount();
        } else
            CloseFile(0);
    }
    SetTitle("FBIde");
}


void FBIdeMainFrame::OnCloseFile_(wxCommandEvent & WXUNUSED(event)) {
    OnCloseFile();
}

void FBIdeMainFrame::OnCloseFile() {
    if (stc == 0)
        return;
    int index = FBNotebook->GetSelection();
    Buffer *buff = bufferList[index];

    if (buff->GetModified()) {
        wxString message = wxString::Format(_(Lang[190]),
                                            wxFileNameFromPath(buff->GetFileName()).c_str());

        int closeDialog = wxMessageBox(message, _(Lang[192]), //"File Modified"
                                       wxYES_NO | wxCANCEL | wxICON_EXCLAMATION, GetParent());

        if (closeDialog == wxYES)
            SaveFile(buff);

        else if (closeDialog == wxCANCEL)
            return;
        bufferList.DecrModCount();
    }

    CloseFile(index);
    if (bufferList.GetBufferCount() > 0)
        SetTitle("FBIde - " + bufferList[FBNotebook->GetSelection()]->GetFileName());
    else
        SetTitle("FBIde");
}


void FBIdeMainFrame::CloseFile(int index) {
    if (SFDialog && FBNotebook->GetPageCount() == 1) {
        SFDialog->Close(true);
    }
    if (FindDialog) {
        FindDialog->Close(true);
    }
    if (ReplaceDialog) {
        ReplaceDialog->Close(true);
    }
    if (formatDialog) {
        formatDialog->Close(true);
    }

    stc->SetBuffer((Buffer *) 0);
    stc->ClearAll();
    stc->ReleaseDocument(bufferList[index]->GetDocument());
    FBNotebook->DeletePage(index);
    bufferList.RemoveBuffer(index);
    if (bufferList.GetBufferCount() == 0) {
        delete stc;
        delete FBNotebook;
        FBCodePanel->SetSizer(NULL);
        stc = 0;
        FBNotebook = 0;
        m_TabStcSizer = 0;
        EnableMenus(false);
    } else {
        SetSTCPage(FBNotebook->GetSelection());
    }
}


void FBIdeMainFrame::OnQuit(wxCommandEvent &event) {
    Close(true);
}


void FBIdeMainFrame::OnNewWindow(wxCommandEvent &WXUNUSED(event)) {
    wxExecute(FB_App->argv[0]);
}


//----------------------------------------------------------------------------


bool FBIdeMainFrame::SaveFile(Buffer *buff, bool SaveAS) {
    wxString FileName = (SaveAS) ? "" : buff->GetFileName();

    int ft = buff->GetFileType();

    wxString Temp = (ft == 0) ? Lang[193] : Lang[200];
    Temp << Lang[194];

    if (FileName == "" || FileName == FBUNNAMED) {
        wxFileDialog dlg(this,
                         Lang[195],//Save file
                         "",
                         (ft == 0) ? ".bas" : ".html",
                         Temp,
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK)
            return false;
        FileName = dlg.GetPath();
        if (SaveAS) {
            if (wxMessageBox(Lang[196], Lang[197], wxICON_QUESTION | wxYES_NO | wxNO_DEFAULT) == wxYES)
                buff->SetFileName(FileName);
        } else {
            buff->SetFileName(FileName);
        }
    }

    stc->SaveFile(FileName);
    SetTitle("FBIde - " + bufferList[FBNotebook->GetSelection()]->GetFileName());
    return true;
}


void FBIdeMainFrame::OnSessionLoad(wxCommandEvent &event) {
    wxFileDialog dlg(this,
                     Lang[186], //Load file
                     "",
                     ".bas",
                     Lang[198], //FBIde Session
                     wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK)
        return;
    wxString File = dlg.GetPath();

    SessionLoad(File);
}

void FBIdeMainFrame::SessionLoad(wxString File) {
    wxTextFile TextFile(File);
    TextFile.Open();
    if (TextFile.GetLineCount() == 0)
        return;

    wxString Temp;
    int result = 0;
    unsigned long selectedtab = 0;

    Temp = TextFile[0];
    int ver = 1;
    if (Temp.Trim(false).Trim(true).Lower() == "<fbide:session:version = \"0.2\"/>")
        ver = 2;

    for (unsigned int i = ver; i < TextFile.GetLineCount(); i++) {
        Temp = TextFile[i];
        if (Temp != "" && ::wxFileExists(Temp)) {
            result = bufferList.FileLoaded(Temp);
            if (result == -1) {
                NewSTCPage(Temp, false);
                if (ver == 2) {
                    unsigned long t = 0;
                    i++;
                    Temp = TextFile[i];
                    Temp.ToULong(&t);
                    stc->ScrollToLine(t);

                    i++;
                    Temp = TextFile[i];
                    Temp.ToULong(&t);
                    stc->SetCurrentPos(t);
                    stc->SetSelectionStart(t);
                    stc->SetSelectionEnd(t);
                }
            }
        }

    }

    if (ver == 2)
        Temp = TextFile[1];
    else
        Temp = TextFile[0];

    Temp.ToULong(&selectedtab);

    FBNotebook->SetSelection(selectedtab);

    TextFile.Close();

    SetTitle("FBIde - " + bufferList[FBNotebook->GetSelection()]->GetFileName());
}

void FBIdeMainFrame::OnSessionSave(wxCommandEvent &event) {
    if (stc == 0)
        return;

    wxString FileName;

    wxFileDialog dlg(this,
                     Lang[199],
                     "",
                     ".fbs",
                     Lang[198],
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK)
        return;
    FileName = dlg.GetPath();

    wxTextFile TextFile(FileName);
    if (TextFile.Exists()) {
        TextFile.Open();
        TextFile.Clear();
    } else {
        TextFile.Create();
    }

    Buffer *buff;
    bool session = false;
    bool header = true;

    int SelectedTab = FBNotebook->GetSelection();
    TextFile.AddLine("<fbide:session:version = \"0.2\"/>");
    for (int i = 0; i < FBNotebook->GetPageCount(); i++) {
        buff = bufferList[i];
        if (buff->GetModified()) {
            FBNotebook->SetSelection(i);
            int result = wxMessageBox(Lang[188] + buff->GetFileName() + Lang[189],
                                      Lang[184],
                                      wxYES_NO | wxCANCEL | wxICON_EXCLAMATION);
            if (result == wxCANCEL)
                return;
            else if (result == wxYES) {
                SaveFile(buff);
                session = true;
                SetModified(i, false);
            } else if (result == wxNO) {
                if (buff->GetFileName() != FBUNNAMED) {
                    session = true;
                }
            }
        } else {
            session = true;
        }

        if (session && buff->GetFileName() != FBUNNAMED) {
            if (header) {
                header = false;
                wxString t;
                t << SelectedTab;
                TextFile.AddLine(t);
            }
            wxString Temp;
            session = false;
            TextFile.AddLine(buff->GetFileName());
            if (i == (int) FBNotebook->GetSelection())
                SaveDocumentStatus(i);
            Temp << buff->GetLine();
            TextFile.AddLine(Temp);
            Temp = "";
            Temp << buff->GetCaretPos();
            TextFile.AddLine(Temp);
        }
    }

    FBNotebook->SetSelection(SelectedTab);

    if (TextFile.GetLineCount()) {
        TextFile.Write();
    }

    TextFile.Close();
}
