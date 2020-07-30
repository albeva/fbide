//
// Created by Albert on 30/07/2020.
//
#pragma once
#include "app_pch.hpp"

namespace fbide {

/**
 * App is the basic entry point into FBIde
 */
class App final : public wxApp {
public:
    using wxApp::wxApp;

    bool OnInit() final;
    int OnExit() final;

    void ExitFBIde();

private:
    void LoadScintillaFBLexer();
    wxString GetIdePath();
};

}

DECLARE_APP(fbide::App)
