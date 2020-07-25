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

void FBIdeMainFrame::OnFind(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    if (FindDialog != NULL || ReplaceDialog != NULL)
        return;

    if (HasSelection())
        FindData->SetFindString(stc->GetSelectedText());

    else
        FindData->SetFindString(GetTextUnderCursor());

    FindDialog = new wxFindReplaceDialog(this, FindData, _(Lang[219]), 0); //Find text
    FindDialog->Show();
    return;
}


void FBIdeMainFrame::OnReplace(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    if (FindDialog != NULL || ReplaceDialog != NULL)
        return;

    if (HasSelection())
        ReplaceData->SetFindString(stc->GetSelectedText());
    else
        ReplaceData->SetFindString(GetTextUnderCursor());

    ReplaceDialog = new wxFindReplaceDialog(this, ReplaceData,
                                            _(Lang[220]), wxFR_REPLACEDIALOG); //Replace text

    ReplaceDialog->Show();
    return;
}


void FBIdeMainFrame::OnFindAgain(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    int flags = FindData->GetFlags();
    if (flags & wxFR_DOWN)
        FindNext();
    else
        FindPrevious();
    return;
}


void FBIdeMainFrame::OnGotoLine(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    wxString lineString = wxGetTextFromUser(_(Lang[221]), _(Lang[222]), _(""), this);
    //Go To Line Number: \ Go To Line

    if (lineString.IsEmpty())
        return;

    if (lineString.Contains(":")) {
        wxString line, col;
        line = lineString.BeforeFirst(':');
        col = lineString.AfterFirst(':');
        if ((line.IsNumber() || line == "e") && (col.IsNumber() || col == "e")) {
            long lineNumber, colNumber;
            if (line.IsNumber())
                line.ToLong(&lineNumber);
            else
                lineNumber = stc->GetLineCount();

            lineNumber--;
            if (col.IsNumber()) {
                col.ToLong(&colNumber);
                colNumber--;
            } else
                colNumber = stc->GetLineEndPosition(lineNumber) -
                            stc->PositionFromLine(lineNumber);

            if (lineNumber >= 0 && lineNumber < stc->GetLineCount())
                stc->GotoLine(lineNumber);

            if (colNumber >= 0 && colNumber <=
                                  stc->GetLineEndPosition(lineNumber) -
                                  stc->PositionFromLine(lineNumber))
                stc->GotoPos(stc->GetCurrentPos() + colNumber);
        }
    } else if (lineString == "e")
        stc->GotoLine(stc->GetLineCount() - 1);

    else if (lineString.IsNumber()) {
        long lineNumber;
        lineString.ToLong(&lineNumber);
        lineNumber--;
        if (lineNumber >= 0 && lineNumber <= stc->GetLineCount())
            stc->GotoLine(lineNumber);
    }
}


void FBIdeMainFrame::FindButton(wxFindDialogEvent &event) {
    if (stc == 0)
        return;
    int flags = 0;
    if (event.GetFlags() & wxFR_WHOLEWORD)
        flags += wxSTC_FIND_WHOLEWORD;

    if (event.GetFlags() & wxFR_MATCHCASE)
        flags += wxSTC_FIND_MATCHCASE;

    // Search Down
    if (event.GetFlags() & wxFR_DOWN)
        FindOccurence(event.GetFindString(), 0, flags);

        // Search Up
    else
        FindOccurence(event.GetFindString(), 1, flags);

    return;
}


void FBIdeMainFrame::FindClose(wxFindDialogEvent &event) {

    if (event.GetDialog() == FindDialog) {
        FindDialog->Destroy();
        FindDialog = NULL;
    } else if (event.GetDialog() == ReplaceDialog) {
        ReplaceDialog->Destroy();
        ReplaceDialog = NULL;
    }
    return;
}


void FBIdeMainFrame::MenuFindNext(wxFindDialogEvent &event) {
    if (stc == 0)
        return;
    int flags = 0;

    if (event.GetFlags() & wxFR_WHOLEWORD)
        flags += wxSTC_FIND_WHOLEWORD;

    if (event.GetFlags() & wxFR_MATCHCASE)
        flags += wxSTC_FIND_MATCHCASE;

    // Search Down
    if (event.GetFlags() & wxFR_DOWN)
        FindOccurence(event.GetFindString(), 0, flags);

        // Search Up
    else
        FindOccurence(event.GetFindString(), 1, flags);

    return;
}


void FBIdeMainFrame::ReplaceSel(wxFindDialogEvent &event) {
    if (stc == 0)
        return;
    int flags = 0;
    if (event.GetFlags() & wxFR_WHOLEWORD)
        flags += wxSTC_FIND_WHOLEWORD;

    if (event.GetFlags() & wxFR_MATCHCASE)
        flags += wxSTC_FIND_MATCHCASE;

    Replace(event.GetFindString(), event.GetReplaceString(), flags);
}


void FBIdeMainFrame::MenuReplaceAll(wxFindDialogEvent &event) {
    if (stc == 0)
        return;
    int flags = 0;
    if (event.GetFlags() & wxFR_WHOLEWORD)
        flags += wxSTC_FIND_WHOLEWORD;

    if (event.GetFlags() & wxFR_MATCHCASE)
        flags += wxSTC_FIND_MATCHCASE;

    ReplaceAll(event.GetFindString(), event.GetReplaceString(), flags);
}



