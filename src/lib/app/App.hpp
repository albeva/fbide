//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Main application class. Manages the application lifecycle.
class App final : public wxApp {
public:
    /// Initialize the application, create main window.
    auto OnInit() -> bool override;
};

} // namespace fbide
