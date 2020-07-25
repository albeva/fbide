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
#include "inc/fbedit.h"
#include "inc/FBIdeMainFrame.h"
#include "inc/browser.h"
#include "inc/wxmynotebook.h"

BEGIN_EVENT_TABLE (FB_Edit, wxStyledTextCtrl)
        EVT_STC_MARGINCLICK (-1, FB_Edit::OnMarginClick)
        EVT_STC_CHARADDED   (-1, FB_Edit::OnCharAdded)
        EVT_STC_UPDATEUI    (-1, FB_Edit::OnUpdateUI)
        EVT_STC_MODIFIED    (-1, FB_Edit::OnModified)
        EVT_STC_HOTSPOT_CLICK(-1, FB_Edit::OnHotSpot)
        EVT_KEY_UP(FB_Edit::OnKeyUp)
        EVT_KEY_DOWN(FB_Edit::OnKeyDown)
END_EVENT_TABLE()

FB_Edit::FB_Edit(FBIdeMainFrame *ParentFrame, wxWindow *parentNotebook, wxWindowID id,
                 wxString FileToLoad,
                 const wxPoint &pos,
                 const wxSize &size,
                 long style)
    : wxStyledTextCtrl(parentNotebook, id, pos, size, style) {

    Parent = ParentFrame;
    braceLoc = -1;
    buff = 0;
    ChangeTab = 0;
    exitUUI = false;
}


void FB_Edit::LoadSTCSettings() {
    CommonInfo *Prefs = &Parent->Prefs;

    SetTabWidth(Prefs->TabSize);
    SetUseTabs(false);
    SetTabIndents(true);
    SetBackSpaceUnIndents(true);
    SetIndent(Prefs->TabSize);

    SetEdgeColumn(Prefs->EdgeColumn);
    SetEOLMode(0);

    SetViewEOL(Prefs->DisplayEOL);

    SetIndentationGuides(Prefs->IndentGuide);
    SetEdgeMode(Prefs->LongLine ? wxSTC_EDGE_LINE : wxSTC_EDGE_NONE);
    SetViewWhiteSpace(Prefs->whiteSpace ? wxSTC_WS_VISIBLEALWAYS : wxSTC_WS_INVISIBLE);
    CmdKeyClear(wxSTC_KEY_TAB, 0);

    if (!Parent->Prefs.BraceHighlight) {
        if (braceLoc != -1) {
            BraceHighlight(-1, -1);
            braceLoc = -1;
        }
    } else {
        wxStyledTextEvent event;
        m_CharAtCur = 0;
        OnUpdateUI(event);
    }
}


