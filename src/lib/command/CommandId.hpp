//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Menu and toolbar command IDs.
/// Standard wxWidgets IDs used where possible for built-in accelerators.
enum class CommandId : wxWindowID {
    // Standard wxWidgets IDs
    Quit = wxID_EXIT,
    About = wxID_ABOUT,
    Help = wxID_HELP,
    New = wxID_NEW,
    Open = wxID_OPEN,
    Save = wxID_SAVE,
    SaveAs = wxID_SAVEAS,
    Close = wxID_CLOSE,
    Undo = wxID_UNDO,
    Redo = wxID_REDO,
    Cut = wxID_CUT,
    Copy = wxID_COPY,
    Paste = wxID_PASTE,
    SelectAll = wxID_SELECTALL,
    Find = wxID_FIND,
    Replace = wxID_REPLACE,
    Preferences = wxID_PREFERENCES,

    // Custom IDs
    NewWindow = wxID_HIGHEST,
    RecentFiles,
    ClearRecentFiles,
    SaveAll,
    FileHistory,
    SessionSave,
    SessionLoad,
    CloseAll,
    SelectLine,
    IndentIncrease,
    IndentDecrease,
    Comment,
    Uncomment,
    FindNext,
    FindPrevious,
    GotoLine,
    Format,
    Result,
    CompilerLog,
    Subs,
    Compile,
    CompileAndRun,
    Run,
    QuickRun,
    CmdPrompt,
    Parameters,
    ShowExitCode,
    QuickKeys,
    ReadMe,
};
FBIDE_INLINE static constexpr auto operator+(const CommandId& rhs) -> wxWindowID {
    return static_cast<wxWindowID>(rhs);
}

} // namespace fbide
