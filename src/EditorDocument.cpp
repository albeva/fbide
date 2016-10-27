//
//  Editor.cpp
//  fbide
//
//  Created by Albert on 09/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "EditorDocument.hpp"
#include "UiManager.hpp"
#include "StyledEditor.hpp"
#include "MainWindow.hpp"

using namespace fbide;


const wxString EditorDocument::Plain = "text/plain";
const wxString EditorDocument::Freebasic = "text/freebasic";


EditorDocument::~EditorDocument()
{
}


/**
 * Instantiate the document
 */
void EditorDocument::Create()
{
    auto & ui = GetUiMgr();
    auto da = ui.GetDocArea();
    
    wxWindowUpdateLocker lock(ui.GetWindow());

    m_editor.Create(da);
    da->AddPage(&m_editor, GetTitle(), true);
    ui.BindCloser(&m_editor, [this, da]() {
        auto idx = da->GetPageIndex(&m_editor);
        if (idx != wxNOT_FOUND) {
            da->RemovePage(idx);
        }
        delete this;
    });
    
    // editor configuration
    auto & config = GetType().config;
    m_editor.SetLexerLanguage(config.Get("lexer", "null"));
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
