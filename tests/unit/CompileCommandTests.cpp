#include <gtest/gtest.h>
#include "compiler/CompileCommand.hpp"
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

TEST_F(CompileCommandTests, PathsWithSpaces) {
    const auto cmd = CompileCommand::makeDefault("/home/user/My Projects/main.bas");
    EXPECT_EQ(cmd.build(R"("<$fbc>" "<$file>")", "/usr/local/bin/fbc"),
        R"("/usr/local/bin/fbc" "/home/user/My Projects/main.bas")");
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

// --- extractIncludePaths ---------------------------------------------------

TEST_F(CompileCommandTests, IncludePaths_DefaultTemplateHasNone) {
    EXPECT_TRUE(CompileCommand::extractIncludePaths(R"("<$fbc>" "<$file>")").empty());
}

TEST_F(CompileCommandTests, IncludePaths_QuotedPathWithSpaces) {
    const auto paths = CompileCommand::extractIncludePaths(R"("<$fbc>" "<$file>" -i "/opt/fb inc")");
    ASSERT_EQ(paths.size(), 1U);
    EXPECT_EQ(paths[0], "/opt/fb inc");
}

TEST_F(CompileCommandTests, IncludePaths_MultipleInOrder) {
    const auto paths = CompileCommand::extractIncludePaths(R"("<$fbc>" "<$file>" -i ./vendor/inc -i "/abs/two")");
    ASSERT_EQ(paths.size(), 2U);
    EXPECT_EQ(paths[0], "./vendor/inc");
    EXPECT_EQ(paths[1], "/abs/two");
}

TEST_F(CompileCommandTests, IncludePaths_IgnoresOtherFlagsAndMetaTags) {
    const auto paths = CompileCommand::extractIncludePaths(R"("<$fbc>" -lang fb "<$file>" -i inc -g)");
    ASSERT_EQ(paths.size(), 1U);
    EXPECT_EQ(paths[0], "inc");
}

TEST_F(CompileCommandTests, IncludePaths_DanglingFlagIgnored) {
    EXPECT_TRUE(CompileCommand::extractIncludePaths(R"("<$fbc>" "<$file>" -i)").empty());
}

TEST_F(CompileCommandTests, IncludePaths_GluedFormNotParsed) {
    // fbc takes the path as a separate argument; the glued -i<path> form is
    // not valid fbc, so it must not be mistaken for an include directory.
    EXPECT_TRUE(CompileCommand::extractIncludePaths(R"("<$fbc>" "<$file>" -i"/glued")").empty());
}
