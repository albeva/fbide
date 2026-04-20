//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ui/controls/Layout.hpp"
#include "analyses/lexer/Token.hpp"

namespace fbide {
class Context;
class Editor;
class Renderer;
class Transform;
class Document;
enum class CaseMode;

/// Format dialog — keyword case conversion and code export.
class FormatDialog final : public Layout<wxDialog> {
public:
    NO_COPY_AND_MOVE(FormatDialog)

    FormatDialog(wxWindow* parent, Context& ctx, Document* doc);
    ~FormatDialog() override;
    void create();

private:
    void onTransformChanged(wxCommandEvent& event);
    void renderCode(wxCommandEvent& event);
    void renderHtml(wxCommandEvent& event);
    void onApply(wxCommandEvent& event);
    void onBrowser(wxCommandEvent& event);

    void updatePreview();
    void updateButtons();

    void rebuildTransforms();

    [[nodiscard]] auto isTransforming() const -> bool;
    [[nodiscard]] auto getKeywordCase() const -> std::optional<CaseMode>;

    Context& m_ctx;
    Document* m_doc;
    wxCharBuffer m_buffer;
    std::vector<lexer::Token> m_tokens;
    std::vector<std::unique_ptr<Transform>> m_transforms;
    std::unique_ptr<Renderer> m_renderer;

    Unowned<wxCheckBox> m_reindentCheck = nullptr;
    Unowned<wxCheckBox> m_reformatCheck = nullptr;
    Unowned<wxCheckBox> m_alignPPCheck = nullptr;
    Unowned<wxRadioButton> m_caseUnchanged = nullptr;
    Unowned<wxRadioButton> m_caseKeyWord = nullptr;
    Unowned<wxRadioButton> m_caseKEYWORD = nullptr;
    Unowned<wxRadioButton> m_casekeyword = nullptr;
    Unowned<Editor> m_preview = nullptr;
    Unowned<wxButton> m_actionBtn = nullptr;
    Unowned<wxButton> m_browserBtn = nullptr;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
