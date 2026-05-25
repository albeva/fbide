//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

// STL
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

// wxWidgets
#include <wx/wx.h>

// wxWidgets components
#include <wx/apptrait.h>
#include <wx/artprov.h>
#include <wx/aui/aui.h>
#include <wx/bmpbuttn.h>
#include <wx/checkbox.h>
#include <wx/clipbrd.h>
#include <wx/colordlg.h>
#include <wx/convauto.h>
#include <wx/dataobj.h>
#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/dir.h>
#include <wx/fdrepdlg.h>
#include <wx/ffile.h>
#include <wx/file.h>
#include <wx/fileconf.h>
#include <wx/filedlg.h>
#include <wx/filehistory.h>
#include <wx/filename.h>
#include <wx/fontenum.h>
#include <wx/hyperlink.h>
#include <wx/image.h>
#include <wx/imaglist.h>
#include <wx/ipc.h>
#include <wx/listctrl.h>
#include <wx/log.h>
#include <wx/menu.h>
#include <wx/notebook.h>
#include <wx/process.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/snglinst.h>
#include <wx/spinctrl.h>
#include <wx/splash.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/stc/minimap.h>
#include <wx/stc/stc.h>
#include <wx/stdpaths.h>
#include <wx/textfile.h>
#include <wx/tglbtn.h>
#include <wx/thread.h>
#include <wx/tokenzr.h>
#include <wx/treectrl.h>
#include <wx/txtstrm.h>
#include <wx/utils.h>
#include <wx/webrequest.h>
#include <wx/wfstream.h>
#include <wx/wupdlock.h>

// fbide
#include "utils/DeferHandler.hpp"
#include "utils/HashCombine.hpp"
#include "utils/Macros.hpp"
#include "utils/NoCopy.hpp"
#include "utils/Unowned.hpp"
#include "utils/Utils.hpp"
#include "utils/ValueRestorer.hpp"
