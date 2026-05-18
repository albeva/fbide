//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AsyncProcess.hpp"
using namespace fbide;

auto AsyncProcess::exec(const wxString& command, const wxString& workingDir, const bool redirect, Callback&& callback, const wxString& input) -> AsyncProcess* {
    auto* self = new AsyncProcess(std::move(callback));
    self->exec(command, workingDir, redirect, input);
    return self;
}

AsyncProcess::AsyncProcess(Callback&& callback)
: wxProcess(nullptr)
, m_callback(std::move(callback)) {}

void AsyncProcess::exec(
    const wxString& command,
    const wxString& workingDir,
    const bool redirect,
    const wxString& input
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
        return;
    }

    // Feed the child's stdin, then close it so the process sees EOF. The
    // pipe buffer is small, so write in a loop to cover large payloads.
    if (!input.empty()) {
        if (auto* stream = GetOutputStream()) {
            const auto utf8 = input.utf8_string();
            const char* data = utf8.data();
            size_t remaining = utf8.size();
            while (remaining > 0 && stream->IsOk()) {
                stream->Write(data, remaining);
                const size_t written = stream->LastWrite();
                if (written == 0) {
                    break;
                }
                data += written;
                remaining -= written;
            }
            CloseOutput();
        }
    }
}

void AsyncProcess::kill() {
    const auto pid = static_cast<int>(GetPid());
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
