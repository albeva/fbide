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
        [this, effectiveEnabled](wxAuiToolBar* tb) {
            // wxAuiToolBar enable / toggle / state queries are id-keyed
            // on the parent toolbar; the per-tool wxAuiToolBarItem has
            // no public mutators of its own. Refresh() is enough — no
            // Realize() needed for state changes (only when adding /
            // removing tools).
            auto* item = tb->FindTool(id);
            if (item == nullptr) {
                return;
            }
            bool refresh = false;
            if (tb->GetToolEnabled(id) != effectiveEnabled) {
                tb->EnableTool(id, effectiveEnabled);
                refresh = true;
            }
            if (kind == wxITEM_CHECK && tb->GetToolToggled(id) != checked) {
                tb->ToggleTool(id, checked);
                refresh = true;
            }
            if (refresh) {
                tb->Refresh(false);
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
