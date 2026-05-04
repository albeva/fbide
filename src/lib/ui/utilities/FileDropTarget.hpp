//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <wx/dnd.h>

namespace fbide {
class Context;

/// File drop target for the main frame. Accepts files dropped from the
/// OS file manager and routes them to `DocumentManager::openFile`. Only
/// files matching one of the configured `[filePatterns]` globs are
/// opened; the catch-all `all` (`*.*`) entry is ignored so unrelated
/// files do not slip through.
class FileDropTarget final : public wxFileDropTarget {
public:
    NO_COPY_AND_MOVE(FileDropTarget)

    explicit FileDropTarget(Context& ctx);
    ~FileDropTarget() override = default;

    /// Open every dropped file whose name matches a known pattern.
    /// Returns true if at least one file was accepted.
    auto OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) -> bool override;

private:
    /// True if `filename` matches any glob from `[filePatterns]` other
    /// than the `all` catch-all entry.
    [[nodiscard]] auto isSupported(const wxString& filename) const -> bool;

    Context& m_ctx; ///< Application context.
};

} // namespace fbide
