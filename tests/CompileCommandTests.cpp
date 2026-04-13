#include "lib/compiler/CompileCommand.hpp"
#include <gtest/gtest.h>
using namespace fbide;

// Default template: "<$fbc>" "<$file>"

TEST(CompileCommandTests, SimpleCommand) {
    const auto cmd = CompileCommand::makeDefault("main.bas");
    EXPECT_EQ(cmd.build(R"("<$fbc>" "<$file>")", "/usr/bin/fbc"), R"("/usr/bin/fbc" "main.bas")");
}

TEST(CompileCommandTests, EscapeInnerQuotes) {
    const auto cmd = CompileCommand::makeDefault(R"(say "hello".bas)");
    EXPECT_EQ(cmd.build(R"("<$fbc>" "<$file>")", "/usr/bin/fbc"), R"("/usr/bin/fbc" "say \"hello\".bas")");
}

TEST(CompileCommandTests, PreserveAlreadyEscapedQuotes) {
    const auto cmd = CompileCommand::makeDefault(R"(say \"hello\".bas)");
    EXPECT_EQ(cmd.build(R"("<$fbc>" "<$file>")", "/usr/bin/fbc"), R"("/usr/bin/fbc" "say \"hello\".bas")");
}

TEST(CompileCommandTests, WindowsPaths) {
    const auto cmd = CompileCommand::makeDefault(R"(D:\My Projects\main.bas)");
    EXPECT_EQ(cmd.build(R"("<$fbc>" "<$file>")", R"(C:\FreeBASIC\fbc.exe)"),
        R"("C:\FreeBASIC\fbc.exe" "D:\My Projects\main.bas")");
}

TEST(CompileCommandTests, CustomTemplate) {
    const auto cmd = CompileCommand::makeDefault("main.bas");
    EXPECT_EQ(cmd.build("<$fbc> -v <$file>", "/usr/bin/fbc"), "/usr/bin/fbc -v main.bas");
}

TEST(CompileCommandTests, TemplateWithExtraFlags) {
    const auto cmd = CompileCommand::makeDefault("main.bas");
    EXPECT_EQ(cmd.build(R"("<$fbc>" -lang fb "<$file>")", "/usr/bin/fbc"),
        R"("/usr/bin/fbc" -lang fb "main.bas")");
}
