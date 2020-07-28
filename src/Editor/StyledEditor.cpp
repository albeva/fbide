//
//  Editor.cpp
//  fbide
//
//  Created by Albert on 09/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "StyledEditor.hpp"

using namespace fbide;

wxBEGIN_EVENT_TABLE(StyledEditor, wxStyledTextCtrl)
wxEND_EVENT_TABLE()

StyledEditor::StyledEditor() = default;

StyledEditor::~StyledEditor() = default;
