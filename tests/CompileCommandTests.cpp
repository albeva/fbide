#include "lib/compiler/CompileCommand.hpp"
#include <gtest/gtest.h>
using namespace fbide;

class CompileCommandTests : public testing::Test {};

// Default template: "<$fbc>" "<$file>"

TEST_F(CompileCommandTests, SimpleCommand) {
    const auto cmd = CompileCommand::makeDefault("main.bas");
    EXPECT_EQ(cmd.build(R"("<$fbc>" "<$file>")", "/usr/bin/fbc"), R"("/usr/bin/fbc" "main.bas")");
}

TEST_F(CompileCommandTests, EscapeInnerQuotes) {
    const auto cmd = CompileCommand::makeDefault(R"(say "hello".bas)");
    EXPECT_EQ(cmd.build(R"("<$fbc>" "<$file>")", "/usr/bin/fbc"), R"("/usr/bin/fbc" "say \"hello\".bas")");
}

TEST_F(CompileCommandTests, PreserveAlreadyEscapedQuotes) {
    const auto cmd = CompileCommand::makeDefault(R"(say \"hello\".bas)");
    EXPECT_EQ(cmd.build(R"("<$fbc>" "<$file>")", "/usr/bin/fbc"), R"("/usr/bin/fbc" "say \"hello\".bas")");
}

TEST_F(CompileCommandTests, WindowsPaths) {
    const auto cmd = CompileCommand::makeDefault(R"(D:\My Projects\main.bas)");
    EXPECT_EQ(cmd.build(R"("<$fbc>" "<$file>")", R"(C:\FreeBASIC\fbc.exe)"),
        R"("C:\FreeBASIC\fbc.exe" "D:\My Projects\main.bas")");
}

TEST_F(CompileCommandTests, CustomTemplate) {
    const auto cmd = CompileCommand::makeDefault("main.bas");
    EXPECT_EQ(cmd.build("<$fbc> -v <$file>", "/usr/bin/fbc"), "/usr/bin/fbc -v main.bas");
}

TEST_F(CompileCommandTests, TemplateWithExtraFlags) {
    const auto cmd = CompileCommand::makeDefault("main.bas");
    EXPECT_EQ(cmd.build(R"("<$fbc>" -lang fb "<$file>")", "/usr/bin/fbc"),
        R"("/usr/bin/fbc" -lang fb "main.bas")");
}
