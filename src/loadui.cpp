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

#ifndef __WXMSW__

#include "rc/bitmaps/open.xpm"
#include "rc/bitmaps/save.xpm"
#include "rc/bitmaps/cut.xpm"
#include "rc/bitmaps/copy.xpm"
#include "rc/bitmaps/paste.xpm"
#include "rc/bitmaps/undo.xpm"
#include "rc/bitmaps/redo.xpm"
#include "rc/bitmaps/compile.xpm"
#include "rc/bitmaps/run.xpm"
#include "rc/bitmaps/compnrun.xpm"
#include "rc/bitmaps/qrun.xpm"
#include "rc/bitmaps/saveall.xpm"
#include "rc/bitmaps/close.xpm"
#include "rc/bitmaps/output.xpm"
#include "rc/bitmaps/new.xpm"

#endif

#include <wx/filename.h>
#include "inc/main.h"
#include "inc/fbedit.h"
#include "inc/browser.h"


//------------------------------------------------------------------------------
//Load menu's
void MyFrame::LoadUI() {

    LoadToolBar();
    LoadMenu();
    LoadStatusBar();

    ConsoleSize = -100;

    FB_App->SetTopWindow(this);
    Freeze();

    HSplitter = new wxSplitterWindow(
        this, 10, wxDefaultPosition, wxDefaultSize,
        wxSP_3DSASH | wxNO_BORDER);
    HSplitter->SetSashGravity(1.0);
    HSplitter->SetMinimumPaneSize(100);

    FBConsole = new wxListCtrl(HSplitter,
                               wxID_ANY,
                               wxDefaultPosition,
                               wxDefaultSize,
                               wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    wxFont LbFont(10, wxMODERN, wxNORMAL, wxNORMAL, false);
    FBConsole->SetFont(LbFont);
    wxListItem itemCol;
    itemCol.SetText(Lang[165]); //"Line"
    itemCol.SetAlign(wxLIST_FORMAT_LEFT);
    FBConsole->InsertColumn(0, itemCol);
    itemCol.SetText(Lang[166]); //"File"
    itemCol.SetAlign(wxLIST_FORMAT_LEFT);
    FBConsole->InsertColumn(1, itemCol);
    itemCol.SetText(Lang[167]); //"Error nr"
    itemCol.SetAlign(wxLIST_FORMAT_LEFT);
    FBConsole->InsertColumn(2, itemCol);
    itemCol.SetText(Lang[161]); //"Messages"
    itemCol.SetAlign(wxLIST_FORMAT_LEFT);
    FBConsole->InsertColumn(3, itemCol);
    FBConsole->SetColumnWidth(0, 60);
    FBConsole->SetColumnWidth(1, 150);
    FBConsole->SetColumnWidth(2, 100);
    FBConsole->SetColumnWidth(3, 600);
    FBConsole->Hide();

    FBCodePanel = new wxPanel(HSplitter, wxID_ANY,
                              wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN);
    FBCodePanel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_APPWORKSPACE));
    HSplitter->Initialize(FBCodePanel);

    Thaw();
    SendSizeEvent();
    stc = 0;
    EnableMenus(false);
    return;

}