void FB_Edit::LoadSTCTheme(int FileType) {
    CommonInfo *Prefs = &(Parent->Prefs);
    StyleInfo *Style = &(Parent->Style);

    SetLexer(0);
    for (int Nr = 0; Nr < 15; Nr++) {
        StyleSetForeground(Nr, GetClr(Style->DefaultFgColour));
        StyleSetBackground(Nr, GetClr(Style->DefaultBgColour));
        StyleSetBold(Nr, 0);
        StyleSetItalic(Nr, 0);
        StyleSetUnderline(Nr, 0);
        StyleSetCase(Nr, 0);
    }

    if (Prefs->SyntaxHighlight && FileType != 2) {
        if (FileType == 0) {
            SetLexer(wxSTC_LEX_VB);
            constexpr std::array styles{
                wxSTC_B_DEFAULT, wxSTC_B_COMMENT,
                wxSTC_B_NUMBER, wxSTC_B_KEYWORD,
                wxSTC_B_STRING, wxSTC_B_PREPROCESSOR,
                wxSTC_B_OPERATOR, wxSTC_B_IDENTIFIER,
                wxSTC_B_DATE, wxSTC_B_STRINGEOL,
                wxSTC_B_KEYWORD2, wxSTC_B_KEYWORD3,
                wxSTC_B_KEYWORD4, wxSTC_B_CONSTANT,
                wxSTC_B_ASM
            };

            for (int i = 1; i < styles.size(); i++) {
                auto no = styles[i];
                wxString fontname = "";

                //Foreground
                StyleSetForeground(no, GetClr(Style->Info[i].foreground));
                StyleSetBackground(no, GetClr(Style->Info[i].background));

                auto font = wxFont(
                    Style->DefaultFontSize,
                    wxFONTFAMILY_MODERN,
                    wxFONTSTYLE_NORMAL,
                    wxFONTWEIGHT_NORMAL,
                    false,
                    Style->Info[i].fontname);
                StyleSetFont(no, font);

                //Font attributes
                StyleSetBold(no, (Style->Info[i].fontstyle & mySTC_STYLE_BOLD) > 0);
                StyleSetItalic(no, (Style->Info[i].fontstyle & mySTC_STYLE_ITALIC) > 0);
                StyleSetUnderline(no, (Style->Info[i].fontstyle & mySTC_STYLE_UNDERL) > 0);
                StyleSetVisible(no, (Style->Info[i].fontstyle & mySTC_STYLE_HIDDEN) == 0);
                StyleSetCase(no, Style->Info[i].lettercase);
            }

            for (int no = 0; no < 4; no++)
                SetKeyWords(no, Parent->Keyword[no + 1]);

        } else if (FileType == 1) {
            SetLexer(wxSTC_LEX_HTML);

            auto font = wxFont(
                Style->DefaultFontSize,
                wxFONTFAMILY_MODERN,
                wxFONTSTYLE_NORMAL,
                wxFONTWEIGHT_NORMAL,
                false,
                Style->DefaultFont);

            for (int i = 0; i < 10; i++) {
                StyleSetFont(i, font);
                StyleSetBackground(i, *wxWHITE);
            }

            StyleSetForeground(wxSTC_H_DEFAULT, *wxBLACK);
            StyleSetForeground(wxSTC_H_TAG, wxColour(128, 0, 128));
            StyleSetForeground(wxSTC_H_TAGUNKNOWN, *wxBLACK);
            StyleSetForeground(wxSTC_H_ATTRIBUTE, *wxBLACK);
            StyleSetForeground(wxSTC_H_ATTRIBUTEUNKNOWN, *wxBLACK);
            StyleSetForeground(wxSTC_H_NUMBER, wxColour(0, 0, 255));
            StyleSetForeground(wxSTC_H_DOUBLESTRING, wxColour(0, 0, 255));
            StyleSetForeground(wxSTC_H_SINGLESTRING, wxColour(0, 0, 255));
            StyleSetForeground(wxSTC_H_COMMENT, *wxLIGHT_GREY);
            StyleSetForeground(wxSTC_H_ENTITY, wxColour(255, 69, 0));

            StyleSetBold(wxSTC_H_TAG, true);
            StyleSetBold(wxSTC_H_ATTRIBUTE, true);
            SetKeyWords(0, "color font b i body style size pre html head body meta http-equiv" \
                         "content charset span style title");
        }
    } else {
        for (int Nr = 0; Nr < 4; Nr++)
            SetKeyWords(Nr, "");
    }

    if (FileType == 0 || !Prefs->SyntaxHighlight || FileType == 2) {
        StyleSetForeground(wxSTC_STYLE_DEFAULT, GetClr(Style->DefaultFgColour));
        StyleSetBackground(wxSTC_STYLE_DEFAULT, GetClr(Style->DefaultBgColour));

        StyleSetForeground(wxSTC_STYLE_LINENUMBER, GetClr(Style->LineNumberFgColour));
        StyleSetBackground(wxSTC_STYLE_LINENUMBER, GetClr(Style->LineNumberBgColour));
        SetCaretForeground(GetClr(Style->CaretColour));

        SetSelBackground(true, GetClr(Style->SelectBgColour));
        SetSelForeground(true, GetClr(Style->SelectFgColour));

        auto font = wxFont(
            Style->DefaultFontSize,
            wxFONTFAMILY_MODERN,
            wxFONTSTYLE_NORMAL,
            wxFONTWEIGHT_NORMAL,
            false,
            Style->DefaultFont);

        StyleSetFont(wxSTC_STYLE_DEFAULT, font);
        StyleSetFont(wxSTC_STYLE_LINENUMBER, font);
    } else {
        StyleSetForeground(wxSTC_STYLE_DEFAULT, *wxBLACK);
        StyleSetBackground(wxSTC_STYLE_DEFAULT, *wxWHITE);

        StyleSetForeground(wxSTC_STYLE_LINENUMBER, *wxWHITE);
        StyleSetBackground(wxSTC_STYLE_LINENUMBER, wxColour(192, 192, 192));
        SetCaretForeground(*wxBLACK);

        SetSelBackground(true, wxColour(192, 192, 192));
        SetSelForeground(true, wxColour(255, 255, 255));
    }

    int LineNrMargin = TextWidth(wxSTC_STYLE_LINENUMBER, _T("00001"));
    SetMarginWidth(0, Prefs->LineNumber ? LineNrMargin : 0);
    SetMarginWidth(1, 0);

    //   SetCaretLineBack("RED");

    //Brace light
    StyleSetForeground(wxSTC_STYLE_BRACELIGHT, GetClr(Style->BraceFgColour));
    StyleSetBackground(wxSTC_STYLE_BRACELIGHT, GetClr(Style->BraceBgColour));
    StyleSetBold(wxSTC_STYLE_BRACELIGHT, (Style->BraceFontStyle & mySTC_STYLE_BOLD) > 0);
    StyleSetItalic(wxSTC_STYLE_BRACELIGHT, (Style->BraceFontStyle & mySTC_STYLE_ITALIC) > 0);
    StyleSetUnderline(wxSTC_STYLE_BRACELIGHT, (Style->BraceFontStyle & mySTC_STYLE_UNDERL) > 0);
    StyleSetVisible(wxSTC_STYLE_BRACELIGHT, (Style->BraceFontStyle & mySTC_STYLE_HIDDEN) == 0);
    //
    //   //BraceBad
    StyleSetForeground(wxSTC_STYLE_BRACEBAD, GetClr(Style->BadBraceFgColour));
    StyleSetBackground(wxSTC_STYLE_BRACEBAD, GetClr(Style->BadBraceBgColour));
    StyleSetBold(wxSTC_STYLE_BRACEBAD, (Style->BadBraceFontStyle & mySTC_STYLE_BOLD) > 0);
    StyleSetItalic(wxSTC_STYLE_BRACEBAD, (Style->BadBraceFontStyle & mySTC_STYLE_ITALIC) > 0);
    StyleSetUnderline(wxSTC_STYLE_BRACEBAD, (Style->BadBraceFontStyle & mySTC_STYLE_UNDERL) > 0);
    StyleSetVisible(wxSTC_STYLE_BRACEBAD, (Style->BadBraceFontStyle & mySTC_STYLE_HIDDEN) == 0);

    //

    //Markers
    SetMarginType(2, wxSTC_MARGIN_SYMBOL);
    SetMarginMask(2, wxSTC_MASK_FOLDERS);

    SetFoldFlags(wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);
    MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS, "white", "gray");
    MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_BOXPLUSCONNECTED, "white", "gray");
    MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER, "white", "gray");
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS, "white", "gray");
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_BOXMINUSCONNECTED, "white", "gray");
    MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_VLINE, "white", "gray");
    MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_LCORNER, "white", "gray");

    SetProperty("fold", "1");
    SetProperty("fold.comment", "1");
    SetProperty("fold.compact", "1");
    SetProperty("fold.preprocessor", "1");

    if (Prefs->FolderMargin) {
        SetMarginWidth(2, 14);
        SetMarginSensitive(2, 1);
    } else {
        SetMarginWidth(2, 0);
        SetMarginSensitive(2, 0);
    }
}

