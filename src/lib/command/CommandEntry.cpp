//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CommandEntry.hpp"
#include "config/ConfigManager.hpp"
using namespace fbide;

void CommandEntry::setEnabled(const bool state) {
    if (kind == wxITEM_DROPDOWN) {
        wxLogWarning("Trying to set enabled on menu '%s'", name);
        return;
    }
    enabled = state;
    update();
}

void CommandEntry::setForceDisabled(const bool state) {
    if (kind == wxITEM_DROPDOWN) {
        wxLogWarning("Trying to set forceDisabled on menu '%s'", name);
        return;
    }
    forceDisabled = state;
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
    const bool effectiveEnabled = isEnabled();
    const auto visitor = Visitor {
        [](wxMenu* /*menu*/) {},
        [this, effectiveEnabled](wxMenuItem* item) {
            bool refresh = false;
            if (item->IsEnabled() != effectiveEnabled) {
                item->Enable(effectiveEnabled);
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
        [this, effectiveEnabled](wxToolBarToolBase* tool) {
            bool refresh = false;
            if (tool->IsEnabled() != effectiveEnabled) {
                tool->Enable(effectiveEnabled);
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
        },
        [this](ConfigManager* configManager) {
            configManager->config()["commands"][name] = checked;
        }
    };
    for (const auto& bind : binds) {
        std::visit(visitor, bind);
    }
}
