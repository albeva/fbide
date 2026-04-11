//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "App.hpp"

namespace fbide {

auto App::OnInit() -> bool {
    const auto frame = make_unowned<wxFrame>(nullptr, wxID_ANY, "FBIde");
    frame->Show();
    return true;
}

} // namespace fbide