//Stc events
void FB_Edit::OnModified(wxStyledTextEvent &event) {
    if (buff == 0)
        return;
    int mod = event.GetModificationType();
    if (mod & wxSTC_MOD_INSERTTEXT ||
        mod & wxSTC_MOD_DELETETEXT ||
        mod & wxSTC_PERFORMED_UNDO ||
        mod & wxSTC_PERFORMED_REDO) {
        if (GetModify() != buff->GetModified()) {
            Parent->SetModified(-1, !buff->GetModified());
        }
        if (Parent->SFDialog) {
            if (!Parent->SFDialog->ChangePos)
                Parent->SFDialog->Rebuild();
        }
    }
}

void FB_Edit::OnUpdateUI(wxStyledTextEvent &event) {
    int tempPos = GetCurrentPos();
    char tempChr = GetCharAt(tempPos);

    if (m_CursorPos == tempPos && m_CharAtCur == tempChr)
        return;
    m_CursorPos = tempPos;
    m_CharAtCur = tempChr;

    if (Parent->Prefs.BraceHighlight) {
        if (IsBrace(m_CharAtCur)) {
            braceLoc = BraceMatch(m_CursorPos);

            if (braceLoc != -1)
                BraceHighlight(m_CursorPos, braceLoc);
            else {
                BraceBadLight(m_CursorPos);
                braceLoc = m_CursorPos;
            }
        } else {
            if (braceLoc != -1) {
                BraceHighlight(-1, -1);
                braceLoc = -1;
            }
        }
    }

    wxString pos;
    pos.Printf("%d : %d", LineFromPosition(m_CursorPos) + 1,
               GetColumn(m_CursorPos) + 1);
    Parent->SetStatusText(pos, 1);
}

