//
// Created by Albert on 8/1/2020.
//
#pragma once
#include "pch.h"

#define DEFAULT_EDITOR_STYLE(_) \
    _( Foreground, wxString("black") ) \
    _( Background, wxString("white") ) \
    _( Bold,       false             ) \
    _( Italic,     false             ) \
    _( Underline,  false             ) \
    _( Visible,    true              ) \
    _( Case,       wxSTC_CASE_MIXED  ) \
    _( EOLFilled,  false             )

#define DEFAULT_EDITOR_CONFIG(_) \
    _( TabWidth,               4                  ) \
    _( UseTabs,                false              ) \
    _( TabIndents,             true               ) \
    _( BackSpaceUnIndents,     true               ) \
    _( Indent,                 4                  ) \
    _( EdgeColumn,             80                 ) \
    _( EdgeMode,               wxSTC_EDGE_LINE    ) \
    _( EOLMode,                0                  ) \
    _( ViewEOL,                false              ) \
    _( IndentationGuides,      false              ) \
    _( ViewWhiteSpace,         wxSTC_WS_INVISIBLE ) \
    _( CodePage,               wxSTC_CP_UTF8      ) \
    _( IMEInteraction,         0                  ) \
    _( CaretLineVisible,       true               ) \
    _( UseHorizontalScrollBar, false              ) \
    _( WrapMode,               false              )

namespace fbide::Defaults {
    constexpr auto FontSize = 12;
    constexpr auto ShowLineNumbers = true;

    #define DEFAULT_CONFIG(name, value) constexpr auto name = value;
    DEFAULT_EDITOR_CONFIG(DEFAULT_CONFIG)
    #undef DEFAULT_CONFIG

    namespace Key {
        constexpr auto FontSize = "FontSize";
        constexpr auto FontName = "FontName";
        constexpr auto ShowLineNumbers = "ShowLineNumbers";

        #define DEFAULT_CONFIG_KEY(name, value) constexpr auto name = #name;
        DEFAULT_EDITOR_CONFIG(DEFAULT_CONFIG_KEY)
        #undef DEFAULT_CONFIG_KEY
    }
}
