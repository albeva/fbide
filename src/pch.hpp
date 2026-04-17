//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

// STL
#include <array>
#include <utility>
#include <cstddef>
#include <memory>
#include <functional>
#include <vector>
#include <optional>
#include <variant>
#include <concepts>

// TOML++
#include <toml.hpp>

// wxWidgets
#include <wx/wx.h>

// wxWidgets components
#include <wx/apptrait.h>
#include <wx/artprov.h>
#include <wx/aui/aui.h>
#include <wx/colordlg.h>
#include <wx/dir.h>
#include <wx/fdrepdlg.h>
#include <wx/fileconf.h>
#include <wx/filehistory.h>
#include <wx/filename.h>
#include <wx/fontenum.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/process.h>
#include <wx/spinctrl.h>
#include <wx/splash.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/stc/stc.h>
#include <wx/stdpaths.h>
#include <wx/tokenzr.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>
#include <wx/wupdlock.h>
#include <wx/hyperlink.h>
#include <wx/log.h>

// fbide
#include "lib/utils/Macros.hpp"
#include "lib/utils/NoCopy.hpp"
#include "lib/utils/Unowned.hpp"
#include "lib/utils/ValueRestorer.hpp"
#include "lib/utils/DeferHandler.hpp"
#include "lib/utils/Utils.hpp"