void MyFrame::LoadMenu() {

    MenuBar = new wxMenuBar();

    //File
    FB_File = new wxMenu;
    //wxMenu * file_history = new wxMenu;
    m_FileHistory->UseMenu(FB_File);

    FB_File->Append(Menu_New, Lang[11] + "\tCtrl+N", Lang[12]);
    FB_File->Append(Menu_Open, Lang[13] + "\tCtrl+O", Lang[14]);
    //FB_File->Append (Menu_FileHistory, _T(Lang[13]) + "...", file_history );

    FB_File->AppendSeparator();
    FB_File->Append(Menu_Save, Lang[15] + "\tCtrl+S", Lang[16]);
    FB_File->Append(Menu_SaveAS, Lang[17] + "\tCtrl+Shift+S", Lang[18]);
    FB_File->Append(Menu_SaveAll, Lang[19], Lang[20]);

    FB_File->AppendSeparator();
    FB_File->Append(Menu_SessionLoad, Lang[169], Lang[170]);
    FB_File->Append(Menu_SessionSave, Lang[171], Lang[172]);

    FB_File->AppendSeparator();
    FB_File->Append(Menu_Close, Lang[21] + "\tCtrl+F4", Lang[22]);
    FB_File->Append(Menu_CloseAll, Lang[173], Lang[174]);

    FB_File->AppendSeparator();
    FB_File->Append(Menu_NewEditor, Lang[23] + "\tShift+Ctrl+N", Lang[24]);
    FB_File->Append(Menu_Quit, Lang[25] + "\tCtrl+Q", Lang[26]);



    // Edit menu
    _FB_Edit = new wxMenu;
    _FB_Edit->Append(Menu_Undo, Lang[27] + "\tCtrl+Z", Lang[28]);
    _FB_Edit->Append(Menu_Redo, Lang[29] + "\tCtrl+Shift+Z", Lang[30]);
    _FB_Edit->AppendSeparator();

    _FB_Edit->Append(Menu_Cut, Lang[31] + "\tCtrl+X", Lang[32]);
    _FB_Edit->Append(Menu_Copy, Lang[33] + "\tCtrl+C", Lang[34]);
    _FB_Edit->Append(Menu_Paste, Lang[35] + "\tCtrl+V", Lang[36]);
    _FB_Edit->AppendSeparator();

    _FB_Edit->Append(Menu_SelectAll, Lang[37] + "\tCtrl+A", Lang[38]);
    _FB_Edit->Append(Menu_SelectLine, Lang[39] + "\tCtrl+L", Lang[40]);
    _FB_Edit->AppendSeparator();

    _FB_Edit->Append(Menu_IndentIncrease, Lang[41] + "\tTab", Lang[42]);
    _FB_Edit->Append(Menu_IndentDecrease, Lang[43] + "\tShift+Tab", Lang[44]);

    _FB_Edit->AppendSeparator();
    _FB_Edit->Append(Menu_Comment, Lang[45] + "\tCtrl+M", Lang[46]);
    _FB_Edit->Append(Menu_UnComment, Lang[47] + "\tCtrl+Shift+M", Lang[48]);


    // Search menu
    FB_Search = new wxMenu;
    FB_Search->Append(Menu_Find, Lang[49] + "\tCtrl+F", Lang[50]);
    FB_Search->Append(Menu_FindNext, Lang[51] + "\tF3", Lang[52]);
    FB_Search->Append(Menu_Replace, Lang[53] + "\tCtrl+R", Lang[54]);
    FB_Search->Append(Menu_GotoLine, Lang[55] + "\tCtrl+G", Lang[56]);



    // View menu
    FB_View = new wxMenu;
    FB_View->Append(Menu_Settings, Lang[57], Lang[58]);
    FB_View->Append(Menu_Format, Lang[175], Lang[176]);
    FB_View->AppendCheckItem(Menu_Result, Lang[59] + "\tF4", Lang[60]);
    FB_View->Append(Menu_Subs, Lang[61] + "\tF2", Lang[62]);
    FB_View->Append(Menu_CompilerLog, Lang[236], Lang[237]);
    //FB_Tools->Append (Menu_Converter, _(Language.ToolsConverter), _(Language.ToolsConverterDesc));


    //Run menu
    FB_Run = new wxMenu;
    FB_Run->Append(Menu_Compile, Lang[63] + "\tCtrl+F9", Lang[64]);
    FB_Run->Append(Menu_CompileAndRun, Lang[65] + "\tF9", Lang[66]);
    FB_Run->Append(Menu_Run, Lang[67] + "\tShift+Ctrl+F9", Lang[68]);
    FB_Run->Append(Menu_QuickRun, Lang[69] + "\tF5", Lang[70]);
    FB_Run->Append(Menu_CmdPromt, Lang[71] + "\tF8", Lang[72]);
    FB_Run->Append(Menu_Parameters, Lang[73], Lang[74]);
    FB_Run->AppendCheckItem(Menu_ShowExitCode, Lang[77], Lang[78]);
    FB_Run->Check(Menu_ShowExitCode, Prefs.ShowExitCode);

    FB_Run->AppendCheckItem(Menu_ActivePath, Lang[234], Lang[235]);
    FB_Run->Check(Menu_ActivePath, Prefs.ActivePath);


    //Help
    HelpMenu = new wxMenu;
    HelpMenu->Append(Menu_Help, Lang[10] + "\tF1");
    if (!Prefs.UseHelp) HelpMenu->Enable(Menu_Help, false);
    HelpMenu->Append(Menu_QuickKeys, "QuickKeys.txt");
    HelpMenu->Append(Menu_ReadMe, "ReadMe.txt");
    //HelpMenu->Append(Menu_Fpp, _T("Fpp.txt") );
    HelpMenu->AppendSeparator();
    HelpMenu->Append(Menu_About, Lang[79], Lang[80]);


    //Implement menus
    MenuBar->Append(FB_File, Lang[4]);
    MenuBar->Append(_FB_Edit, Lang[5]);
    MenuBar->Append(FB_Search, Lang[6]);
    MenuBar->Append(FB_View, Lang[7]);
    MenuBar->Append(FB_Run, Lang[9]);
    MenuBar->Append(HelpMenu, Lang[10]);
    SetMenuBar(MenuBar);

    return;
}


