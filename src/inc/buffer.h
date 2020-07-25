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
#pragma once
#include "pch.h"

class Buffer {
public:
    Buffer(const wxString &fileName = "Untitled");

    const wxDateTime &GetModificationTime();

    void SetModificationTime(const wxDateTime &modTime);

    const wxString &GetFileName();

    void SetFileName(const wxString &fileName);

    bool IsUntitled();

    const wxString &GetHighlighter();

    void SetHighlighter(const wxString &highlighter);

    bool UpdateModTime();

    bool Exists();

    bool CheckModTime();

    void SetDocument(void *document);

    void *GetDocument();

    bool GetModified();

    void SetModified(bool modified);

    bool WasModified();

    void SetWasModified(bool wasModified);

    void SetPositions(int selStart, int selEnd);

    void SetCaretPos(int cp) {
        caretpos = cp;
    }

    int GetCaretPos() {
        return caretpos;
    }

    void SetLine(int firstLine);

    int GetSelectionStart();

    int GetSelectionEnd();

    int GetLine();

    void SetFileType(int i) {
        FileMode = i;
    }

    int GetFileType() {
        return FileMode;
    }

    void SetCompiledFile(wxString CompiledFile) {
        m_CompiledFile = CompiledFile;
    }

    wxString GetCompiledFile() {
        return m_CompiledFile;
    }

    void ClearFoldBuffer() {
        vectorFoldData.clear();
    }

    void AddFoldData(int line) {
        vectorFoldData.push_back(line);
    }

    bool GetFoldData(int &line) {
        if (vectorFoldData.empty()) return false;
        line = vectorFoldData.back();
        vectorFoldData.pop_back();
        return true;
    }

private:
    wxDateTime modTime;
    wxString fileName;
    wxString highlighter;
    wxString m_CompiledFile;

    bool wasModified;
    bool modified;

    void *document;

    int selStart, selEnd, firstLine, caretpos;
    int FileMode;

    std::vector<int> vectorFoldData;
};

WX_DEFINE_ARRAY(Buffer*, BufferArray);
