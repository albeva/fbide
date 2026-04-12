//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerManager.hpp"
#include "lib/app/Context.hpp"
using namespace fbide;

// clang-format off
wxBEGIN_EVENT_TABLE(CompilerManager, wxEvtHandler)
wxEND_EVENT_TABLE()
// clang-format on

CompilerManager::CompilerManager(Context& ctx)
: m_ctx(ctx) {}

auto CompilerManager::compile() -> bool {
    // TODO: implement
    return false;
}

void CompilerManager::compileAndRun() {
    // TODO: implement
}

void CompilerManager::run() {
    // TODO: implement
}

void CompilerManager::quickRun() {
    // TODO: implement
}

void CompilerManager::openCmdPrompt() {
    // TODO: implement
}

void CompilerManager::showParametersDialog() {
    // TODO: implement
}

void CompilerManager::toggleShowExitCode() {
    // TODO: implement
}

void CompilerManager::toggleActivePath() {
    // TODO: implement
}

void CompilerManager::showCompilerLog() {
    // TODO: implement
}

void CompilerManager::goToError(int /*line*/, const wxString& /*fileName*/) {
    // TODO: implement
}

auto CompilerManager::ensureSaved() -> bool {
    // TODO: implement
    return false;
}

auto CompilerManager::buildCompileCommand() const -> wxString {
    // TODO: implement
    return {};
}

auto CompilerManager::buildRunCommand(const wxString& /*exePath*/) const -> wxString {
    // TODO: implement
    return {};
}

void CompilerManager::parseCompilerOutput(const wxArrayString& /*output*/) {
    // TODO: implement
}

void CompilerManager::runAsync(const wxString& /*command*/) {
    // TODO: implement
}

void CompilerManager::onProcessTerminated(int /*exitCode*/) {
    // TODO: implement
}