//------------------------------------------------------------------------------
// Load toolbar
void MyFrame::LoadToolBar() {

    // FB_Toolbar = GetToolBar();
    FB_Toolbar = CreateToolBar(wxNO_BORDER | wxTB_HORIZONTAL | wxTB_DOCKABLE | wxTB_FLAT);

    // Add controls:
    wxBitmap toolBarBitmaps[15];
    toolBarBitmaps[0] = wxBITMAP(new);
    toolBarBitmaps[1] = wxBITMAP(open);
    toolBarBitmaps[2] = wxBITMAP(save);
    toolBarBitmaps[3] = wxBITMAP(cut);
    toolBarBitmaps[4] = wxBITMAP(copy);
    toolBarBitmaps[5] = wxBITMAP(paste);
    toolBarBitmaps[6] = wxBITMAP(undo);
    toolBarBitmaps[7] = wxBITMAP(redo);
    toolBarBitmaps[8] = wxBITMAP(compile);
    toolBarBitmaps[9] = wxBITMAP(run);
    toolBarBitmaps[10] = wxBITMAP(compnrun);
    toolBarBitmaps[11] = wxBITMAP(qrun);
    toolBarBitmaps[12] = wxBITMAP(saveall);
    toolBarBitmaps[13] = wxBITMAP(close);
    toolBarBitmaps[14] = wxBITMAP(output);


    FB_Toolbar->AddTool(Menu_New, Lang[83], toolBarBitmaps[0]);
    FB_Toolbar->AddTool(Menu_Open, Lang[84], toolBarBitmaps[1]);
    FB_Toolbar->AddTool(Menu_Save, Lang[85], toolBarBitmaps[2]);
    FB_Toolbar->AddTool(Menu_SaveAll, Lang[86], toolBarBitmaps[12]);
    FB_Toolbar->AddTool(Menu_Close, Lang[87], toolBarBitmaps[13]);
    FB_Toolbar->AddSeparator();
    FB_Toolbar->AddTool(Menu_Cut, Lang[88], toolBarBitmaps[3]);
    FB_Toolbar->AddTool(Menu_Copy, Lang[89], toolBarBitmaps[4]);
    FB_Toolbar->AddTool(Menu_Paste, Lang[90], toolBarBitmaps[5]);
    FB_Toolbar->AddSeparator();
    FB_Toolbar->AddTool(Menu_Undo, Lang[91], toolBarBitmaps[6]);
    FB_Toolbar->AddTool(Menu_Redo, Lang[92], toolBarBitmaps[7]);
    FB_Toolbar->AddSeparator();
    FB_Toolbar->AddTool(Menu_Compile, Lang[93], toolBarBitmaps[8]);
    FB_Toolbar->AddTool(Menu_Run, Lang[94], toolBarBitmaps[9]);
    FB_Toolbar->AddTool(Menu_CompileAndRun, Lang[95], toolBarBitmaps[10]);
    FB_Toolbar->AddTool(Menu_QuickRun, Lang[96], toolBarBitmaps[11]);
    FB_Toolbar->AddTool(Menu_Result, Lang[97], toolBarBitmaps[14]);

    FB_Toolbar->Realize();

    return;
}

