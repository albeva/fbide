//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ConsoleOutput.hpp"
#ifdef __WXMSW__
#include <windows.h>
#endif
using namespace fbide;

namespace {

#ifdef __WXMSW__
/// Inject a synthetic Enter into the parent console's input queue. Workaround
/// for the `/SUBSYSTEM:WINDOWS` UX wart: the shell doesn't wait for a GUI
/// child, so it prints its next prompt immediately and our AttachConsole +
/// WriteFile output prints on top of it. Posting Enter makes the shell consume
/// the (empty) line and redraw a fresh prompt below our text. Skipped when
/// stdin isn't a console (e.g. piped/redirected).
void pokeParentConsole() {
    const HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == nullptr || hIn == INVALID_HANDLE_VALUE) {
        return;
    }
    if (GetFileType(hIn) != FILE_TYPE_CHAR) {
        return;
    }
    INPUT_RECORD events[2] = {};
    events[0].EventType = KEY_EVENT;
    events[0].Event.KeyEvent.bKeyDown = TRUE;
    events[0].Event.KeyEvent.wRepeatCount = 1;
    events[0].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
    events[0].Event.KeyEvent.uChar.AsciiChar = '\r';
    events[1] = events[0];
    events[1].Event.KeyEvent.bKeyDown = FALSE;
    DWORD written = 0;
    WriteConsoleInput(hIn, events, 2, &written);
}
#endif

/// Write `utf8` verbatim to the host's stdout/stderr. Goes through the raw OS
/// handle on Windows because `/SUBSYSTEM:WINDOWS` builds don't have CRT-bound
/// streams even when the shell redirects. If no handle is attached (Explorer
/// launch with no parent console), attach to the parent and retry, then poke an
/// Enter so the shell redraws its prompt.
void writeBytes(const std::string& utf8, const bool toStderr) {
#ifdef __WXMSW__
    const DWORD stdHandle = toStderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
    auto write = [&](const HANDLE h) {
        DWORD written = 0;
        return h != nullptr && h != INVALID_HANDLE_VALUE
            && WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr) != 0;
    };
    if (write(GetStdHandle(stdHandle))) {
        return;
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS) != 0) {
        write(GetStdHandle(stdHandle));
        pokeParentConsole();
    }
#else
    auto& stream = toStderr ? std::cerr : std::cout;
    stream << utf8;
    stream.flush();
#endif
}

} // namespace

void ConsoleOutput::write(const wxString& text) {
    writeBytes(text.ToStdString(wxConvUTF8), /*toStderr=*/false);
}

void ConsoleOutput::writeLine(const wxString& text) {
    writeBytes(text.ToStdString(wxConvUTF8) + '\n', /*toStderr=*/false);
}

void ConsoleOutput::writeError(const wxString& text) {
    writeBytes(text.ToStdString(wxConvUTF8) + '\n', /*toStderr=*/true);
}
