//
// Created by Albert on 7/27/2020.
//
#include "LexerInterface.h"
using namespace fbide;

void LexerInterface::Log(const std::string &message) {
    wxLogMessage(wxString(message));
}
