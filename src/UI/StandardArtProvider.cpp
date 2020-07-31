//
//  StandardArtProvider.cpp
//  fbide
//
//  Created by Albert on 02/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "StandardArtProvider.hpp"

using namespace fbide;

namespace {
namespace XPM {
#include "xpm/close.xpm"
#include "xpm/about.xpm"

#include "xpm/compile.xpm"
#include "xpm/comprun.xpm"
#include "xpm/rebuild.xpm"
#include "xpm/run.xpm"

#include "xpm/cut.xpm"
#include "xpm/copy.xpm"
#include "xpm/paste.xpm"

#include "xpm/newsrc.xpm"
#include "xpm/openproj.xpm"
#include "xpm/save.xpm"

#include "xpm/undo.xpm"
#include "xpm/redo.xpm"

#include "xpm/search.xpm"
#include "xpm/srchrep.xpm"
#include "xpm/xpm_goto.xpm"

#include "xpm/toggle_log.xpm"
#include "xpm/view-fullscreen-4.xpm"
}; // namespace XPM

// map containing the icons
const StringMap<wxBitmap> _icons{ // NOLINT
    { "quit",       XPM::close },
    { "about",      XPM::about },

    { "compile",    XPM::compile },
    { "comprun",    XPM::comprun },
    { "rebuild",    XPM::rebuild },
    { "run",        XPM::run },

    { "cut",        XPM::cut },
    { "copy",       XPM::copy },
    { "paste",      XPM::paste },

    { "new",        XPM::newsrc },
    { "open",       XPM::openproj },
    { "save",       XPM::save },

    { "undo",       XPM::undo },
    { "redo",       XPM::redo },

    { "find",       XPM::search },
    { "replace",    XPM::srchrep },
    { "goto",       XPM::xpm_goto },

    { "toggle_log", XPM::toggle_log },
    { "fullscreen", XPM::view_fullscreen__ }
};

// default icon size
const wxSize _size{ 16, 16 }; // NOLINT
} // namespace

const wxBitmap& StandardArtProvider::GetIcon(const wxString& name) {
    auto icon = _icons.find(name);
    if (icon != _icons.end()) {
        return icon->second;
    }

    return wxNullBitmap;
}

const wxSize& StandardArtProvider::GetIconSize() {
    return _size;
}
