//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include <chrono>
#include <iostream>
#include "EditorTestShim.hpp"

using namespace fbide;
using namespace fbide::tests;

// Performance probe: load a real-world FreeBASIC source file, select the
// whole buffer, then indent it with a single Tab press. Reports the wall
// time of the indent operation. Bare editor — no minimap attached.
TEST(EditorPerfTests, SelectAllIndentFlappy) {
    EditorTestShim shim;

    // Load the sample file into the editor.
    const wxString path = FBIDE_TEST_DATA_DIR "flappyfb.bas";
    wxFile file(path);
    ASSERT_TRUE(file.IsOpened()) << "cannot open " << path;
    wxString content;
    ASSERT_TRUE(file.ReadAll(&content));
    shim.editor().SetText(content);
    ASSERT_GT(shim.editor().GetLineCount(), 100);

    // Select the whole buffer.
    shim.editor().SelectAll();

    // Measure: press Tab to indent the selection.
    double elapsedMs = 0.0;
    shim.run([&] {
        wxUIActionSimulator sim;
        const auto start = std::chrono::steady_clock::now();
        sim.Char(WXK_TAB);
        wxYield();
        const auto end = std::chrono::steady_clock::now();
        elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    });

    std::cout << "[ PERF     ] select-all + indent ("
              << shim.editor().GetLineCount() << " lines): "
              << elapsedMs << " ms" << std::endl;
    RecordProperty("indent_ms", static_cast<int>(elapsedMs));
}
