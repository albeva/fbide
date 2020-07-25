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
#include "main.h"

////@begin control identifiers
#define ID_DIALOG 10000
#define SYMBOL_ConfigDialog_STYLE wxCAPTION|wxSYSTEM_MENU|wxSTAY_ON_TOP|wxCLOSE_BOX|wxCLIP_CHILDREN
#define SYMBOL_ConfigDialog_TITLE _("FBIde settings")
#define SYMBOL_ConfigDialog_IDNAME ID_DIALOG
#define SYMBOL_ConfigDialog_SIZE wxDefaultSize
#define SYMBOL_ConfigDialog_POSITION wxDefaultPosition
#define ID_NOTEBOOK 10001
#define TabID_General 10002
#define ID_chkAutoIndent 10006
#define ID_chkIndentGuides 10007
#define ID_chkWhiteSpaces 10008
#define ID_chkLineEnd 10014
#define ID_chkMatchingBraces 10009
#define ID_spinRightMargin 10016
#define ID_chkSyntaxHighlight 10013
#define ID_chkLineNumbers 10012
#define ID_chkRightMargin 10010
#define ID_chkFoldMargin 10015
#define ID_chkSplashScreen 10011
#define ID_spinTabSize 10017
#define ID_chLanguage 10018
#define TabID_Themes 10003
#define ID_listThemeType 10019
#define ID_chTheme 10020
#define ID_btnSaveTheme 10021
#define ID_btnForeground 10022
#define ID_btnBackground 10023
#define ID_chFont 10024
#define ID_chkBold 10025
#define ID_chkItalic 10026
#define ID_chkUnderLined 10027
#define ID_spinFontSize 10028
#define TabID_Keywords 10004
#define ID_chKeywordGroup 10029
#define ID_textKeyWords 10030
#define TabID_Compiler 10005
#define ID_textCompilerPath 10031
#define ID_btnCompilerPath 10032
#define ID_textCompilerCommand 10033
#define ID_textRunCommand 10034
#define ID_textHelpFile 10035
#define ID_btnHelpFilePath 10036
////@end control identifiers

/*!
 * Compatibility
 */

#ifndef wxCLOSE_BOX
#define wxCLOSE_BOX 0x1000
#endif

/*!
 * ConfigDialog class declaration
 */

class ConfigDialog : public wxDialog {
DECLARE_DYNAMIC_CLASS(ConfigDialog)

DECLARE_EVENT_TABLE()

public:
    /// Constructors
    ConfigDialog();

    ConfigDialog(wxWindow *parent, wxWindowID id = SYMBOL_ConfigDialog_IDNAME,
                 const wxString &caption = SYMBOL_ConfigDialog_TITLE, const wxPoint &pos = SYMBOL_ConfigDialog_POSITION,
                 const wxSize &size = SYMBOL_ConfigDialog_SIZE, long style = SYMBOL_ConfigDialog_STYLE);

    /// Creation
    bool Create(wxWindow *parent, wxWindowID id = SYMBOL_ConfigDialog_IDNAME,
                const wxString &caption = SYMBOL_ConfigDialog_TITLE, const wxPoint &pos = SYMBOL_ConfigDialog_POSITION,
                const wxSize &size = SYMBOL_ConfigDialog_SIZE, long style = SYMBOL_ConfigDialog_STYLE);

    /// Creates the controls and sizers
    void CreateControls();

    ////@begin ConfigDialog event handler declarations

    /// wxEVT_COMMAND_LISTBOX_SELECTED event handler for ID_listThemeType
    void OnThemeSelectType(wxCommandEvent &event);

    /// wxEVT_COMMAND_CHOICE_SELECTED event handler for ID_chTheme
    void OnSelectTheme(wxCommandEvent &event);

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_btnSaveTheme
    void OnSaveTheme(wxCommandEvent &event);

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_btnForeground
    void OnBtnForeground(wxCommandEvent &event);

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_btnBackground
    void OnBtnBackground(wxCommandEvent &event);

    /// wxEVT_COMMAND_CHOICE_SELECTED event handler for ID_chKeywordGroup
    void OnKeywordsGroup(wxCommandEvent &event);

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_btnCompilerPath
    void OnCompilerPath(wxCommandEvent &event);

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_btnHelpFilePath
    void OnHelpPath(wxCommandEvent &event);

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_OK
    void OnOkClick(wxCommandEvent &event);

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_CANCEL
    void OnCancelClick(wxCommandEvent &event);

    ////@end ConfigDialog event handler declarations

    ////@begin ConfigDialog member function declarations

    bool GetMoGeneral() const {
        return m_MoGeneral;
    }

    void SetMoGeneral(bool value) {
        m_MoGeneral = value;
    }

    bool GetModTheme() const {
        return m_ModTheme;
    }

    void SetModTheme(bool value) {
        m_ModTheme = value;
    }

    bool GetModKeyWord() const {
        return m_ModKeyWord;
    }

    void SetModKeyWord(bool value) {
        m_ModKeyWord = value;
    }

    bool GetModCompiler() const {
        return m_ModCompiler;
    }

    void SetModCompiler(bool value) {
        m_ModCompiler = value;
    }

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource(const wxString &name);

    /// Retrieves icon resources
    wxIcon GetIconResource(const wxString &name);
    ////@end ConfigDialog member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

    ////@begin ConfigDialog member variables
    wxNotebook *objNoteBook;
    wxPanel *objPanelGeneral;
    wxCheckBox *chkAutoIndent;
    wxCheckBox *chkIndentGuides;
    wxCheckBox *chkWhiteSpaces;
    wxCheckBox *chkLineEnd;
    wxCheckBox *chkMatchingBraces;
    wxSpinCtrl *spinRightMargin;
    wxCheckBox *chkSyntaxHighlight;
    wxCheckBox *chkLineNumbers;
    wxCheckBox *chkRightMargin;
    wxCheckBox *chkFoldMargin;
    wxCheckBox *chkSplashScreen;
    wxSpinCtrl *spinTabSize;
    wxChoice *chLanguage;
    wxPanel *objPanelThemes;
    wxListBox *listThemeType;
    wxChoice *chTheme;
    wxButton *btnSaveTheme;
    wxButton *btnForeground;
    wxButton *btnBackground;
    wxChoice *chFont;
    wxCheckBox *chkBold;
    wxCheckBox *chkItalic;
    wxCheckBox *chkUnderLined;
    wxSpinCtrl *spinFontSize;
    wxPanel *objPanelKeywords;
    wxChoice *chKeywordGroup;
    wxTextCtrl *textKeyWords;
    wxPanel *objPanelCompiler;
    wxTextCtrl *textCompilerPath;
    wxButton *btnCompilerPath;
    wxTextCtrl *textCompilerCommand;
    wxTextCtrl *textRunCommand;
    wxTextCtrl *textHelpFile;
    wxButton *btnHelpFilePath;
    bool m_MoGeneral;
    bool m_ModTheme;
    bool m_ModKeyWord;
    bool m_ModCompiler;
    ////@end ConfigDialog member variables

    MyFrame *m_Parent;
    StyleInfo m_Style;
    CommonInfo m_Prefs;
    wxArrayString m_Lang;
    wxArrayString m_Keywords;
    int m_KeywordsGroupOld;
    int m_ThemeTypeOld;
    int m_ThemeOld;
    unsigned int m_fg;
    unsigned int m_bg;

    void LoadGeneral();

    void LoadThemes();

    void LoadKeywords();

    void LoadCompiler();

    void SetTypeSelection(int intSel);

    void StoreTypeSelection(int intSel);

};