inline bool FB_Edit::IsBrace(wxChar brace) {
    return brace == '{' || brace == '}' ||
           brace == '[' || brace == ']' ||
           brace == '(' || brace == ')';
}


void FB_Edit::OnCharAdded(wxStyledTextEvent &event) {
    event.Skip();
    if (!Parent->Prefs.AutoIndent)
        return;

    char key = event.GetKey();
    if (key != '\r')
        return;

    int cLine = GetCurrentLine();
    int lineInd = GetLineIndentation(cLine - 1);

    IndentLine(lineInd, cLine);

    GotoPos(PositionFromLine(cLine) + lineInd);
}

void FB_Edit::IndentLine(int &lineInd, int cLine) {
    int TabSize = Parent->Prefs.TabSize;
    wxString TempLine(ClearCmdLine(cLine - 1));
    auto keywords = GetKeywords(TempLine);

    auto FirstKW = keywords[0];
    auto SecondKW = keywords[1];
    auto LastKW = keywords[2];

    switch (FirstKW) {
        case kw::UNION :
        case kw::ENUM :
        case kw::WITH :
        case kw::SCOPE :
        case kw::SUB : {
            lineInd += TabSize;
            break;
        }
        case kw::STATIC :
        case kw::PRIVATE :
            if (SecondKW == kw::SUB)
                lineInd += TabSize;
            else if (SecondKW == kw::FUNCTION)
                if (TempLine.Find('=') == -1)
                    lineInd += TabSize;
            break;
        case kw::FUNCTION : {
            TempLine = TempLine.Mid(8).Trim(false);
            if (!TempLine.empty() && TempLine[0] != '=')
                lineInd += TabSize;
            break;
        }
        case kw::IF : {
            if (LastKW == kw::THEN)
                lineInd += TabSize;
            break;
        }
        case kw::ELSE:
        case kw::ELSEIF: {
            if (LastKW == kw::THEN || LastKW == FirstKW) {
                int i = cLine - 2;
                if (i < 0)
                    return;
                wxString pLine = ClearCmdLine(i);
                while (pLine.Len() == 0 && i) {
                    i--;
                    pLine = ClearCmdLine(i);
                }
                int plineInd = GetLineIndentation(i);

                auto pKeywords = GetKeywords(pLine);
                auto pFirstKW = pKeywords[0];
                auto pLastKW = pKeywords[2];

                if (lineInd > 0 && lineInd >= plineInd) {
                    if ((pFirstKW == kw::IF || pFirstKW == kw::ELSE) && pLastKW == kw::THEN)
                        lineInd = plineInd;
                    else
                        lineInd -= TabSize;
                    SetLineIndentation(cLine - 1, lineInd);
                }
                lineInd += TabSize;
            }
            break;
        }
        case kw::CASE : {
            int i = cLine - 2;
            if (i < 0)
                return;
            wxString pLine = ClearCmdLine(i);
            while (pLine.Len() == 0 && i) {
                i--;
                pLine = ClearCmdLine(i);
            }
            int plineInd = GetLineIndentation(i);
            auto pKeywords = GetKeywords(pLine);
            auto pFirstKW = pKeywords[0];

            if (lineInd > 0 && lineInd >= plineInd) {
                if (pFirstKW != kw::CASE && pFirstKW != kw::SELECT) {
                    lineInd -= TabSize;
                } else
                    lineInd = plineInd;
                SetLineIndentation(cLine - 1, lineInd);
            }
            if (TempLine.Find(':') == -1)
                lineInd += TabSize;
            break;
        }
        case kw::TYPE : {
            if ((!TempLine.Contains(" as ")) && (!TempLine.Contains("\tas ")) &&
                (!TempLine.Contains(" as\t")) && (!TempLine.Contains("\tas\t")) &&
                LastKW != FirstKW)
                lineInd += TabSize;
            break;
        }
        case kw::ASM : {
            if (LastKW == FirstKW && TempLine.Find(':') == -1)
                lineInd += TabSize;
            break;
        }
        case kw::DO : {
            if (LastKW != kw::LOOP)
                lineInd += TabSize;
            break;
        }
        case kw::FOR : {
            if (LastKW != kw::NEXT)
                lineInd += TabSize;
            break;
        }
        case kw::WHILE : {
            if (LastKW != kw::WEND)
                lineInd += TabSize;
            break;
        }
        case kw::NEXT :
        case kw::WEND :
        case kw::LOOP : {
            int i = cLine - 2;
            if (i < 0)
                return;
            wxString pLine = ClearCmdLine(i);
            while (pLine.Len() == 0 && i) {
                i--;
                pLine = ClearCmdLine(i);
            }
            int plineInd = GetLineIndentation(i);

            if (lineInd > 0 && lineInd >= plineInd) {
                lineInd -= TabSize;
                SetLineIndentation(cLine - 1, lineInd);
            }
            break;
        }
        case kw::END : {
            int i = cLine - 2;
            if (i < 0)
                return;
            wxString pLine = ClearCmdLine(i);
            while (pLine.Len() == 0 && i > 0) {
                i--;
                pLine = ClearCmdLine(i);
            }
            int plineInd = GetLineIndentation(i);

            if (lineInd > 0 && lineInd >= plineInd) {
                auto pKeywords = GetKeywords(pLine);
                auto pFirstKW = pKeywords[0];
                auto pLastKW = pKeywords[2];
                switch (SecondKW) {
                    case kw::SUB :
                    case kw::FUNCTION :
                    case kw::IF :
                    case kw::SELECT :
                    case kw::WITH :
                    case kw::ASM :
                    case kw::TYPE :
                    case kw::UNION :
                    case kw::SCOPE :
                    case kw::ENUM : {
                        TempLine = TempLine.Mid(8).Trim(false);
                        if (SecondKW == kw::FUNCTION &&
                            pFirstKW == kw::FUNCTION &&
                            TempLine[0] != '=')
                            lineInd -= TabSize;
                        else if (SecondKW == kw::IF &&
                                 (pFirstKW == kw::IF || pFirstKW == kw::ELSE) &&
                                 pLastKW != kw::THEN)
                            lineInd -= TabSize;
                        else if (pFirstKW == SecondKW ||
                                 (SecondKW == kw::IF && pFirstKW == kw::ELSE) ||
                                 (SecondKW == kw::SELECT && pFirstKW == kw::CASE)) {
                            lineInd = plineInd;
                        } else
                            lineInd -= TabSize;
                        SetLineIndentation(cLine - 1, lineInd);
                        break;
                    }
                } // switch
            } // if
            break;
        } // case
        default : {
            break;
        }
    }
    SetLineIndentation(cLine, lineInd);
}

