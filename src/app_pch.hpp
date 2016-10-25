/**
 * Pre Compiled Header
 */
#ifndef FBIDE_APP_PCH_HPP
#define FBIDE_APP_PCH_HPP

// wxWidgets
#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/apptrait.h>
#include <wx/aui/aui.h>
#include <wx/tokenzr.h>
#include <wx/wupdlock.h>

// std
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <memory>
#include <functional>
#include <algorithm>
#include <utility>
#include <cctype>
#include <assert.h>
#include <type_traits>

// boost
#include <boost/any.hpp>

// fbide
#include "Utils.hpp"
#include "Manager.hpp"
#include "Config.hpp"

#include "wxstc/include/wx/stc/stc.h"

#endif // FBIDE_APP_PCH_HPP
