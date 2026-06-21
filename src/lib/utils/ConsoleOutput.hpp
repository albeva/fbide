//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Stdout / stderr for the GUI-subsystem (`/SUBSYSTEM:WINDOWS`) build, which has
/// no CRT-bound `std::cout` / `std::cerr` wired to the launching shell. Writes go
/// through the raw OS handle on Windows and, when the process was started without
/// a console (an Explorer launch), attach to the parent console first.
class ConsoleOutput final {
public:
    /// Write `text` to stdout verbatim — no trailing newline.
    static void write(const wxString& text);
    /// Write `text` to stdout followed by a newline.
    static void writeLine(const wxString& text);
    /// Write `text` to stderr followed by a newline.
    static void writeError(const wxString& text);
};

} // namespace fbide
