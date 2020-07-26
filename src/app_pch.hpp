/**
 * Pre Compiled Header
 */
#pragma once

// wxWidgets
#include <wx/apptrait.h>
#include <wx/aui/aui.h>
#include <wx/stc/stc.h>
#include <wx/stdpaths.h>
#include <wx/tokenzr.h>
#include <wx/wupdlock.h>
#include <wx/wx.h>

// std
#include <algorithm>
#include <any>
#include <variant>
#include <assert.h>
#include <cctype>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

// fbide
#include "Utils.hpp"
#define LOG_V(v) std::cout << #v " = " << (v) << std::endl;
