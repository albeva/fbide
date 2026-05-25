//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;

/**
 * Find / Replace / Goto Line for the focused editor.
 *
 * Owns the `wxFindReplaceData` that carries search state between
 * dialog invocations and routes `wxFindDialogEvent` events back into
 * the active editor's find / replace pipeline. Lives outside
 * `DocumentManager` because every operation here is editor-bound and
 * never touches the document collection.
 *
 * **Owns:** the `wxFindReplaceData` state. Dialog widgets are
 * wx-parented to the main frame and clean themselves up on close.
 * **Owned by:** `Context`.
 * **Threading:** UI thread only.
 */
class EditorSearchService final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(EditorSearchService)

    /// Construct without showing any dialog. `ctx` is borrowed by
    /// reference and used to reach the active document and the main
    /// frame each time a dialog is needed.
    explicit EditorSearchService(Context& ctx);
};

} // namespace fbide
