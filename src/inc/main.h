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
#pragma once
#include "pch.h"
#include "buffer.h"
#include "bufferlist.h"

class FB_Edit;
class SFBrowser;
class FormatDialog;
class wxMyNotebook;
class wxTabbedCtrlEvent;

#define KWGROUPS        4
#define mySTC_STYLE_BOLD  1
#define mySTC_STYLE_ITALIC  2
#define mySTC_STYLE_UNDERL  4
#define mySTC_STYLE_HIDDEN  8

typedef unsigned int uint;

struct CommonInfo {
    bool SyntaxHighlight;
    bool IndentGuide;
    bool DisplayEOL;
    bool LineNumber;
    bool LongLine;
    bool whiteSpace;
    bool AutoIndent;
    bool BraceHighlight;
    bool ShowExitCode;
    bool FolderMargin;
    bool FloatBars;
    bool CurrentLine;
    int TabSize;
    int EdgeColumn;
    wxString Language;
    wxString HelpFile;
    bool SplashScreen;
    bool UseHelp;
    bool ActivePath;
};


typedef unsigned int uint;

struct StyleInfo {
    uint DefaultBgColour;
    uint DefaultFgColour;
    wxString DefaultFont;
    int DefaultFontSize;
    int DefaultFontStyle;

    uint LineNumberBgColour;
    uint LineNumberFgColour;
    uint SelectBgColour;
    uint SelectFgColour;
    uint CaretColour;

    uint BraceFgColour;
    uint BraceBgColour;
    int BraceFontStyle;

    uint BadBraceFgColour;
    uint BadBraceBgColour;
    int BadBraceFontStyle;

    uint MarginBgColour;
    uint MarginFgColour;
    int MarginSize;

    uint CaretLine;

    struct Info {
        uint foreground;
        uint background;
        wxString fontname;
        int fontsize;
        int fontstyle;
        int lettercase;

        Info() {};

        Info(const Info &i) {
            foreground = i.foreground;
            background = i.background;
            fontname = i.fontname;
            fontsize = i.fontsize;
            lettercase = i.lettercase;

        }
    }
        Info[16];

    StyleInfo() {};

    StyleInfo(const StyleInfo &si) {
        DefaultBgColour = si.DefaultBgColour;
        DefaultFgColour = si.DefaultFgColour;
        DefaultFont = si.DefaultFont;
        DefaultFontSize = si.DefaultFontSize;
        DefaultFontStyle = si.DefaultFontStyle;

        LineNumberBgColour = si.LineNumberBgColour;
        LineNumberFgColour = si.LineNumberFgColour;
        SelectBgColour = si.SelectBgColour;
        SelectFgColour = si.SelectFgColour;
        CaretColour = si.CaretColour;

        BraceFgColour = si.BraceFgColour;
        BraceBgColour = si.BraceBgColour;
        BraceFontStyle = si.BraceFontStyle;

        BadBraceFgColour = si.BadBraceFgColour;
        BadBraceBgColour = si.BadBraceBgColour;
        BadBraceFontStyle = si.BadBraceFontStyle;

        MarginBgColour = si.MarginBgColour;
        MarginFgColour = si.MarginFgColour;
        MarginSize = si.MarginSize;
        CaretLine = si.CaretLine;

        for (int i = 1; i < 15; i++)
            Info[i] = si.Info[i];
    }

};


wxColour GetClr(uint color);

class MyApp : public wxApp {
public:
    virtual bool OnInit();
};

wxDECLARE_APP(MyApp);
void LogFBIdeMessage(const wxString& message);


class MyFrame : public wxFrame {
public:
    MyFrame(MyApp *App, const wxString &title);

    void OnClose(wxCloseEvent &event);

    void LoadSettings();

    void SaveSettings();

    StyleInfo LoadThemeFile(wxString ThemeFile);

    void SaveThemeFile(StyleInfo Style, wxString ThemeFile);

    void LoadkwFile(wxString SyntaxFile);

    void SavekwFile(wxString SyntaxFile);

    void LoadUI();

    void LoadMenu();

    void LoadToolBar();

    void LoadStatusBar();

    void OpenLangFile(wxString FileName);

    void SaveDocumentStatus(int docID);

