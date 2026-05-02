//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ArtiProvider.hpp"
#include "analyses/symbols/SymbolTable.hpp"
#include "rc/icons.hpp"

using namespace fbide;

ArtiProvider::ArtiProvider()
: m_commandIcons {
    { CommandId::New, XPM::new_xpm },
    { CommandId::Open, XPM::open_xpm },
    { CommandId::Save, XPM::save_xpm },
    { CommandId::SaveAll, XPM::saveall_xpm },
    { CommandId::Close, XPM::close_xpm },
    { CommandId::Cut, XPM::cut_xpm },
    { CommandId::Copy, XPM::copy_xpm },
    { CommandId::Paste, XPM::paste_xpm },
    { CommandId::Undo, XPM::undo_xpm },
    { CommandId::Redo, XPM::redo_xpm },
    { CommandId::Compile, XPM::compile_xpm },
    { CommandId::Run, XPM::run_xpm },
    { CommandId::CompileAndRun, XPM::compnrun_xpm },
    { CommandId::QuickRun, XPM::qrun_xpm },
    { CommandId::KillProcess, XPM::compresx_xpm },
    { CommandId::Result, XPM::output_xpm },
    { CommandId::Browser, XPM::browse_xpm }
}
, m_symbolIcons {
    { SymbolKind::Sub, XPM::sym_sub_xpm },
    { SymbolKind::Function, XPM::sym_function_xpm },
    { SymbolKind::Type, XPM::sym_type_xpm },
    { SymbolKind::Union, XPM::sym_union_xpm },
    { SymbolKind::Enum, XPM::sym_enum_xpm },
    { SymbolKind::Macro, XPM::sym_macro_xpm },
    { SymbolKind::Include, XPM::sym_include_xpm }
} {}

auto ArtiProvider::getBitmap(const CommandId id) const -> wxBitmap {
    const auto it = m_commandIcons.find(id);
    if (it == m_commandIcons.end()) {
        return wxNullBitmap;
    }
    return make(it->second);
}

auto ArtiProvider::getBitmap(const SymbolKind kind) const -> wxBitmap {
    const auto it = m_symbolIcons.find(kind);
    if (it == m_symbolIcons.end()) {
        return wxNullBitmap;
    }
    return make(it->second);
}

auto ArtiProvider::make(const char* const* xpm) const -> wxBitmap {
    wxBitmap bitmap(xpm);
    const auto mask = make_unowned<wxMask>(bitmap, wxColour(192, 192, 192));
    bitmap.SetMask(mask);
    return bitmap;
}