void MyFrame::EnableMenus(bool state) {

    int arrMenus[] = {
        Menu_Undo, Menu_Redo, Menu_Cut, Menu_Copy, Menu_Paste,
        Menu_SelectAll, Menu_Find, Menu_Replace, Menu_Save, Menu_SaveAll,
        Menu_SaveAS, Menu_Close, Menu_SessionSave, Menu_CloseAll, Menu_SelectLine,
        Menu_IndentIncrease, Menu_IndentDecrease, Menu_Comment, Menu_UnComment, Menu_FindNext,
        Menu_GotoLine, Menu_Format, Menu_Subs};

    for (int idx = 0; idx < 23; idx++) {
        MenuBar->Enable(arrMenus[idx], state);
    }

    if (!ProcessIsRunning) {
        MenuBar->Enable(Menu_Compile, state);
        MenuBar->Enable(Menu_Run, state);
        MenuBar->Enable(Menu_CompileAndRun, state);
        MenuBar->Enable(Menu_QuickRun, state);
    }

    FB_Toolbar->EnableTool(Menu_Save, state);
    FB_Toolbar->EnableTool(Menu_SaveAll, state);
    FB_Toolbar->EnableTool(Menu_Close, state);
    FB_Toolbar->EnableTool(Menu_Cut, state);
    FB_Toolbar->EnableTool(Menu_Copy, state);
    FB_Toolbar->EnableTool(Menu_Paste, state);
    FB_Toolbar->EnableTool(Menu_Undo, state);
    FB_Toolbar->EnableTool(Menu_Redo, state);
    if (!ProcessIsRunning) {
        FB_Toolbar->EnableTool(Menu_Compile, state);
        FB_Toolbar->EnableTool(Menu_Run, state);
        FB_Toolbar->EnableTool(Menu_CompileAndRun, state);
        FB_Toolbar->EnableTool(Menu_QuickRun, state);
    }

    return;
}

//------------------------------------------------------------------------------
// Load Statusbar
void MyFrame::LoadStatusBar() {
    CreateStatusBar(2);
    SetStatusText(Lang[1]);
    return;
}


