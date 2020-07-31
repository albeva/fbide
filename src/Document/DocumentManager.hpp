//
// Created by Albert on 7/29/2020.
//
#pragma once
#include "app_pch.hpp"

namespace fbide {

class DocumentManager final: public wxEvtHandler {
    NON_COPYABLE(DocumentManager)
public:

    DocumentManager();
    ~DocumentManager() final;

private:
    void OnNew(wxCommandEvent& event);
    void OnOpen(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE(); // NOLINT
};

} // namespace fbide
