//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ArtiProvider.hpp"
#include "rc/icons.hpp"

using namespace fbide;

ArtiProvider::ArtiProvider()
: m_icons{
    { CommandId::New,           XPM::new_xpm      },
    { CommandId::Open,          XPM::open_xpm     },
    { CommandId::Save,          XPM::save_xpm     },
    { CommandId::SaveAll,       XPM::saveall_xpm  },
    { CommandId::Close,         XPM::close_xpm    },
    { CommandId::Cut,           XPM::cut_xpm      },
    { CommandId::Copy,          XPM::copy_xpm     },
    { CommandId::Paste,         XPM::paste_xpm    },
    { CommandId::Undo,          XPM::undo_xpm     },
    { CommandId::Redo,          XPM::redo_xpm     },
    { CommandId::Compile,       XPM::compile_xpm  },
    { CommandId::Run,           XPM::run_xpm      },
    { CommandId::CompileAndRun, XPM::compnrun_xpm },
    { CommandId::QuickRun,      XPM::qrun_xpm     },
    { CommandId::Result,        XPM::output_xpm   },
} {}

auto ArtiProvider::getBitmap(const CommandId id) const -> wxBitmap {
    const auto it = m_icons.find(id);
    if (it == m_icons.end()) {
        return wxNullBitmap;
    }
    wxBitmap bitmap(it->second);
    const auto mask = make_unowned<wxMask>(bitmap, wxColour(192, 192, 192));
    bitmap.SetMask(mask);
    return bitmap;
}
