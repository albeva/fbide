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
#include "StyleEntry.hpp"
#include "Config.hpp"
using namespace fbide;

StyleEntry::StyleEntry(const Config& style, const StyleEntry* parent) {
    if (parent != nullptr) {
        font = parent->font;
    } else {
        font = wxFont(
            style.Get(Defaults::Key::FontSize, Defaults::FontSize),
            wxFONTFAMILY_MODERN,
            wxFONTSTYLE_NORMAL,
            wxFONTWEIGHT_NORMAL,
            false,
            style.Get(Defaults::Key::FontName, wxEmptyString));
    }

    #define INIT_FIELD(NAME, DEF) NAME = style.Get(#NAME, parent != nullptr ? parent->NAME : DEF);
    DEFAULT_EDITOR_STYLE(INIT_FIELD)
    #undef INIT_FIELD
}
