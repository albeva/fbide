//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FormatDialog.hpp"
#include "renderers/BBCodeRenderer.hpp"
#include "formatters/CaseTransform.hpp"
#include "renderers/HtmlRenderer.hpp"
#include "Lexer.hpp"
#include "renderers/PlainTextRenderer.hpp"
#include "formatters/ReindentTransform.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Keywords.hpp"
#include "lib/config/Lang.hpp"
#include "lib/config/Theme.hpp"
#include "lib/editor/Document.hpp"
#include "lib/editor/DocumentManager.hpp"
#include "lib/editor/Editor.hpp"
#include "lib/ui/UIManager.hpp"
#include <wx/clipbrd.h>
using namespace fbide;

namespace {

enum ControlId {
    ID_REINDENT = wxID_HIGHEST + 1,
    ID_CASE_UNCHANGED,
    ID_CASE_KEYWORD,
    ID_CASE_KEYWORD_UPPER,
    ID_CASE_KEYWORD_LOWER,
    ID_RENDER_CODE,
    ID_RENDER_HTML,
    ID_RENDER_BBCODE,
    ID_BROWSER,
};

} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(FormatDialog, wxDialog)
    EVT_CHECKBOX(ID_REINDENT,               FormatDialog::onTransformChanged)
    EVT_RADIOBUTTON(ID_CASE_UNCHANGED,      FormatDialog::onTransformChanged)
    EVT_RADIOBUTTON(ID_CASE_KEYWORD,        FormatDialog::onTransformChanged)
    EVT_RADIOBUTTON(ID_CASE_KEYWORD_UPPER,  FormatDialog::onTransformChanged)
    EVT_RADIOBUTTON(ID_CASE_KEYWORD_LOWER,  FormatDialog::onTransformChanged)
    EVT_RADIOBUTTON(ID_RENDER_CODE,         FormatDialog::renderCode)
    EVT_RADIOBUTTON(ID_RENDER_HTML,         FormatDialog::renderHtml)
    EVT_RADIOBUTTON(ID_RENDER_BBCODE,       FormatDialog::renderBBCode)
    EVT_BUTTON(wxID_OK,                     FormatDialog::onApply)
    EVT_BUTTON(ID_BROWSER,                  FormatDialog::onBrowser)
wxEND_EVENT_TABLE()
// clang-format on

FormatDialog::FormatDialog(wxWindow* parent, Context& ctx)
: wxDialog(
      parent, wxID_ANY, ctx.getLang()[LangId::ViewFormatTitle],
      wxDefaultPosition, wxDefaultSize,
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
  )
, m_ctx(ctx) {}

FormatDialog::~FormatDialog() = default;

