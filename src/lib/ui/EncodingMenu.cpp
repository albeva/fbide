//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "EncodingMenu.hpp"
using namespace fbide;

namespace {

template<typename Array>
auto decodeFromId(int id, int base, const Array& all) -> std::optional<typename Array::value_type> {
    const auto offset = id - base;
    if (offset < 0 || offset >= static_cast<int>(all.size())) {
        return std::nullopt;
    }
    return all[static_cast<std::size_t>(offset)];
}

} // namespace

auto EncodingMenu::buildEolMenu(const EolMode current) -> std::unique_ptr<wxMenu> {
    auto menu = std::make_unique<wxMenu>();
    for (std::size_t i = 0; i < EolMode::all.size(); i++) {
        const EolMode mode { EolMode::all[i] };
        const int id = kEolIdBase + static_cast<int>(i);
        auto* item = menu->AppendRadioItem(id, wxString::FromUTF8(mode.toString()));
        if (mode == current) {
            item->Check(true);
        }
    }
    return menu;
}

auto EncodingMenu::buildEncodingMenu(const TextEncoding current, const wxString& reloadLabel) -> std::unique_ptr<wxMenu> {
    auto menu = std::make_unique<wxMenu>();

    for (std::size_t i = 0; i < TextEncoding::all.size(); i++) {
        const TextEncoding enc { TextEncoding::all[i] };
        const int id = kEncodingSaveIdBase + static_cast<int>(i);
        auto* item = menu->AppendRadioItem(id, wxString::FromUTF8(enc.toString()));
        if (enc == current) {
            item->Check(true);
        }
    }

    menu->AppendSeparator();

    auto* reload = new wxMenu();
    for (std::size_t i = 0; i < TextEncoding::all.size(); i++) {
        const TextEncoding enc { TextEncoding::all[i] };
        const int id = kEncodingReloadIdBase + static_cast<int>(i);
        reload->Append(id, wxString::FromUTF8(enc.toString()));
    }
    menu->AppendSubMenu(reload, reloadLabel);

    return menu;
}

auto EncodingMenu::eolFromId(const int id) -> std::optional<EolMode> {
    if (const auto v = decodeFromId(id, kEolIdBase, EolMode::all); v.has_value()) {
        return EolMode { *v };
    }
    return std::nullopt;
}

auto EncodingMenu::encodingSaveFromId(const int id) -> std::optional<TextEncoding> {
    if (const auto v = decodeFromId(id, kEncodingSaveIdBase, TextEncoding::all); v.has_value()) {
        return TextEncoding { *v };
    }
    return std::nullopt;
}

auto EncodingMenu::encodingReloadFromId(const int id) -> std::optional<TextEncoding> {
    if (const auto v = decodeFromId(id, kEncodingReloadIdBase, TextEncoding::all); v.has_value()) {
        return TextEncoding { *v };
    }
    return std::nullopt;
}
