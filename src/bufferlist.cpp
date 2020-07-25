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

/**
 * BufferList constructor.
 * Constructs a BufferList object holding multiple buffers.
 * @see BufferList::~BufferList()
 */
BufferList::BufferList()
    : modifiedCount(0) {}

/**
 * BufferList destructor.
 * Cleans up all of the buffers held by the BufferList.
 * @see BufferList::BufferList()
 */
BufferList::~BufferList() {
    WX_CLEAR_ARRAY(buffers);
}

/**
 * Adds a new untitled buffer using the highlighter passed to the function.
 * @param highlighter The highlighter the new buffer should use.
 * @see BufferList::AddFileBuffer(const wxString& fileName, const wxString& highlighter)
 * @return A pointer to the newly created buffer object.
 */
Buffer *BufferList::AddBuffer(const wxString &highlighter) {
    Buffer *buff = new Buffer("Untitled");
    buffers.Add(buff);

    return buff;
}

/**
 * Adds a new buffer with a file name. This function also initializes the
 * highlighter for the new buffer.
 * @param fileName The file name the new buffer should use.
 * @param highlighter The highlighter the new buffer should use.
 * @see BufferList::AddBuffer(const wxString& highlighter)
 * @return A pointer to the newly created buffer object.
 */
Buffer *BufferList::AddFileBuffer(const wxString &fileName, const wxString &highlighter) {
    Buffer *buff = new Buffer(fileName);
    buffers.Add(buff);

    return buff;
}

/**
 * Gets a pointer to the buffer at the index specified.
 * @param index The index of the buffer to get a pointer for.
 * @see BufferList::operator[](int index)
 * @return A pointer to the buffer specified by index.
 */
Buffer *BufferList::GetBuffer(int index) {
    return buffers[index];
}

/**
 * Gets the number of buffers in the buffer list.
 * @see BufferList::GetModifedCount()
 * @return The number of buffers in this buffer list.
 */
int BufferList::GetBufferCount() {
    return buffers.GetCount();
}

/**
 * Checks to see if the buffer at index is modified.
 * @param index The index of the buffer to check.
 * @return Whether the buffer is modified or not.
 */
bool BufferList::GetBufferModified(int index) {
    return buffers[index]->GetModified();
}

/**
 * Gets the last buffer in the buffer list.
 * @return A pointer to the last buffer in the buffer list.
 */
Buffer *BufferList::GetLastBuffer() {
    return buffers.Last();
}

/**
 * Gets the number of buffers which are currently modified.
 * @see BufferList::GetBufferCount
 * @return How many buffers are currently modified.
 */
int BufferList::GetModifiedCount() {
    return modifiedCount;
}

/**
 * Gets a pointer to the buffer at the index specified.
 * @param index The index of the buffer to get a pointer for.
 * @see BufferList::GetBuffer(int index)
 * @return A pointer to the buffer specified by index.
 */
Buffer *BufferList::operator[](int index) {
    return buffers[index];
}

/**
 * Removes the buffer at index.
 * @param index The index of the buffer to remove.
 * @see BufferList::AddBuffer(const wxString& highlighter)
 * @see BufferList::AddFileBuffer(const wxString& fileName, const wxString& highlighter)
 */
void BufferList::RemoveBuffer(int index) {
    delete buffers[index];
    buffers.RemoveAt(index);
}

/**
 * Sets the buffer at index as modified.
 * @param index The index of the buffer to set as modified.
 * @see BufferList::SetBufferUnModified(int index)
 */
void BufferList::SetBufferModified(int index, bool status) {
    if (buffers[index]->GetModified() == status)
        return;
    buffers[index]->SetModified(status);
    if (status)
        modifiedCount++;
    else
        modifiedCount--;
}

/**
 * Sets the buffer at index as unmodified.
 * @param index The index of the buffer to set as unmodified.
 * @see BufferList::SetBufferModified(int index)
 */
void BufferList::SetBufferUnModified(int index) {
    //    buffers[index]->SetModified(false);
    //    modifiedCount--;
}

void BufferList::SetBuffer(int index, Buffer *buff) {
    buffers[index] = buff;
}


int BufferList::FileLoaded(wxString FileName) {
    int counter = 0;
    Buffer *buff;
    FileName = FileName.Lower().Trim(true).Trim(false);

    wxFileName file(FileName);

    while (counter < GetBufferCount()) {
        buff = buffers[counter];
        wxFileName name(buff->GetFileName().Lower().Trim(true).Trim(false));

        if (name == file)
            return counter;
        counter++;
    }
    return -1;
}
















