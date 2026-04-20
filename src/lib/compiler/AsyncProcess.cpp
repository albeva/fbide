//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AsyncProcess.hpp"
#include <wx/txtstrm.h>
using namespace fbide;

auto AsyncProcess::exec(const wxString& command, const wxString& workingDir, const bool redirect, Callback&& callback) -> AsyncProcess* {
    auto* self = new AsyncProcess(std::move(callback));
    self->exec(command, workingDir, redirect);
    return self;
}

AsyncProcess::AsyncProcess(Callback&& callback)
: wxProcess(nullptr)
, m_callback(std::move(callback)) {}

void AsyncProcess::exec(
    const wxString& command,
    const wxString& workingDir,
    const bool redirect
) {
    if (redirect) {
        Redirect();
    }

    wxExecuteEnv env;
    env.cwd = workingDir;
    const long pid = wxExecute(command, wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER, this, &env);

    if (pid == 0) {
        m_callback(ProcessResult {});
        delete this; // NOLINT(*-owning-memory)
    }
}

void AsyncProcess::kill() {
    const auto pid = GetPid();
    if (pid == 0) {
        return;
    }
    const auto err = wxProcess::Kill(pid, wxSIGKILL, wxKILL_CHILDREN);
    if (err != wxKILL_OK) {
        wxLogError("Failed to kill process %ld: error %d", pid, static_cast<int>(err));
    }
}

void AsyncProcess::OnTerminate(int /*pid*/, const int status) {
    ProcessResult result;
    result.exitCode = status;
    result.launched = true;

    if (IsRedirected()) {
        readStream(GetInputStream(), result.output);
        readStream(GetErrorStream(), result.output);
    }

    m_callback(std::move(result));
    delete this; // NOLINT(*-owning-memory)
}

void AsyncProcess::readStream(wxInputStream* stream, wxArrayString& output) {
    if (stream == nullptr) {
        return;
    }
    wxTextInputStream text(*stream);
    while (!stream->Eof()) {
        auto line = text.ReadLine();
        if (!line.Strip(wxString::both).empty() || !stream->Eof()) {
            output.Add(std::move(line));
        }
    }
}
