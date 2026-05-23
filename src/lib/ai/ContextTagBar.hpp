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
} // namespace fbide

namespace fbide::ai {

/// Emitted by `ContextTagBar` after a tag is removed, so the host can
/// re-lay its sizer (the bar's height changes / it may hide).
wxDECLARE_EVENT(EVT_CONTEXT_TAGS_CHANGED, wxCommandEvent);

/**
 * Strip of "tag" chips — one per file attached to the AI conversation —
 * shown between the chat view and the input box.
 *
 * Each chip shows the file name and a close button that drops it from the
 * conversation context. The bar hides itself while nothing is attached.
 *
 * **Owns:** its chip widgets (wx-parented).
 * **Owned by:** `AiChatPanel` (wx-parented).
 */
class ContextTagBar final : public wxPanel {
public:
    NO_COPY_AND_MOVE(ContextTagBar)

    /// Build the bar as a child of `parent`.
    ContextTagBar(wxWindow* parent, Context& ctx);

    /// Rebuild the chips from the current AI context; show / hide the bar.
    void refresh();

private:
    /// Drop the context item at `index`, rebuild, and notify the host.
    void removeItem(std::size_t index);

    Context& m_ctx; ///< Application context — reaches the AI context.
};

} // namespace fbide::ai