void MyFrame::NewSTCPage(wxString InitFile, bool select, int FileType) {

    void *doc;
    if (InitFile == "") InitFile = FBUNNAMED;
    Buffer *buff;

    wxFileName File(InitFile);

    if (File.GetExt() == "html" || File.GetExt() == "htm") { FileType = 1; }
    else if (File.GetExt() == "txt") { FileType = 2; }

    if (stc == NULL) {
        Freeze();
        EnableMenus(true);
        OldTabSelected = -1;

        m_TabStcSizer = new wxBoxSizer(wxVERTICAL);

        FBNotebook = new wxMyNotebook(this, FBCodePanel, wxID_ANY, wxDefaultPosition,
                                      wxDefaultSize, wxSTATIC_BORDER | wxTB_TOP | wxTB_X);

        stc = new FB_Edit(this, FBCodePanel, -1, "");
        m_TabStcSizer->Add(FBNotebook, 0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND | wxALIGN_TOP, 0);
        m_TabStcSizer->Add(stc, 1, wxBOTTOM | wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 0);
        FBCodePanel->SetSizer(m_TabStcSizer);
        FBCodePanel->Layout();

        CurrentFileType = FileType;
        stc->LoadSTCTheme(CurrentFileType);
        stc->LoadSTCSettings();
        stc->StyleClearAll();
        stc->LoadSTCTheme(CurrentFileType);
        stc->LoadSTCSettings();
        buff = bufferList.AddFileBuffer("", "");
        buff->SetFileType(FileType);
        doc = stc->GetDocPointer();
        stc->AddRefDocument(doc);
        stc->SetDocPointer(doc);
        if (InitFile != FBUNNAMED) stc->LoadFile(InitFile);
        FBNotebook->AddPage(wxFileNameFromPath(InitFile), true);

        SendSizeEvent();
        Thaw();
    } else {
        stc->SetBuffer((Buffer *) 0);
        buff = bufferList.AddFileBuffer("", "");
        buff->SetFileType(FileType);
        SaveDocumentStatus(FBNotebook->GetSelection());
        doc = stc->CreateDocument();
        stc->AddRefDocument(doc);
        stc->SetDocPointer(doc);
        OldTabSelected = -1;
        if (InitFile != FBUNNAMED) stc->LoadFile(InitFile);
        FBNotebook->AddPage(wxFileNameFromPath(InitFile), true);

        CurrentFileType = FileType;
        stc->LoadSTCTheme(CurrentFileType);

        stc->LoadSTCSettings();
    }

    buff->SetFileName(InitFile);
    buff->SetModified(false);
    buff->UpdateModTime();
    buff->SetDocument(doc);
    stc->SetBuffer((Buffer *) buff);
    stc->SetFocus();

    if (SFDialog) SFDialog->Rebuild();
    if (select) {
        SetTitle("FBIde - " + InitFile);
    }

    return;
}


void MyFrame::ChangeNBPage(wxTabbedCtrlEvent &event) {
    if (OldTabSelected == -1) {
        OldTabSelected = 0;
        return;
    }
    if (stc == 0) return;

    int index = event.GetSelection();
    if (FBNotebook->GetPageCount() > 1) SaveDocumentStatus(event.GetOldSelection());
    SetSTCPage(index);
    SetTitle("FBIde - " + bufferList[index]->GetFileName());
    return;
}


void MyFrame::SaveDocumentStatus(int docID) {
    Buffer *buff = bufferList.GetBuffer(docID);
    buff->SetPositions(stc->GetSelectionStart(), stc->GetSelectionEnd());
    buff->SetLine(stc->GetFirstVisibleLine());
    buff->SetCaretPos(stc->GetCurrentPos());
}

void MyFrame::SetSTCPage(int index) {
    if (stc == 0) return;

    //stc->Freeze();

    stc->SetBuffer((Buffer *) 0);

    Buffer *buff = bufferList.GetBuffer(index);

    void *doc = buff->GetDocument();
    stc->AddRefDocument(doc);
    stc->SetDocPointer(doc);

    stc->ScrollToLine(buff->GetLine());
    stc->SetCurrentPos(buff->GetCaretPos());
    stc->SetSelectionStart(buff->GetSelectionStart());
    stc->SetSelectionEnd(buff->GetSelectionEnd());
    stc->SetFocus();
    stc->SetBuffer((Buffer *) buff);

    if (buff->GetFileType() != CurrentFileType) {
        CurrentFileType = buff->GetFileType();
        stc->LoadSTCTheme(CurrentFileType);
        stc->LoadSTCSettings();
    }

    //stc->Thaw();
    if (SFDialog) SFDialog->Rebuild();
}

void MyFrame::SetModified(int index, bool status) {

    if (index == -1) index = FBNotebook->GetSelection();

    Buffer *buff = bufferList.GetBuffer(index);

    buff->SetWasModified(buff->GetModified());
    wxString NewName;
    if (status) NewName << "[*] ";

    bufferList.SetBufferModified(index, status);
    NewName << wxFileNameFromPath(buff->GetFileName());
    FBNotebook->SetPageText(index, NewName);

}

