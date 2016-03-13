//
//  Editor.cpp
//  fbide
//
//  Created by Albert on 09/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "app_pch.hpp"
#include "EditorDocument.hpp"
#include "UiManager.hpp"
#include "Editor.hpp"

using namespace fbide;


/**
 * Instantiate the document
 */
void EditorDocument::Create()
{
    auto & ui = GetUiMgr();
    auto da = ui.GetDocArea();
    
    m_editor.Create(da);
    da->AddPage(&m_editor, GetTitle(), true);
    ui.BindCloser(&m_editor, [this, da]() {
        auto idx = da->GetPageIndex(&m_editor);
        if (idx != wxNOT_FOUND) {
            da->RemovePage(idx);
        }
        delete this;
    });
}


/**
 * Load specified file. Will Create the instance
 */
void EditorDocument::Load(const wxString & filename)
{
}


/**
 * Save the document
 */
void EditorDocument::Save(const wxString & filename)
{
}