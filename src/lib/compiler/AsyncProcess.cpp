//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AsyncProcess.hpp"
using namespace fbide;

auto AsyncProcess::exec(const wxString& command, const wxString& workingDir, const bool redirect, Callback&& callback) -> AsyncProcess* {
    auto* self = new AsyncProcess(std::move(callback));
    if (!self->exec(command, workingDir, redirect)) {
        // Launch failed: `self` already invoked the callback and deleted
        // itself. Hand back nullptr so the caller's pointer stays honest.
        return nullptr;
    }
    return self;
}

AsyncProcess::AsyncProcess(Callback&& callback)
: wxProcess(nullptr)
, m_callback(std::move(callback)) {}

auto AsyncProcess::exec(
    const wxString& command,
    const wxString& workingDir,
    const bool redirect
) -> bool {
    if (redirect) {
        Redirect();
    }

    wxExecuteEnv env;
    env.cwd = workingDir;
    const long pid = wxExecute(command, wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER, this, &env);

    if (pid == 0) {
        m_callback(ProcessResult {});
        delete this; // NOLINT(*-owning-memory)
        return false;
    }
    return true;
}

void AsyncProcess::detach() {
    // Sever the termination callback. The callback captures its owner by
    // reference; once that owner is gone, a still-pending OnTerminate must
    // not touch it. The object still self-deletes on termination as usual.
    m_callback = nullptr;
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

    if (m_callback) {
        m_callback(std::move(result));
    }
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
