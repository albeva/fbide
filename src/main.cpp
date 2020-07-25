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
#include "inc/fbedit.h"
#include <wx/image.h>
#include <wx/splash.h>
#include <wx/filename.h>
#include <wx/snglinst.h>


#include "wx/ipc.h"
#include "wx/clipbrd.h"


MyFrame *_myframe_;

class stConnection : public wxConnection {
public:
    stConnection() {}

    ~stConnection() {}

    bool OnExecute(const wxString &topic, wxChar *data, int size,
                   wxIPCFormat format);
};

class stServer : public wxServer {
public:
    wxConnectionBase *OnAcceptConnection(const wxString &topic);
};


class stClient : public wxClient {
public:
    stClient() {};

    wxConnectionBase *OnMakeConnection() {
        return new stConnection;
    }
};




// Accepts a connection from another instance

wxConnectionBase *stServer::OnAcceptConnection(const wxString &topic) {
    if (topic.Lower() == wxT("myapp")) {
        // Check that there are no modal dialogs active
        wxWindowList::Node *node = wxTopLevelWindows.GetFirst();
        while (node) {
            wxDialog *dialog = wxDynamicCast(node->GetData(), wxDialog);
            if (dialog && dialog->IsModal()) {
                return false;
            }

            node = node->GetNext();
        }
        return new stConnection();
    } else
        return NULL;
}



// Opens a file passed from another instance

bool stConnection::OnExecute(const wxString & WXUNUSED(topic),
                             wxChar *data,
                             int WXUNUSED(size),
                             wxIPCFormat WXUNUSED(format)) {
    wxString filename(data);
    wxFileName file(filename);
    if (!filename.IsEmpty()) {
        if (file.GetExt() == "fbs")
            _myframe_->SessionLoad(filename);
        else {
            int result = _myframe_->bufferList.FileLoaded(filename);
            if (result != -1)
                _myframe_->FBNotebook->SetSelection(result);
            else {
                if (::wxFileExists(filename)) {
                    _myframe_->NewSTCPage(filename, true);
                    _myframe_->m_FileHistory->AddFileToHistory(filename);
                }
            }

            _myframe_->SetFocus();
        }
    }
    return true;
}


stServer *m_server;

//------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(MyFrame, wxFrame)
        EVT_CLOSE (MyFrame::OnClose)
        EVT_MENU(Menu_Quit, MyFrame::OnQuit)
        EVT_MENU(Menu_About, MyFrame::OnAbout)
        EVT_MENU(Menu_New, MyFrame::OnNew)
        EVT_MENU(Menu_Open, MyFrame::OnOpen)
        EVT_MENU(Menu_Save, MyFrame::OnSave)
        EVT_MENU(Menu_SaveAS, MyFrame::OnSaveAs)
        EVT_MENU(Menu_SaveAll, MyFrame::OnSaveAll)
        EVT_MENU(Menu_SessionLoad, MyFrame::OnSessionLoad)
        EVT_MENU(Menu_SessionSave, MyFrame::OnSessionSave)
        EVT_MENU(Menu_Close, MyFrame::OnCloseFile_)
        EVT_MENU(Menu_CloseAll, MyFrame::OnCloseAll_)
        EVT_MENU(Menu_NewEditor, MyFrame::OnNewWindow)

        EVT_MENU(Menu_Undo, MyFrame::OnMenuUndo)
        EVT_MENU(Menu_Redo, MyFrame::OnMenuRedo)
        EVT_MENU(Menu_Cut, MyFrame::OnMenuCut)
        EVT_MENU(Menu_Copy, MyFrame::OnMenuCopy)
        EVT_MENU(Menu_Paste, MyFrame::OnMenuPaste)
        EVT_MENU(Menu_SelectAll, MyFrame::OnSelectAll)
        EVT_MENU(Menu_SelectLine, MyFrame::OnSelectLine)
        EVT_MENU(Menu_IndentIncrease, MyFrame::OnIndentInc)
        EVT_MENU(Menu_IndentDecrease, MyFrame::OnIndentDecr)
        EVT_MENU(Menu_Comment, MyFrame::OnComment)
        EVT_MENU(Menu_UnComment, MyFrame::OnUncomment)

        EVT_MENU(Menu_Find, MyFrame::OnFind)
        EVT_MENU(Menu_Replace, MyFrame::OnReplace)
        EVT_MENU(Menu_FindNext, MyFrame::OnFindAgain)
        EVT_MENU(Menu_GotoLine, MyFrame::OnGotoLine)
        EVT_FIND(-1, MyFrame::FindButton)
        EVT_FIND_CLOSE(-1, MyFrame::FindClose)
        EVT_FIND_NEXT(-1, MyFrame::MenuFindNext)
        EVT_FIND_REPLACE(-1, MyFrame::ReplaceSel)
        EVT_FIND_REPLACE_ALL(-1, MyFrame::MenuReplaceAll)

        EVT_MENU(Menu_Settings, MyFrame::OnSettings)
        EVT_MENU(Menu_Format, MyFrame::OnFormat)
        EVT_MENU(Menu_Result, MyFrame::OnResult)
        EVT_MENU(Menu_Subs, MyFrame::OnSubs)
        EVT_MENU(Menu_CompilerLog, MyFrame::OnCompilerLog)

        EVT_MENU(Menu_Compile, MyFrame::OnCompile)
        EVT_MENU(Menu_CompileAndRun, MyFrame::OnCompileAndRun)
        EVT_MENU(Menu_Run, MyFrame::OnRun)
        EVT_MENU(Menu_QuickRun, MyFrame::OnQuickRun)
        EVT_MENU(Menu_CmdPromt, MyFrame::OnCmdPromt)
        EVT_MENU(Menu_Parameters, MyFrame::OnParameters)
        EVT_MENU(Menu_ShowExitCode, MyFrame::OnShowExitCode)
        EVT_MENU(Menu_ActivePath, MyFrame::OnActivePath)

        EVT_MENU(Menu_Help, MyFrame::OnHelp)
        EVT_MENU(Menu_QuickKeys, MyFrame::OnQuickKeys)
        EVT_MENU(Menu_ReadMe, MyFrame::OnReadMe)
        EVT_MENU(Menu_Fpp, MyFrame::OnFpp)

        EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, MyFrame::OnFileHistory)

        EVT_TABBEDCTRL_PAGE_CHANGED(-1, MyFrame::ChangeNBPage)

        EVT_LIST_ITEM_ACTIVATED(-1, MyFrame::OnGoToError)
        EVT_LIST_ITEM_RIGHT_CLICK(-1, MyFrame::OnConsoMouseleLeft)
        EVT_LIST_COL_RIGHT_CLICK(-1, MyFrame::OnConsoMouseleLeft)

