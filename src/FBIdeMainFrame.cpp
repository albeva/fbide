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
#include "inc/wxmynotebook.h"
#include "inc/FBIdeApp.h"

BEGIN_EVENT_TABLE(FBIdeMainFrame, wxFrame)
    EVT_CLOSE (FBIdeMainFrame::OnClose)
    EVT_MENU(Menu_Quit, FBIdeMainFrame::OnQuit)
    EVT_MENU(Menu_About, FBIdeMainFrame::OnAbout)
    EVT_MENU(Menu_New, FBIdeMainFrame::OnNew)
    EVT_MENU(Menu_Open, FBIdeMainFrame::OnOpen)
    EVT_MENU(Menu_Save, FBIdeMainFrame::OnSave)
    EVT_MENU(Menu_SaveAS, FBIdeMainFrame::OnSaveAs)
    EVT_MENU(Menu_SaveAll, FBIdeMainFrame::OnSaveAll)
    EVT_MENU(Menu_SessionLoad, FBIdeMainFrame::OnSessionLoad)
    EVT_MENU(Menu_SessionSave, FBIdeMainFrame::OnSessionSave)
    EVT_MENU(Menu_Close, FBIdeMainFrame::OnCloseFile_)
    EVT_MENU(Menu_CloseAll, FBIdeMainFrame::OnCloseAll_)
    EVT_MENU(Menu_NewEditor, FBIdeMainFrame::OnNewWindow)

    EVT_MENU(Menu_Undo, FBIdeMainFrame::OnMenuUndo)
    EVT_MENU(Menu_Redo, FBIdeMainFrame::OnMenuRedo)
    EVT_MENU(Menu_Cut, FBIdeMainFrame::OnMenuCut)
    EVT_MENU(Menu_Copy, FBIdeMainFrame::OnMenuCopy)
    EVT_MENU(Menu_Paste, FBIdeMainFrame::OnMenuPaste)
    EVT_MENU(Menu_SelectAll, FBIdeMainFrame::OnSelectAll)
    EVT_MENU(Menu_SelectLine, FBIdeMainFrame::OnSelectLine)
    EVT_MENU(Menu_IndentIncrease, FBIdeMainFrame::OnIndentInc)
    EVT_MENU(Menu_IndentDecrease, FBIdeMainFrame::OnIndentDecr)
    EVT_MENU(Menu_Comment, FBIdeMainFrame::OnComment)
    EVT_MENU(Menu_UnComment, FBIdeMainFrame::OnUncomment)

    EVT_MENU(Menu_Find, FBIdeMainFrame::OnFind)
    EVT_MENU(Menu_Replace, FBIdeMainFrame::OnReplace)
    EVT_MENU(Menu_FindNext, FBIdeMainFrame::OnFindAgain)
    EVT_MENU(Menu_GotoLine, FBIdeMainFrame::OnGotoLine)
    EVT_FIND(-1, FBIdeMainFrame::FindButton)
    EVT_FIND_CLOSE(-1, FBIdeMainFrame::FindClose)
    EVT_FIND_NEXT(-1, FBIdeMainFrame::MenuFindNext)
    EVT_FIND_REPLACE(-1, FBIdeMainFrame::ReplaceSel)
    EVT_FIND_REPLACE_ALL(-1, FBIdeMainFrame::MenuReplaceAll)

    EVT_MENU(Menu_Settings, FBIdeMainFrame::OnSettings)
    EVT_MENU(Menu_Format, FBIdeMainFrame::OnFormat)
    EVT_MENU(Menu_Result, FBIdeMainFrame::OnResult)
    EVT_MENU(Menu_Subs, FBIdeMainFrame::OnSubs)
    EVT_MENU(Menu_CompilerLog, FBIdeMainFrame::OnCompilerLog)

    EVT_MENU(Menu_Compile, FBIdeMainFrame::OnCompile)
    EVT_MENU(Menu_CompileAndRun, FBIdeMainFrame::OnCompileAndRun)
    EVT_MENU(Menu_Run, FBIdeMainFrame::OnRun)
    EVT_MENU(Menu_QuickRun, FBIdeMainFrame::OnQuickRun)
    EVT_MENU(Menu_CmdPromt, FBIdeMainFrame::OnCmdPromt)
    EVT_MENU(Menu_Parameters, FBIdeMainFrame::OnParameters)
    EVT_MENU(Menu_ShowExitCode, FBIdeMainFrame::OnShowExitCode)
    EVT_MENU(Menu_ActivePath, FBIdeMainFrame::OnActivePath)

    EVT_MENU(Menu_Help, FBIdeMainFrame::OnHelp)
    EVT_MENU(Menu_QuickKeys, FBIdeMainFrame::OnQuickKeys)
    EVT_MENU(Menu_ReadMe, FBIdeMainFrame::OnReadMe)
    EVT_MENU(Menu_Fpp, FBIdeMainFrame::OnFpp)

    EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, FBIdeMainFrame::OnFileHistory)

    EVT_TABBEDCTRL_PAGE_CHANGED(-1, FBIdeMainFrame::ChangeNBPage)

    EVT_LIST_ITEM_ACTIVATED(-1, FBIdeMainFrame::OnGoToError)
    EVT_LIST_ITEM_RIGHT_CLICK(-1, FBIdeMainFrame::OnConsoMouseleLeft)
    EVT_LIST_COL_RIGHT_CLICK(-1, FBIdeMainFrame::OnConsoMouseleLeft)
