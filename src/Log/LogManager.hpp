//
// Created by Albert on 7/27/2020.
//
#pragma once
#include "app_pch.hpp"

namespace fbide {

class LogManager final: NonCopyable {
public:

    LogManager();
    ~LogManager();

private:
    wxTextCtrl* m_textCtrl;
    std::unique_ptr<wxLogTextCtrl> m_log;
};

} // namespace fbide
