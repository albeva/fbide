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
#pragma once
#include "pch.h"
#include "UI/PanelHandler.hpp"

namespace fbide {

class LogManager final: public Panel {
    NON_COPYABLE(LogManager)
public:
    LogManager();
    ~LogManager() final;

    wxWindow* ShowPanel() final;
    bool HidePanel() final;

private:
    wxTextCtrl* m_textCtrl;
    std::unique_ptr<wxLogTextCtrl> m_log;
};

} // namespace fbide
