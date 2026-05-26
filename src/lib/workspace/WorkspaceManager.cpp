//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "WorkspaceManager.hpp"

using namespace fbide;

void WorkspaceManager::setActiveDocument(Document* /*doc*/) {
    // Phase 5 will populate m_activeProject from doc->getProject(); until
    // documents carry a project back-link, the active project stays null.
    m_activeProject = nullptr;
}
