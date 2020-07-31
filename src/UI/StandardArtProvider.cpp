/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */
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
} // namespace XPM

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
