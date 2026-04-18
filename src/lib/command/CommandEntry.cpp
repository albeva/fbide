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

void CommandEntry::update() {
    const auto visitor = Visitor {
        [](wxMenu* /*menu*/) {},
        [this](wxMenuItem* item) {
            bool refresh = false;
            if (item->IsEnabled() != enabled) {
                item->Enable(enabled);
                refresh = true;
            }
            if (item->IsCheckable() && item->IsChecked() != checked) {
                item->Check(checked);
                refresh = true;
            }
            if (refresh) {
                item->GetMenu()->UpdateUI();
            }
        },
        [this](wxToolBarToolBase* tool) {
            bool refresh = false;
            if (tool->IsEnabled() != enabled) {
                tool->Enable(enabled);
                refresh = true;
            }
            if (tool->CanBeToggled() && tool->IsToggled() != checked) {
                tool->Toggle(checked);
                refresh = true;
            }
            if (refresh) {
                tool->GetToolBar()->Realize();
            }
        },
        [this](wxAuiManager* aui) {
            auto& pane = aui->GetPane(name);
            if (pane.IsShown() != checked) {
                pane.Show(checked);
                aui->Update();
            }
        }
    };
    for (const auto& bind: binds) {
        std::visit(visitor, bind);
    }
}
