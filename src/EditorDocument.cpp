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

using namespace fbide;

namespace {
    bool _needToloadLexerLibrary = true;
}


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


    if (_needToloadLexerLibrary) {
        _needToloadLexerLibrary = false;
        #if __DARWIN__
            auto path = GetConfig("BasePath").AsString() / "libfblexer.dylib";
            m_editor.LoadLexerLibrary(path);
        #elif __WXMSW__
            auto path = GetConfig("IdePath").AsString() / "fblexer.dll";
            m_editor.LoadLexerLibrary(path);
        #endif // __WXMSW__
    }
    m_editor.SetLexerLanguage("fbide-freebasic");
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
