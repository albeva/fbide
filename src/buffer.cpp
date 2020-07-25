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

/**
 * Buffer constructor.
 * Constructs a buffer object from a file name and highlighter.
 * @param fileName The file name of the buffer. Use "Untitled" for and
 * untitled file.
 * @param highlighter The highlighter the buffer should use.
 */
Buffer::Buffer(const wxString &fileName)
    : wasModified(false), modified(false), document(NULL),
      selStart(0), selEnd(0), firstLine(0) {
    this->fileName = fileName;
    FileMode = 0;
    //    UpdateModTime();
}

/**
 * Checks the modification time of the file to see if the file has been
 * modified externally.
 * @see Buffer::GetModificationTime()
 * @see Buffer::SetModificationTime(const wxDateTime& modTime)
 * @see Buffer::UpdateModTime()
 * @return Whether the file modification time has changed and the file
 * has been modified externally.
 */
bool Buffer::CheckModTime() {
    if (!Exists()) {
        return false;
    }

    wxFileName file(fileName);

    if (file.GetModificationTime() > modTime) {
        return true;
    }

    return false;
}

/**
 * Checks to see whether the file name specified in the buffer exists.
 * If the file name is empty or is the string "Untitled", false is returned.
 * @see Buffer::IsUntitled()
 * @return Whether the file referenced by this buffer exists.
 */
bool Buffer::Exists() {
    if (fileName.IsEmpty() || IsUntitled()) {
        return false;
    } else {
        return wxFileExists(fileName);
    }
}

/**
 * Gets the document pointer referenced by this buffer.
 * @see SetDocument(void* document)
 * @return A document pointer to the document this buffer represents.
 */
void *Buffer::GetDocument() {
    return document;
}

/**
 * Gets the file name this buffer is holding.
 * @see Buffer::SetFileName(const wxString& fileName)
 * @return The file name of this buffer.
 */
const wxString &Buffer::GetFileName() {
    return fileName;
}

/**
 * Gets the highlighter name the buffer is holding.
 * @todo The highlighter name should be stored as an index, not a string.
 * @see Buffer::SetHighlighter(const wxString& highlighter)
 * @return The name of the highlighter this buffer is currently using.
 */
const wxString &Buffer::GetHighlighter() {
    return highlighter;
}

/**
 * Gets the current line for the document. This is used to restore
 * the line that was scrolled to after changing buffers. Please note that
 * this gets the first visible line.
 * @see Buffer::SetLine(int firstLine)
 * @return The first visible line for the document.
 */
int Buffer::GetLine() {
    return firstLine;
}

/**
 * Gets the modification time of the file.
 * @see Buffer::SetModificationTime(const wxDateTime& modTime)
 * @return A wxDateTime object representing the modification time
 * of the file this buffer represents.
 */
const wxDateTime &Buffer::GetModificationTime() {
    return modTime;
}

/**
 * Gets whether this buffer has been modified.
 * @see Buffer::SetModified(bool modified)
 * @return Whether this buffer has been modified.
 */
bool Buffer::GetModified() {
    return modified;
}

/**
 * Gets the index of the end of the selection in the current document.
 * This is used to restore the selection between buffer changes. If the
 * document has no selection, the return value will be equal to the current
 * position, which will also be the same as what is
 * returned by GetSelectionStart().
 * @see Buffer::GetSelectionStart()
 * @return The end of the selection in the current document.
 */
int Buffer::GetSelectionEnd() {
    return selEnd;
}

/**
 * Gets the index of the start of the selection in the current document.
 * This is used to restore the selection between buffer changes. If the
 * document has no selection, the return value will be equal to the current
 * position, which will also be the same as what is
 * returned by GetSelectionEnd().
 * @see GetSelectionEnd()
 * @return The start of the selection in the current document.
 */
int Buffer::GetSelectionStart() {
    return selStart;
}

/**
 * Returns whether the current buffer represents an untitled file. A file
 * is considered untitled if the file name is the string "Untitled".
 * @see Buffer::Exists()
 * @return If the current buffer is untitled for the buffer.
 */
bool Buffer::IsUntitled() {
    return fileName == "Untitled"; //Untitled
}

/**
 * Changes the document pointer to the one passed to this function.
 * @param document The new document pointer.
 * @see Buffer::GetDocument()
 */
