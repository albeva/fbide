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

class MyFrame;
class Buffer;

namespace kw {
    enum {
        SUB = 1,
        FUNCTION,
        IF,
        THEN,
        ELSE,
        ELSEIF,
        CASE,
        SELECT,
        WITH,
        ASM,
        TYPE,
        UNION,
        ENUM,
        SCOPE,
        DO,
        FOR,
        WHILE,
        //------
        LOOP,
        NEXT,
        WEND,
        END,
        STATIC,
        PRIVATE
    };
    const int word_count = 23;
    const wxString words[word_count] = {
        "sub", "function", "if", "then", "else", "elseif",
        "case", "select", "with", "asm", "type", "union",
        "enum", "scope", "do", "for", "while", "loop", "next",
        "wend", "end", "static", "private"
    };
}

class FB_Edit : public wxStyledTextCtrl {
public:
    FB_Edit(MyFrame *ParentFrame, wxWindow *parentNotebook, wxWindowID id = -1,
            wxString FileToLoad = FBUNNAMED,
            const wxPoint &pos = wxDefaultPosition,
            const wxSize &size = wxDefaultSize,
            long style = wxSUNKEN_BORDER | wxVSCROLL
    );

    MyFrame *Parent;
    Buffer *buff;

    void LoadSTCSettings();

    void LoadSTCTheme(int FileType = 0);

    void OnCharAdded(wxStyledTextEvent &event);

    void OnUpdateUI(wxStyledTextEvent &event);

    static bool IsBrace(wxChar brace);

    void OnMarginClick(wxStyledTextEvent &event);

    void OnModified(wxStyledTextEvent &event);

    void OnKeyDown(wxKeyEvent &event);

    void OnKeyUp(wxKeyEvent &event);

    void OnHotSpot(wxStyledTextEvent &event);

    void FB_Edit::IndentLine(int &lineInd, int cLine);

    wxString DocumentName;
    int braceLoc;
    int ChangeTab;
    bool exitUUI;

    int GetID(const wxString& kw) const;

    wxString ClearCmdLine(int cLine);

    /**
     * Return array containing keyword IDs
     * will contain first, second and last keyword
     * @param cmdline
     * @return
     */
    std::array<int, 3> GetKeywords(const wxString& string) const;


    inline void SetBuffer(Buffer *buff) {
        this->buff = buff;
    }

    inline bool FileExists(wxString File) {
        wxFileName w(File);
        return w.FileExists();
    }

    int m_CursorPos;
    char m_CharAtCur;


    //   ~FB_Edit ();
private:

DECLARE_EVENT_TABLE()
};
