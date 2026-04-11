//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

// STL
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// wxWidgets
#include <wx/wx.h>

// wxWidgets components
#include <wx/apptrait.h>
#include <wx/aui/aui.h>
#include <wx/filename.h>
#include <wx/stc/stc.h>
#include <wx/stdpaths.h>
#include <wx/tokenzr.h>
#include <wx/wupdlock.h>

// fbide
#include "lib/utils/Macros.hpp"
#include "lib/utils/Unowned.hpp"

namespace fbide {
using namespace std::string_view_literals;
using namespace std::string_literals;
} // namespace fbide