wxString FB_Edit::ClearCmdLine(int cLine) {
    wxString cmdline(GetLine(cLine).Trim(false).Lower());
    bool instring = false;
    int len = 0;
    if (cmdline.Len() < 2) {
        return "";
    }
    if (cmdline[0] == '#' || (cmdline[0] == '\'' && cmdline[1] == '$'))
        return cmdline.Trim(true);
    for (unsigned int i = 0; i < cmdline.Len(); i++) {
        if (cmdline[i] == '\"')
            instring = !instring;
        else if (cmdline[i] == '\'' && !instring)
            break;
        len++;
    }
    return cmdline.Left(len).Trim(true);
}

static inline bool IsIdentChar(const wxUniChar& ch) {
    return ::wxIsalpha(ch) || ch == '_';
}

std::array<int, 3> FB_Edit::GetKeywords(const wxString& string) const {
    std::array<int, 3> result{0, 0, 0};

    int key = 0;
    int lastStart = -1, lastLen = -1;
    for (auto index = 0; index < string.Len(); index++) {
        const auto ch = string[index];

        // white spaces
        if (ch == ' ' || ch == '\t') {
            continue;
        }

        // preprocessor
        if (ch == '#') {
            break;
        }

        // string literal
        if (ch == '\"') {
            while (++index < string.Len()) {
                auto c = string[index];
                if (c == '"') {
                    break;
                }
                if (c == '\\' && (index + 1) < string.Len() && string[index + 1] == '"') {
                    index++;
                    continue;
                }
            }
            continue;
        }

        // inline comment
        if (ch == '/' && index + 1 < string.Len() && string[index + 1] == '\'') {
            index++;
            while(++index < string.Len()) {
                if (string[index] == '\'' && index + 1 < string.Len() && string[index + 1] == '/') {
                    index++;
                    break;
                }
            }
            continue;
        }

        // some sort of identifier
        if (IsIdentChar(ch)) {
            auto start = index;
            while (++index < string.Len() && IsIdentChar(string[index]));

            if (key < 2) {
                auto len = index - start;
                auto word = string.Mid(start, len);
                result[key] = GetID(word);
                key += 1;
            } else {
                lastStart = start;
                lastLen = index - start;
            }
            continue;
        }

        // something follows last keyword. ignore it
        lastStart = -1;
    }

    if (lastStart != -1) {
        auto word = string.Mid(lastStart, lastLen);
        result[2] = GetID(word);
    }

    return result;
}