//------------------------------------------------------------------------------



inline bool FBIdeMainFrame::HasSelection() {
    return stc->GetSelectionStart() != stc->GetSelectionEnd();
}

inline bool FBIdeMainFrame::HasText() {
    return stc->GetLength() > 0;
}


wxString FBIdeMainFrame::GetTextUnderCursor() {
    int startpos = stc->WordStartPosition(stc->GetCurrentPos(), true);
    int endpos = stc->WordEndPosition(stc->GetCurrentPos(), true);

    return stc->GetTextRange(startpos, endpos);
}

wxString FBIdeMainFrame::GetTextUnderCursor(int &startPos, int &endPos) {
    startPos = stc->WordStartPosition(stc->GetCurrentPos(), true);
    endPos = stc->WordEndPosition(stc->GetCurrentPos(), true);

    return stc->GetTextRange(startPos, endPos);
}


inline bool FBIdeMainFrame::FindNext() {
    if (findString.IsEmpty())
        return false;
    return FindOccurence(findString, 0, FindFlags);
}

inline bool FBIdeMainFrame::FindPrevious() {
    if (findString.IsEmpty())
        return false;
    return FindOccurence(findString, 1, FindFlags);
}

inline void FBIdeMainFrame::FindPreviousWord(wxCommandEvent & WXUNUSED(event)) {
    FindCurrentWord(1);
}

inline void FBIdeMainFrame::FindNextWord(wxCommandEvent & WXUNUSED(event)) {
    FindCurrentWord(0);
}


bool FBIdeMainFrame::FindOccurence(const wxString &findStr, int direc, int flags) {
    // Store the find string and flags in the class for later use
    findString = findStr;
    FindFlags = flags;

    int targetStart = stc->GetCurrentPos();
    int targetEnd = direc == 0 ? stc->GetLength() : 0;

    // If this is a case sensitive search
    if (flags & wxSTC_FIND_MATCHCASE) {
        if ((stc->GetSelectedText() == findStr) && (direc == 0))
            targetStart = stc->GetSelectionEnd();

        else if ((stc->GetSelectedText() == findStr) && (direc == 1))
            targetStart = (stc->GetSelectionStart() < stc->GetSelectionEnd()) - 1;
    }

        // If this is a case insensitive search
    else {
        if ((stc->GetSelectedText().Lower() == findStr.Lower()) && (direc == 0))
            targetStart = stc->GetSelectionEnd();

        else if ((stc->GetSelectedText().Lower() == findStr.Lower()) && (direc == 1))
            targetStart = (stc->GetSelectionStart() < stc->GetSelectionEnd()) - 1;
    }

    // Set some search parameters
    stc->SetTargetStart(targetStart);
    stc->SetTargetEnd(targetEnd);
    stc->SetSearchFlags(flags);

    // If the text was found, select it and return true
    if (stc->SearchInTarget(findStr) != -1) {
        stc->SetSelection(stc->GetTargetStart(), stc->GetTargetEnd());
        stc->EnsureCaretVisible();
        return true;
    }

        // Return false because the text wasn't found
    else
        return false;

}


bool FBIdeMainFrame::FindCurrentWord(int direc) {
    wxString findText;
    if (HasSelection()) {
        findText = stc->GetSelectedText();
        return FindOccurence(findText, direc, 0);
    } else {
        findText = GetTextUnderCursor();
        return FindOccurence(findText, direc, wxSTC_FIND_WHOLEWORD);
    }

    return false;
}


void FBIdeMainFrame::Replace(const wxString &findStr, const wxString &replaceStr, int flags) {
    if (!HasSelection()) {
        wxBell();
        return;
    }

    if (flags & wxSTC_FIND_MATCHCASE && stc->GetSelectedText() != findStr) {
        wxBell();
        return;
    } else if (stc->GetSelectedText().Lower() != findStr.Lower()) {
        wxBell();
        return;
    }

    stc->ReplaceSelection(replaceStr);
    FindOccurence(findStr, 0, flags);
}


void FBIdeMainFrame::ReplaceAll(const wxString &findStr, const wxString &replaceStr, int flags) {
    int find;

    stc->BeginUndoAction();
    stc->SetTargetStart(HasSelection() ? stc->GetSelectionStart() : stc->GetCurrentPos() - 1);
    stc->SetTargetEnd(stc->GetLength());
    stc->SetSearchFlags(flags);

    do {
        find = stc->SearchInTarget(findStr);

        if (find != -1) {
            stc->ReplaceTarget(replaceStr);
            stc->SetTargetStart(stc->GetTargetEnd());
            stc->SetTargetEnd(stc->GetLength());
        }
    } while (find != -1);

    stc->EndUndoAction();
}


void FBIdeMainFrame::ReplaceCurrentWord(const wxString &text) {
    int start = stc->WordStartPosition(stc->GetCurrentPos(), true);
    int end = stc->WordEndPosition(stc->GetCurrentPos(), true);

    ReplaceText(start, end, text);
}


void FBIdeMainFrame::ReplaceText(int from, int to, const wxString &text) {
    if (from == to) {
        stc->InsertText(to, text);
        return;
    }

    stc->SetTargetStart(from);
    stc->SetTargetEnd(to);
    stc->ReplaceTarget(text);
}


