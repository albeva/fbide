//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CommandEntry.hpp"
using namespace fbide;

void CommandEntry::setEnabled(const bool state) {
    if (kind == wxITEM_DROPDOWN) {
        wxLogWarning("Trying to set enabled on menu '%s'", name);
        return;
    }
    enabled = state;
    update();
}

void CommandEntry::setChecked(const bool state) {
    if (kind != wxITEM_CHECK) {
        wxLogWarning("Trying to set checked state on '%s'", name);
        return;
    }
    checked = state;
    update();
}

void CommandEntry::update() const {
    const auto visitor = Visitor {
        [&](wxMenu* /*menu*/) {},
        [&](wxMenuItem* item) {
            item->Enable(enabled);
            if (item->IsCheckable()) {
                item->Check(checked);
            }
        },
        [&](wxToolBarToolBase* tool) {
            tool->Enable(enabled);
            if (tool->CanBeToggled()) {
                tool->Toggle(checked);
            }
        }
    };
    for (const auto& bind: binds) {
        std::visit(visitor, bind);
    }
}
