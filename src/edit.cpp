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

void FBIdeMainFrame::OnMenuUndo(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    if (!stc->CanUndo())
        return;
    stc->Undo();
    stc->EnsureCaretVisible();
}

void FBIdeMainFrame::OnMenuRedo(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    if (!stc->CanRedo())
        return;
    stc->Redo();
    stc->EnsureCaretVisible();
}

void FBIdeMainFrame::OnMenuCut(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    if ((stc->GetSelectionEnd() - stc->GetSelectionStart() <= 0))
        return;
    stc->Cut();
    stc->EnsureCaretVisible();
}

void FBIdeMainFrame::OnMenuCopy(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    if (stc->GetSelectionEnd() - stc->GetSelectionStart() <= 0)
        return;
    stc->Copy();
}

void FBIdeMainFrame::OnMenuPaste(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    if (!stc->CanPaste())
        return;
    stc->Paste();
    stc->EnsureCaretVisible();
}

void FBIdeMainFrame::OnSelectAll(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    stc->SetSelection(0, stc->GetTextLength());
}

void FBIdeMainFrame::OnSelectLine(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    int lineStart = stc->PositionFromLine(stc->GetCurrentLine());
    int lineEnd = stc->PositionFromLine(stc->GetCurrentLine() + 1);
    stc->SetSelection(lineStart, lineEnd);
}

void FBIdeMainFrame::OnIndentInc(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    stc->CmdKeyExecute(wxSTC_CMD_TAB);
}

void FBIdeMainFrame::OnIndentDecr(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    stc->CmdKeyExecute(wxSTC_CMD_BACKTAB);
}

void FBIdeMainFrame::OnComment(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    int SelStart = stc->GetSelectionStart();
    int SelEnd = stc->GetSelectionEnd();
    int lineStart = stc->LineFromPosition(SelStart);
    int lineEnd = stc->LineFromPosition(SelEnd);

    stc->BeginUndoAction();
    for (; lineStart <= lineEnd; lineStart++) {
        stc->InsertText(stc->PositionFromLine(lineStart), "\'");
    }
    stc->EndUndoAction();
}

void FBIdeMainFrame::OnUncomment(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    int lineStart = stc->LineFromPosition(stc->GetSelectionStart());
    int lineEnd = stc->LineFromPosition(stc->GetSelectionEnd());
    int x = 0;
    wxString Temp;

    stc->BeginUndoAction();
    for (; lineStart <= lineEnd; lineStart++) {
        Temp = stc->GetLine(lineStart);
        Temp = Temp.Lower();
        Temp = Temp.Trim(false);
        Temp = Temp.Trim(true);
        Temp += " ";
        x = stc->PositionFromLine(lineStart) + stc->GetLineIndentation(lineStart);
        if (Temp.Left(4) == "rem " || Temp.Left(4) == "rem\t")
            ReplaceText(x, x + 3, "");
        else if (Temp.Left(1) == "\'")
            ReplaceText(x, x + 1, "");
    }
    stc->EndUndoAction();
}
