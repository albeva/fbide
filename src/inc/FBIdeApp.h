/*
* This file is part of FBIde, an open-source (cross-platform) IDE for
* FreeBasic compiler.
* Copyright (C) 2020  Albert Varaksin
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
* Contact e-mail: Albert Varaksin <albeva@me.com>
* Program URL: https://github.com/albeva/fbide
*/
#pragma once
#include "pch.h"

class FBIdeMainFrame;
class InstanceHandler;

class FBIdeApp final: public wxApp {
public:
    bool OnInit() final;

    [[nodiscard]] FBIdeMainFrame* GetMainFrame() const { return m_frame; }

private:
    std::unique_ptr<InstanceHandler> m_instanceHandler;
    FBIdeMainFrame* m_frame = nullptr;
};

wxDECLARE_APP(FBIdeApp);
