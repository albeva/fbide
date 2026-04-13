#include "lib/compiler/RunCommand.hpp"
#include <gtest/gtest.h>
using namespace fbide;

TEST(RunCommandTests, DefaultTemplate) {
    const auto cmd = RunCommand::makeDefault("/usr/bin/hello");
    EXPECT_EQ(cmd.build(R"(<$terminal> "<$file>" <$param>)", "open -a Terminal", "--verbose"),
        R"(open -a Terminal "/usr/bin/hello" --verbose)");
}

TEST(RunCommandTests, NoTerminal) {
    const auto cmd = RunCommand::makeDefault("/usr/bin/hello");
    EXPECT_EQ(cmd.build(R"("<$file>" <$param>)", "", "--verbose"), R"("/usr/bin/hello" --verbose)");
}

TEST(RunCommandTests, FilePathParts) {
    const auto cmd = RunCommand::makeDefault("/home/user/project/main.exe");
    EXPECT_EQ(cmd.build("<$file_path>/<$file_name>.<$file_ext>"), "/home/user/project/main.exe");
}

TEST(RunCommandTests, EmptyParameters) {
    const auto cmd = RunCommand::makeDefault("/usr/bin/hello");
    EXPECT_EQ(cmd.build(R"(<$terminal> "<$file>" <$param>)", "xterm"),
        R"(xterm "/usr/bin/hello")");
}

TEST(RunCommandTests, WindowsPath) {
    const auto cmd = RunCommand::makeDefault(R"(C:\Projects\hello.exe)");
    EXPECT_EQ(cmd.build(R"("<$file>" <$param>)", "", "-arg1"), R"("C:\Projects\hello.exe" -arg1)");
}

TEST(RunCommandTests, EscapeInnerQuotes) {
    const auto cmd = RunCommand::makeDefault(R"(/path/to/say "hello".exe)");
    EXPECT_EQ(cmd.build(R"("<$file>")"), R"("/path/to/say \"hello\".exe")");
}

TEST(RunCommandTests, PreserveAlreadyEscapedQuotes) {
    const auto cmd = RunCommand::makeDefault(R"(/path/to/say \"hello\".exe)");
    EXPECT_EQ(cmd.build(R"("<$file>")"), R"("/path/to/say \"hello\".exe")");
}
