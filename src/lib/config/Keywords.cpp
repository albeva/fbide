//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Keywords.hpp"
using namespace fbide;

void Keywords::load(const wxString& filePath) {
    m_langePath = filePath;
    m_groups = {};
    m_sortedList.Clear();

    wxFFileInputStream stream(filePath);
    if (!stream.IsOk()) {
        return;
    }

    wxFileConfig ini(stream);
    ini.SetPath("/keywords");

    for (std::size_t idx = 0; idx < GROUP_COUNT; idx++) {
        wxString key;
        key.Printf("kw%zu", idx + 1);
        m_groups.at(idx) = ini.Read(key, "");
    }

    buildSortedList();
}

void Keywords::save() const {
    wxFileConfig ini;
    ini.SetPath("/keywords");

    for (std::size_t idx = 0; idx < GROUP_COUNT; idx++) {
        wxString key;
        key.Printf("kw%zu", idx + 1);
        ini.Write(key, m_groups.at(idx));
    }

    wxFileOutputStream outStream(m_langePath);
    ini.Save(outStream);
}

void Keywords::buildSortedList() {
    m_sortedList.Clear();

    for (const auto& group : m_groups) {
        wxStringTokenizer tokenizer(group, " \t\n\r");
        while (tokenizer.HasMoreTokens()) {
            if (auto word = tokenizer.GetNextToken().Trim().Trim(false); !word.empty()) {
                m_sortedList.Add(word.MakeLower());
            }
        }
    }

    m_sortedList.Sort();
}
