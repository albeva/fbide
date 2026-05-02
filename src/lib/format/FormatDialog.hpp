//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "analyses/lexer/Token.hpp"
#include "ui/controls/Layout.hpp"

namespace fbide {
class Context;
class Editor;
class Renderer;
class Transform;
class Document;
class CaseMode;

/// Format dialog — pipeline-driven preview of `Reformat`, `CaseTransform`,
/// and `HtmlRenderer` transforms applied to the document.
class FormatDialog final : public Layout<wxDialog> {
public:
    NO_COPY_AND_MOVE(FormatDialog)

    /// Construct without populating widgets; `create()` builds the UI.
    FormatDialog(wxWindow* parent, Context& ctx, Document* doc);
    /// Tear down owned transforms and renderer.
    ~FormatDialog() override;
    /// Build the dialog widgets.
    void create();

private:
    /// Any transform-toggle checkbox flipped — rebuild and re-render.
    void onTransformChanged(wxCommandEvent& event);
    /// "Render Code" — emit formatted text into the preview pane.
    void renderCode(wxCommandEvent& event);
    /// "Render HTML" — emit HTML into the preview pane.
    void renderHtml(wxCommandEvent& event);
    /// "Apply" — replace the source document's text with the preview.
    void onApply(wxCommandEvent& event);
    /// "Browser" — open the rendered HTML in the system browser.
    void onBrowser(wxCommandEvent& event);

    /// Re-run the pipeline and push the result into the preview editor.
    void updatePreview();
    /// Sync Apply/Browser button enabled state with the active transform set.
    void updateButtons();

    /// Rebuild `m_transforms` and `m_renderer` from the current checkbox state.
    void rebuildTransforms();

    /// True when at least one transform is active.
    [[nodiscard]] auto isTransforming() const -> bool;

    Context& m_ctx;                                            ///< Application context.
    Document* m_doc;                                           ///< Source document being previewed.
    wxCharBuffer m_buffer;                                     ///< UTF-8 snapshot of the document text.
    std::vector<lexer::Token> m_tokens;                        ///< Lexed tokens, fed into the transform chain.
    std::vector<std::unique_ptr<Transform>> m_transforms;      ///< Active transforms, in pipeline order.
    std::unique_ptr<Renderer> m_renderer;                      ///< Active renderer (text or HTML).

    Unowned<wxCheckBox> m_reindentCheck = nullptr;             ///< "Re-indent" toggle.
    Unowned<wxCheckBox> m_reformatCheck = nullptr;             ///< "Re-format" toggle.
    Unowned<wxCheckBox> m_alignPPCheck = nullptr;              ///< "Anchored PP" toggle.
    Unowned<wxCheckBox> m_applyCaseCheck = nullptr;            ///< "Apply keyword case" toggle.
    Unowned<Editor> m_preview = nullptr;                       ///< Preview editor (read-only Editor).
    Unowned<wxButton> m_actionBtn = nullptr;                   ///< Apply button.
    Unowned<wxButton> m_browserBtn = nullptr;                  ///< Open-in-browser button (HTML mode).

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