    void SetSTCPage(int index);

    void SetModified(int index, bool status);

    void AddListItem(int Linenr, int ErrorNr, wxString FileName, wxString Message);

    void OnGoToError(wxListEvent &event);

    void OnConsoMouseleLeft(wxListEvent &event);

    void GoToError(int Linenr, wxString FileName);

    void EnableMenus(bool state);

    void OnMouseEvent(wxMouseEvent &event);


    //FileMenu-event and related stuff
    void OnNew(wxCommandEvent &event);

    void OnOpen(wxCommandEvent &event);

    void OnSave(wxCommandEvent &event);

    void OnSaveAs(wxCommandEvent &event);

    void OnSaveAll(wxCommandEvent &event);

    void OnSessionLoad(wxCommandEvent &event);

    void OnSessionSave(wxCommandEvent &event);

    void OnCloseFile_(wxCommandEvent &event);

    void OnCloseFile();

    void OnCloseAll_(wxCommandEvent &event);

    void OnCloseAll();

    void OnQuit(wxCommandEvent &event);

    void OnNewWindow(wxCommandEvent &event);

    bool SaveFile(Buffer *buff, bool SaveAS = false);

    void CloseFile(int index);

    int Proceed(void);

    void SessionLoad(wxString File);

    void OnFileHistory(wxCommandEvent &event);


    //Editmenu-events and related stuff
    void OnMenuUndo(wxCommandEvent &event);

    void OnMenuRedo(wxCommandEvent &event);

    void OnMenuCut(wxCommandEvent &event);

    void OnMenuCopy(wxCommandEvent &event);

    void OnMenuPaste(wxCommandEvent &event);

    void OnSelectAll(wxCommandEvent &event);

    void OnSelectLine(wxCommandEvent &event);

    void OnIndentInc(wxCommandEvent &event);

    void OnIndentDecr(wxCommandEvent &event);

    void OnComment(wxCommandEvent &event);

    void OnUncomment(wxCommandEvent &event);

    //Searchmenu and related stuff
    void OnFind(wxCommandEvent &event);

    void OnReplace(wxCommandEvent &event);

    void OnFindAgain(wxCommandEvent &event);

    void OnGotoLine(wxCommandEvent &event);

    void FindButton(wxFindDialogEvent &event);

    void FindClose(wxFindDialogEvent &event);

    void MenuFindNext(wxFindDialogEvent &event);

    void ReplaceSel(wxFindDialogEvent &event);

    void MenuReplaceAll(wxFindDialogEvent &event);

    bool HasSelection();

    bool HasText();

    bool FindCurrentWord(int direc);

    void ReplaceCurrentWord(const wxString &text);

    void Replace(const wxString &findStr, const wxString &replaceStr, int flags);

    void ReplaceAll(const wxString &findStr, const wxString &replaceStr, int flags);

    void ReplaceText(int from, int to, const wxString &value);

    bool FindOccurence(const wxString &findStr, int direc, int flags = 0);

    bool FindNext();

    bool FindPrevious();

    void FindNextWord(wxCommandEvent &event);

    void FindPreviousWord(wxCommandEvent &event);

    wxString GetTextUnderCursor();

    wxString GetTextUnderCursor(int &startPos, int &endPos);


    //View menu stuff
    void OnSettings(wxCommandEvent &event);

    void OnFormat(wxCommandEvent &event);

    void OnResult(wxCommandEvent &event);

    void OnSubs(wxCommandEvent &event);

    void OnCompilerLog(wxCommandEvent &event);

    void CompilerLog();

    //Run menu stuff
    void OnCompile(wxCommandEvent &event);

    void OnCompileAndRun(wxCommandEvent &event);

    void OnRun(wxCommandEvent &event);

    void OnQuickRun(wxCommandEvent &event);

    void OnCmdPromt(wxCommandEvent &event);

    void OnParameters(wxCommandEvent &event);

    void OnShowExitCode(wxCommandEvent &event);

    int Compile(int index);

    void Run(wxFileName file);

    wxString GetCompileData(int index);

    void OnActivePath(wxCommandEvent &event);

    void OnGetFocus(wxFocusEvent &event);