void FormatDialog::create() {
    const auto mainSizer = make_unowned<wxBoxSizer>(wxVERTICAL);

    // Options bar
    {
        const auto row = make_unowned<wxBoxSizer>(wxHORIZONTAL);

        m_reindentCheck = make_unowned<wxCheckBox>(this, ID_REINDENT, "Re-indent");
        row->Add(m_reindentCheck, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

        row->Add(make_unowned<wxStaticLine>(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL),
            0, wxEXPAND | wxLEFT | wxRIGHT, 5);

        m_caseUnchanged = make_unowned<wxRadioButton>(this, ID_CASE_UNCHANGED, "Unchanged", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
        m_caseKeyWord = make_unowned<wxRadioButton>(this, ID_CASE_KEYWORD, "KeyWord");
        m_caseKEYWORD = make_unowned<wxRadioButton>(this, ID_CASE_KEYWORD_UPPER, "KEYWORD");
        m_casekeyword = make_unowned<wxRadioButton>(this, ID_CASE_KEYWORD_LOWER, "keyword");
        m_caseUnchanged->SetValue(true);

        row->Add(m_caseUnchanged, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        row->Add(m_caseKeyWord, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        row->Add(m_caseKEYWORD, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        row->Add(m_casekeyword, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

        row->Add(make_unowned<wxStaticLine>(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL),
            0, wxEXPAND | wxLEFT | wxRIGHT, 5);

        m_outputFormat = make_unowned<wxRadioButton>(this, ID_RENDER_CODE, "Format", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
        m_outputHtml = make_unowned<wxRadioButton>(this, ID_RENDER_HTML, "As HTML");
        m_outputBBCode = make_unowned<wxRadioButton>(this, ID_RENDER_BBCODE, "As BBCode");
        m_outputFormat->SetValue(true);

        row->Add(m_outputFormat, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        row->Add(m_outputHtml, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        row->Add(m_outputBBCode, 0, wxALIGN_CENTER_VERTICAL);

        mainSizer->Add(row, 0, wxEXPAND | wxALL, 5);
    }

    // Preview label
    {
        const auto label = make_unowned<wxStaticText>(this, wxID_ANY, "Preview");
        label->SetFont(label->GetFont().Bold());
        mainSizer->Add(label, 0, wxLEFT | wxRIGHT | wxTOP, 5);
    }

    // Preview editor
    {
        m_preview = make_unowned<Editor>(this, m_ctx, DocumentType::FreeBASIC, true);
        m_preview->SetReadOnly(true);
        m_preview->SetMinSize(wxSize(450, 250));
        mainSizer->Add(m_preview, 1, wxEXPAND | wxALL, 5);
    }

    // Buttons
    {
        const auto btnSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);

        m_browserBtn = make_unowned<wxButton>(this, ID_BROWSER, "Open in Browser");
        btnSizer->Add(m_browserBtn, 0, wxRIGHT, 5);

        btnSizer->AddStretchSpacer();

        m_actionBtn = make_unowned<wxButton>(this, wxID_OK, "Apply");
        btnSizer->Add(m_actionBtn, 0, wxRIGHT, 5);
        btnSizer->Add(make_unowned<wxButton>(this, wxID_CANCEL), 0);

        mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    }

    SetSizer(mainSizer);
    SetMinSize(wxSize(700, 450));
    Fit();
    Centre();

    // Tokenise source once
    const auto source = getSourceText();
    if (!source.empty()) {
        const Lexer lexer(m_ctx.getKeywords());
        m_tokens = lexer.tokenise(source);
    }

    m_renderer = std::make_unique<PlainTextRenderer>();
    updatePreview();
}

void FormatDialog::onTransformChanged(wxCommandEvent&) {
    rebuildTransforms();
    updatePreview();
}

void FormatDialog::renderCode(wxCommandEvent&) {
    m_renderer = std::make_unique<PlainTextRenderer>();
    updatePreview();
}

void FormatDialog::renderHtml(wxCommandEvent&) {
    m_renderer = std::make_unique<HtmlRenderer>(m_ctx.getTheme());
    updatePreview();
}

void FormatDialog::renderBBCode(wxCommandEvent&) {
    m_renderer = std::make_unique<BBCodeRenderer>(m_ctx.getTheme());
    updatePreview();
}

void FormatDialog::onApply(wxCommandEvent&) {
    const auto rendered = m_preview->GetText();
    if (rendered.IsEmpty()) {
        return;
    }

    if (m_renderer->getType() == DocumentType::FreeBASIC) {
        auto* doc = m_ctx.getDocumentManager().getActive();
        if (doc != nullptr) {
            auto* editor = doc->getEditor();
            editor->BeginUndoAction();
            editor->SetText(rendered);
            editor->EndUndoAction();
        }
    } else {
        if (!m_tokens.empty()) {
            if (wxTheClipboard->Open()) {
                wxTheClipboard->SetData(make_unowned<wxTextDataObject>(rendered));
                wxTheClipboard->Close();
            }
        }
    }

    EndModal(wxID_OK);
}

void FormatDialog::onBrowser(wxCommandEvent&) {
    if (m_renderer->getType() != DocumentType::HTML) {
        return;
    }

    const auto rendered = m_preview->GetText();
    if (rendered.IsEmpty()) {
        return;
    }

    const auto tmpFile = wxFileName::CreateTempFileName("fbide_html") + ".html";
    wxFile file(tmpFile, wxFile::write);
    if (file.IsOpened()) {
        file.Write(HtmlRenderer::decorate(rendered));
        file.Close();
        wxLaunchDefaultBrowser("file:///" + tmpFile);
    }
}

void FormatDialog::rebuildTransforms() {
    m_transforms.clear();

    if (m_reindentCheck->IsChecked()) {
        m_transforms.push_back(std::make_unique<ReindentTransform>(m_ctx.getConfig().getTabSize()));
    }

    if (const auto mode = getKeywordCase()) {
         m_transforms.push_back(std::make_unique<CaseTransform>(*mode));
    }
}

auto FormatDialog::getKeywordCase() const -> std::optional<CaseMode> {
    if (m_caseKeyWord->GetValue()) return CaseMode::Mixed;
    if (m_caseKEYWORD->GetValue()) return CaseMode::Upper;
    if (m_casekeyword->GetValue()) return CaseMode::Lower;
    return std::nullopt;
}

auto FormatDialog::getSourceText() const -> wxString {
    if (const auto* doc = m_ctx.getDocumentManager().getActive()) {
        return doc->getEditor()->GetText();
    }
    return {};
}

auto FormatDialog::isTransforming() const -> bool {
    return not m_transforms.empty() || m_renderer->getType() != DocumentType::FreeBASIC;
}

void FormatDialog::updateButtons() {
    const auto docTy = m_renderer->getType();
    if (docTy != DocumentType::FreeBASIC) {
        m_actionBtn->SetLabel("Copy");
    } else {
        m_actionBtn->SetLabel("Apply");
    }
    m_actionBtn->Enable(isTransforming());
    m_browserBtn->Show(docTy == DocumentType::HTML);
}

void FormatDialog::updatePreview() {
    updateButtons();

    if (m_tokens.empty()) {
        return;
    }

    const auto thaw = FreezeLock(this);
    m_preview->SetReadOnly(false);
    m_preview->setDocType(m_renderer->getType());
    if (isTransforming()) {
        m_preview->SetText(m_renderer->render(m_tokens, m_transforms));
    } else {
        m_preview->SetText(getSourceText());
    }
    m_preview->SetReadOnly(true);
}