END_EVENT_TABLE()

wxIMPLEMENT_APP(MyApp);

//------------------------------------------------------------------------------

void LogFBIdeMessage(const wxString& message) {
    static wxLogWindow* _theLogWindow = nullptr;
    if (_theLogWindow == nullptr) {
        auto top = wxGetApp().GetTopWindow();
        while (top->GetParent() != nullptr) {
            top = top->GetParent();
        }
        _theLogWindow = new wxLogWindow(top, "Log");
        _theLogWindow->GetFrame()->Bind(wxEVT_CLOSE_WINDOW, [](wxCloseEvent&){
            _theLogWindow->GetFrame()->Destroy();
            _theLogWindow = nullptr;
            wxLog::SetActiveTarget(nullptr);
        });

        wxLog::SetActiveTarget(_theLogWindow);
        _theLogWindow->PassMessages(false);
    }
    wxLogMessage(message);
}

wxSingleInstanceChecker *m_singleInstanceChecker;

bool MyApp::OnInit() {

    SetVendorName("FBIde");
    SetAppName("FBIde");

    m_singleInstanceChecker = new wxSingleInstanceChecker("MyApp");

    // If using a single instance, use IPC to
    // communicate with the other instance
    if (!m_singleInstanceChecker->IsAnotherRunning()) {
        // Create a new server
        m_server = new stServer;

        if (!m_server->Create("myapp")) {
            wxMessageBox("Failed to create an IPC service.");
        }
    } else {
        if (argc > 1) {
            wxString cmdFilename = argv[1];
            wxLogNull logNull;

            // OK, there IS another one running, so try to connect to it
            // and send it any filename before exiting.
            stClient *client = new stClient;

            // ignored under DDE, host name in TCP/IP based classes
            wxString hostName = "localhost";

            // Create the connection
            wxConnectionBase *connection =
                client->MakeConnection(hostName,
                                       "myapp", "MyApp");

            if (connection) {
                // Ask the other instance to open a file or raise itself
                connection->Execute(cmdFilename);
                connection->Disconnect();
                delete connection;
            } else {
                wxMessageBox(
                    "Sorry, the existing instance may be too busy too respond.\nPlease close any open dialogs and retry.",
                    "My application", wxICON_INFORMATION | wxOK);
            }
            delete client;
            return false;
        }
        delete m_singleInstanceChecker;
    }


    _myframe_ = new MyFrame(this, GetAppName());

    return true;
}


//------------------------------------------------------------------------------
MyFrame::MyFrame(MyApp *App, const wxString &title)
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

void MyFrame::OnClose(wxCloseEvent &event) {

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
    if (::m_server)
        delete m_server;
    event.Skip();
}

wxColour GetClr(uint color) {
    wxColour clr((color >> 16) & 255, (color >> 8) & 255, color & 255);
    return clr;
}