END_EVENT_TABLE()


FBIdeMainFrame::FBIdeMainFrame(FBIdeApp *App, const wxString &title)
    : wxFrame(0, wxID_ANY, title) {

    FB_App = App;
    LoadSettings();

    wxImage::AddHandler(new wxPNGHandler);

    wxBitmap bitmap;
    if (Prefs.SplashScreen && bitmap.LoadFile(this->EditorPath + "/IDE/splash.png", wxBITMAP_TYPE_PNG)) {
        new wxSplashScreen(bitmap,
                           wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_TIMEOUT,
                           1000, this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxSIMPLE_BORDER | wxSTAY_ON_TOP);
    }

    LoadkwFile(SyntaxFile);
    Style = LoadThemeFile(ThemeFile);
    ProcessIsRunning = false;
    IsTemp = false;
    InitState = false;

    FindData = new wxFindReplaceData(wxFR_DOWN);
    ReplaceData = new wxFindReplaceData(wxFR_DOWN);
    FindDialog = NULL;
    ReplaceDialog = NULL;
    SFDialog = NULL;
    formatDialog = NULL;
    FBNotebook = NULL;

    CurrentFileType = 0;
    LoadUI();
    m_FileHistory->AddFilesToMenu();

#ifdef __WXMSW__
    SetIcon(wxICON(fbicon));
#endif

    for (int i = 1; i < FB_App->argc; i++) {
        wxFileName File(FB_App->argv[i]);
        if (File.GetExt() == "fbs") {
            SessionLoad(FB_App->argv[i]);
        } else {
            if (FB_App->argc > 1) {
                if (::wxFileExists(FB_App->argv[i])) {
                    m_FileHistory->AddFileToHistory(FB_App->argv[i]);
                    NewSTCPage(FB_App->argv[i], true);
                    SetTitle("FBIde - " + bufferList[FBNotebook->GetSelection()]->GetFileName());
                }
            }
        }
    }

#ifdef __WXMSW__
    if (Prefs.UseHelp) {
        wxFileName helpFile(Prefs.HelpFile);
        if (helpFile.IsRelative())
            help.Initialize(EditorPath + "IDE/" + Prefs.HelpFile);
        else
            help.Initialize(Prefs.HelpFile);
    }
#endif

    Show();
}

void FBIdeMainFrame::OnClose(wxCloseEvent &event) {
    if (bufferList.GetModifiedCount()) {
        int result = wxMessageBox(Lang[230], Lang[231], wxYES_NO | wxCANCEL | wxICON_EXCLAMATION);
        if (result == wxCANCEL)
            return;
        if (result == wxYES) {
            OnCloseAll();
            if (stc)
                return;
        }
    }

    if (FBCodePanel)
        delete FBCodePanel;
    wxTheClipboard->Flush();
    SaveSettings();
    if (m_FileHistory)
        delete m_FileHistory;

    event.Skip();
}

wxColour GetClr(uint color) {
    wxColour clr((color >> 16) & 255, (color >> 8) & 255, color & 255);
    return clr;
}