    wxFindReplaceData *FindData;
    wxFindReplaceData *ReplaceData;
    wxFindReplaceDialog *FindDialog;
    wxFindReplaceDialog *ReplaceDialog;
    SFBrowser *SFDialog;
    FormatDialog *formatDialog;
    wxString findText;
    wxString replaceText;
    wxString findString;
    wxArrayString kwList;
    int FindFlags;


    void OnAbout(wxCommandEvent &event);

    void OnHelp(wxCommandEvent &event);

    void OnQuickKeys(wxCommandEvent &event);

    void OnReadMe(wxCommandEvent &event);

    void OnFpp(wxCommandEvent &event);

    void NewSTCPage(wxString InitFile, bool select = false, int FileType = 0);

    void ChangeNBPage(wxTabbedCtrlEvent &event);

    //Pointers
    MyApp *FB_App;
    FB_Edit *stc;
    wxToolBar *FB_Toolbar;
    wxMyNotebook *FBNotebook;
    wxPanel *FBCodePanel;
    wxPanel *FBConsolePanel;
    wxSplitterWindow *HSplitter;
    wxListCtrl *FBConsole;

    wxMenu *HelpMenu;
    wxMenu *FB_Run;
    wxMenu *FB_Tools;
    wxMenu *FB_View;
    wxMenu *FB_Search;
    wxMenu *_FB_Edit;
    wxMenu *FB_File;
    wxMenuBar *MenuBar;

    bool IDE_Modified;
    int braceLoc;
    int ConsoleSize;
    bool ProcessIsRunning;
    bool IsTemp;
    bool InitState;

    wxString CompilerPath, SyntaxFile, CMDPrototype, ThemeFile, RunPrototype, strTerminal;
    wxString Document, CompiledFile, EditorPath, ParameterList;
    wxString CurFolder;
    CommonInfo Prefs;
    StyleInfo Style;
    wxArrayString Lang;

    wxString Keyword[KWGROUPS + 1];

    wxColourData colr;
    BufferList bufferList;
    int lastTabCreated;
    int OldTabSelected;
    int CurrentFileType;
#ifdef __WXMSW__
    wxCHMHelpController help;
#endif


    wxFileHistory *m_FileHistory;
    wxBoxSizer *m_TabStcSizer;

    wxArrayString strCompilerOutput;


private:
DECLARE_EVENT_TABLE()
};

enum {
    Menu_Quit = wxID_EXIT,
    Menu_About = wxID_ABOUT,
    Menu_Help = wxID_HELP,

    //File-menu
    Menu_New = wxID_NEW,
    Menu_Open = wxID_OPEN,
    Menu_Save = wxID_SAVE,
    Menu_SaveAS = wxID_SAVEAS,
    Menu_Close = wxID_CLOSE,
    Menu_Undo = wxID_UNDO,
    Menu_Redo = wxID_REDO,
    Menu_Cut = wxID_CUT,
    Menu_Copy = wxID_COPY,
    Menu_Paste = wxID_PASTE,
    Menu_SelectAll = wxID_SELECTALL,
    Menu_Find = wxID_FIND,
    Menu_Replace = wxID_REPLACE,
    Menu_Settings = wxID_PROPERTIES,

    Menu_NewEditor = wxID_HIGHEST,
    Menu_SaveAll,
    Menu_FileHistory,
    Menu_SessionSave,
    Menu_SessionLoad,
    Menu_CloseAll,

    //EditMenu:
    Menu_SelectLine,
    Menu_IndentIncrease,
    Menu_IndentDecrease,
    Menu_Comment,
    Menu_UnComment,

    //Search:
    Menu_FindNext,
    Menu_GotoLine,
    Menu_FindPrevious,

    //ViewMenu:
    Menu_Format,
    Menu_Result,
    Menu_CompilerLog,

    //Tools menu
    Menu_Converter,
    Menu_Subs,

    //RunMenu
    Menu_Compile,
    Menu_CompileAndRun,
    Menu_Run,
    Menu_QuickRun,
    Menu_CmdPromt,
    Menu_Parameters,
    Menu_ShowExitCode,
    Menu_ActivePath,

    //Help menu
    Menu_QuickKeys,
    Menu_ReadMe,
    Menu_Fpp,
};