void Buffer::SetDocument(void *document) {
    this->document = document;
}

/**
 * Changes the file name to the one passed to this function.
 * This function is usually called after an untitled file has been saved
 * and the file name should be changed from "Untitled" to represent
 * the file it has been saved as.
 * @param fileName The new file name for the buffer.
 * @see Buffer::GetFileName()
 */
void Buffer::SetFileName(const wxString &fileName) {
    this->fileName = fileName;
}

/**
 * Changes the highlighter this buffer represents to the one passed
 * to this function. This function is used whenever a highlighter
 * change occurs.
 * @param highlighter The new highlighter name for the buffer.
 * @see Buffer::GetHighlighter()
 */
void Buffer::SetHighlighter(const wxString &highlighter) {
    this->highlighter = highlighter;
}

/**
 * Updates the number of the first visible line in the document. This is
 * called before a buffer change so that when the buffer is restored
 * the scroll bar will be back in the same position as it was
 * before the buffer change. Using this function will only store the first
 * visible line; the first line in the TextCtrl will remain unaffected
 * and will not be changed after a call to this function.
 * @param firstLine The first line visible in the current document.
 * @see Buffer::GetLine()
 */
void Buffer::SetLine(int firstLine) {
    this->firstLine = firstLine;
}

/**
 * Sets the modification time to the one passed to this function. Please
 * note that this doesn't change the actual modification time of the file,
 * only the one stored in this class.
 * @param modTime The new modification time of the file.
 * @see Buffer::GetModificationTime()
 */
void Buffer::SetModificationTime(const wxDateTime &modTime) {
    this->modTime = modTime;
}

/**
 * Sets this buffer as being modified or unmodified. The buffer is
 * set as modified when text changes and unmodified after saving the file.
 * The wasModified variable is also set to true the first time this
 * function is called.
 * @param modified True if the current buffer has been modified, false if
 * the current buffer is unmodified.
 * @see Buffer::GetModified()
 * @see Buffer::WasModified()
 * @see Buffer::SetWasModified(bool wasModified)
 */
void Buffer::SetModified(bool modified) {
    this->modified = modified;

    //    if (modified && !wasModified)
    //    {
    //        wasModified = true;
    //    }
}

/**
 * Sets the selection start and the selection end. If there is no selection,
 * these should be the same.
 * @param selStart The start of the selection.
 * @param selEnd The end of the selection.
 * @see Buffer::GetSelectionStart()
 * @see Buffer::GetSelectionEnd()
 */
void Buffer::SetPositions(int selStart, int selEnd) {
    this->selStart = selStart;
    this->selEnd = selEnd;
}

/**
 * Sets whether this buffer has ever been modified. This function should
 * never need to be called except when debugging since Buffer::SetModified()
 * already sets whether the buffer has been modifed automatically.
 * @see Buffer::WasModified()
 * @param wasModified If this buffer has ever been modified since it was
 * created.
 */
void Buffer::SetWasModified(bool wasModified) {
    this->wasModified = wasModified;
}
// bitmap->SetMask( new wxMask( *bitmap, wxColour( 192, 192, 192 ) ) );
/**
 * Updates the modification time of the file to match the current one on
 * disk. This should be called after saving (so that Buffer::CheckModTime()
 * doesn't think the file was modified externally) and after the file
 * has been updated when an external change occurred. If this isn't called
 * after the file has been modified on disk, Buffer::CheckModTime will fail.
 * @see Buffer::GetModificationTime()
 * @see Buffer::SetModificationTime(const wxDateTime& modTime)
 * @see Buffer::CheckModTime()
 * @return Whether updating the modification time suceeeded. It fail
 * when the current file doesn't exist or the file is untitled.
 */
bool Buffer::UpdateModTime() {
    if (!Exists()) {
        return false;
    }

    wxFileName file(fileName);
    modTime = file.GetModificationTime();
    return true;
}

/**
 * Checks to see if this buffer has ever been modified. This is used
 * when checking to see if the first untitled buffer can be replaced
 * by a file.
 * @see Buffer::SetWasModified(bool wasModified)
 * @return If this buffer has ever been modified.
 */
bool Buffer::WasModified() {
    return wasModified;
}

