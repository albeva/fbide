//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "StatusBarHandler.hpp"
#include "DocumentTypeMenu.hpp"
#include "EncodingMenu.hpp"
#include "app/Context.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
using namespace fbide;

namespace {
/// Field widths tuned to typical content:
///   line:col (≤ "9999 : 999"), document type (longest is "Properties"),
///   configuration (≈ "FBC 32bit GUI"), EOL ("CRLF"), encoding
///   ("Windows-1252"). Tighter than the legacy values — the older
///   90/100/90/140 layout left a lot of empty space on the right.
constexpr std::array kWidthsNoConfig { -1, 80, 90, 60, 110 };
constexpr std::array kWidthsWithConfig { -1, 80, 90, 130, 60, 110 };
} // namespace

void StatusBarHandler::create(wxFrame* frame) {
    m_frame = frame;
    // Build with the widest layout up-front so the click binding is
    // wired once. `applyPreference` shrinks back to 5 fields when the
    // preference is off.
    m_bar = frame->CreateStatusBar(static_cast<int>(kWidthsWithConfig.size()));
    m_bar->SetStatusWidths(static_cast<int>(kWidthsWithConfig.size()), kWidthsWithConfig.data());
    frame->SetStatusText(m_ctx.tr("common.welcome"));
    m_bar->Bind(wxEVT_LEFT_DOWN, &StatusBarHandler::onClick, this);
    applyPreference();
}

void StatusBarHandler::applyPreference() {
    if (m_bar == nullptr) {
        return;
    }
    m_hasConfigField = m_ctx.getConfigManager().config().get_or("commands.configurationInStatusBar", false);
    const std::span<const int> widths = m_hasConfigField
                                          ? std::span<const int> { kWidthsWithConfig }
                                          : std::span<const int> { kWidthsNoConfig };
    if (m_bar->GetFieldsCount() != static_cast<int>(widths.size())) {
        m_bar->SetFieldsCount(static_cast<int>(widths.size()));
    }
    m_bar->SetStatusWidths(static_cast<int>(widths.size()), widths.data());
    refreshConfigurationField();
}

void StatusBarHandler::setCursor(const int line, const int column) const {
    if (m_bar == nullptr) {
        return;
    }
    m_bar->SetStatusText(wxString::Format("%d : %d", line, column), cursorField());
}

void StatusBarHandler::setDocumentFields(const Document& doc) const {
    if (m_bar == nullptr) {
        return;
    }
    const auto typeKey = documentTypeKey(doc.getType());
    auto typeLabel = m_ctx.tr(wxString("statusbar.type.") + wxString::FromUTF8(typeKey.data(), typeKey.size()));
    if (typeLabel.empty()) {
        typeLabel = wxString::FromUTF8(typeKey.data(), typeKey.size());
    }
    m_bar->SetStatusText(typeLabel, typeField());
    m_bar->SetStatusText(wxString::FromUTF8(doc.getEolMode().toString()), eolField());
    m_bar->SetStatusText(wxString::FromUTF8(doc.getEncoding().toString()), encodingField());
    refreshConfigurationField();
}

void StatusBarHandler::clearDocumentFields() const {
    if (m_bar == nullptr) {
        return;
    }
    // Wipe every per-document cell — cursor, type, EOL, encoding, and the
    // optional configuration cell. Driven by the field-index helpers so
    // both the 5- and 6-field layouts clear the right cells (the 6-field
    // layout shifts encoding to index 5, which a hardcoded sweep misses).
    m_bar->SetStatusText("", cursorField());
    m_bar->SetStatusText("", typeField());
    m_bar->SetStatusText("", eolField());
    m_bar->SetStatusText("", encodingField());
    if (const auto cfgField = configurationField(); cfgField >= 0) {
        m_bar->SetStatusText("", cfgField);
    }
}

void StatusBarHandler::refreshConfigurationField() const {
    if (m_bar == nullptr) {
        return;
    }
    const auto cfgField = configurationField();
    if (cfgField < 0) {
        return;
    }
    m_bar->SetStatusText(m_ctx.getCompilerManager().configurationStatusLabel(), cfgField);
}

void StatusBarHandler::onClick(wxMouseEvent& event) {
    event.Skip();
    if (m_bar == nullptr) {
        return;
    }
    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }
    handleClickAt(event.GetPosition(), *doc);
}

auto StatusBarHandler::handleClickAt(const wxPoint& position, Document& doc) -> bool {
    wxRect rect;

    if (m_bar->GetFieldRect(typeField(), rect) && rect.Contains(position)) {
        auto menu = DocumentTypeMenu::build(m_ctx, doc.getType());
        menu->Bind(wxEVT_MENU, [this, &doc](const wxCommandEvent& evt) {
            if (const auto type = DocumentTypeMenu::typeFromId(evt.GetId())) {
                doc.setType(*type);
                doc.getEditor()->updateStatusBar();
                m_ctx.getDocumentManager().updateActiveTabTitle();
            }
        });
        m_bar->PopupMenu(menu.get());
        return true;
    }

    // Configuration field — only when the 6-field layout is active and
    // the document is a FreeBASIC source (the selector hides for any
    // other document type, mirroring the toolbar combobox).
    if (const auto cfgField = configurationField();
        cfgField >= 0
        && doc.getType() == DocumentType::FreeBASIC
        && m_bar->GetFieldRect(cfgField, rect)
        && rect.Contains(position)) {
        auto menu = m_ctx.getCompilerManager().buildConfigurationMenu();
        menu->Bind(wxEVT_MENU, [this](const wxCommandEvent& evt) {
            m_ctx.getCompilerManager().applyConfigurationMenuSelection(evt.GetId());
        });
        m_bar->PopupMenu(menu.get());
        return true;
    }

    if (m_bar->GetFieldRect(eolField(), rect) && rect.Contains(position)) {
        const auto menu = EncodingMenu::buildEolMenu(doc.getEolMode());
        menu->Bind(wxEVT_MENU, [&doc](const wxCommandEvent& evt) {
            if (const auto mode = EncodingMenu::eolFromId(evt.GetId())) {
                doc.setEolMode(*mode);
                doc.getEditor()->updateStatusBar();
            }
        });
        m_bar->PopupMenu(menu.get());
        return true;
    }

    if (m_bar->GetFieldRect(encodingField(), rect) && rect.Contains(position)) {
        auto menu = EncodingMenu::buildEncodingMenu(
            doc.getEncoding(),
            m_ctx.tr("statusbar.encoding.reloadWithEncoding")
        );
        menu->Bind(wxEVT_MENU, [this, &doc](const wxCommandEvent& evt) {
            if (const auto enc = EncodingMenu::encodingSaveFromId(evt.GetId())) {
                doc.setEncoding(*enc);
                m_ctx.getDocumentManager().updateActiveTabTitle();
                doc.getEditor()->updateStatusBar();
                return;
            }
            if (const auto enc = EncodingMenu::encodingReloadFromId(evt.GetId())) {
                m_ctx.getDocumentManager().reloadWithEncoding(doc, *enc);
            }
        });
        m_bar->PopupMenu(menu.get());
        return true;
    }

    return false;
}