int FB_Edit::GetID(const wxString& kw) const {
    for (int i = 0; i < kw::word_count; i++) {
        if (kw == kw::words[i]) {
            return i + 1;
        }
    }
    return 0;
}

void FB_Edit::OnMarginClick(wxStyledTextEvent &event) {
    if (event.GetMargin() == 2) {
        int lineClick = LineFromPosition(event.GetPosition());
        int levelClick = GetFoldLevel(lineClick);
        if ((levelClick & wxSTC_FOLDLEVELHEADERFLAG) > 0) {
            ToggleFold(lineClick);
        }
    }
}

void FB_Edit::OnKeyDown(wxKeyEvent &event) {
    event.Skip();
    if (!event.ControlDown())
        return;
    if (!event.AltDown()) {
        int key = event.GetKeyCode();
        if (key >= 48 && key <= 57) {
            if (key == 48)
                key = 58;
            int tab = (key - 49) + (event.ShiftDown() ? 10 : 0);
            if (tab != (int) Parent->FBNotebook->GetSelection() &&
                tab < (int) Parent->FBNotebook->GetPageCount()) {
                Parent->FBNotebook->SetSelection(tab);
            }
            return;
        } else if (key == WXK_TAB && Parent->FBNotebook->GetPageCount() > 1) {
            int max = Parent->FBNotebook->GetPageCount() - 1;
            int current = Parent->FBNotebook->GetSelection();

            if (event.ShiftDown()) {
                if (current == 0) current = max;
                else current--;
            } else {
                if (current < max) current++;
                else current = 0;
            }
            Parent->FBNotebook->SetSelection(current);
            return;
        }
        SetMouseDownCaptures(false);
        StyleSetHotSpot(wxSTC_B_PREPROCESSOR, true);
    }
    return;
}

void FB_Edit::OnKeyUp(wxKeyEvent &event) {
    event.Skip();
    if (event.ControlDown())
        return;
    SetMouseDownCaptures(true);
    StyleSetHotSpot(wxSTC_B_PREPROCESSOR, false);
    return;
}

void FB_Edit::OnHotSpot(wxStyledTextEvent &event) {
    ChangeTab = LineFromPosition(event.GetPosition());
    wxString Temp = ClearCmdLine(ChangeTab);
    wxString FileName;

    if (Temp[0] == '\'')
        Temp = Temp.Mid(2);

    FileName = Temp.AfterFirst('\"');
    FileName = FileName.BeforeFirst('\"');

    if (!FileName.Len()) {
        FileName = Temp.AfterFirst('\'');
        FileName = FileName.BeforeFirst('\'');
    }

    FileName = FileName.MakeLower();
    wxFileName File(FileName);

    if (!File.HasVolume()) {
        wxString FilePath = buff->GetFileName();
        wxString FBCPath = Parent->CompilerPath;
        if (FilePath == "" || FilePath == FBUNNAMED)
            FilePath = "";

        wxFileName w(FilePath);
        FilePath = w.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME);

        w.Assign(FBCPath);
        FBCPath = w.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME);

        if (FileName != "") {
            if (FileExists(FilePath + FileName)) {
                FileName = FilePath + FileName;
            } else if (FileExists(FBCPath + FileName)) {
                FileName = FBCPath + FileName;
            } else if (FileExists(FBCPath + "inc\\" + FileName)) {
                FileName = FBCPath + "inc\\" + FileName;
            } else
                FileName = "";
        }
    }

    if (FileName != "") {
        if (FileExists(FileName)) {
            if (FileName.Right(4) == ".exe" || FileName.Right(2) == ".o" ||
                FileName.Right(4) == ".dll" || FileName.Right(2) == ".a")
                return;
            int result = Parent->bufferList.FileLoaded(FileName);
            if (result != -1)
                Parent->FBNotebook->SetSelection(result);
            else {
                Parent->NewSTCPage(FileName, true);
                Parent->SetTitle("FBIde - " + Parent->bufferList[Parent->FBNotebook->GetSelection()]->GetFileName());
            }
            return;
        }
    }
}
