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
enum class MenuId : int {
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
    Settings = wxID_PROPERTIES,

    // Custom IDs
    NewWindow = wxID_HIGHEST,
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

} // namespace fbide
