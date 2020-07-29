//
// Created by Albert on 7/27/2020.
//
#pragma once
#include "app_pch.hpp"
#include "UI/PanelHandler.hpp"

namespace fbide {

class LogManager final: public Panel {
    NON_COPYABLE(LogManager)
public:
    LogManager();
    ~LogManager();

    wxWindow* ShowPanel() final;
    bool HidePanel() final;

private:
    wxLogBuffer m_buffer;
    wxTextCtrl* m_textCtrl;
    std::unique_ptr<wxLogTextCtrl> m_log;
};

} // namespace fbide