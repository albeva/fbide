#include <gtest/gtest.h>

class BasicTests : public testing::Test {};

TEST_F(BasicTests, Sanity) {
    EXPECT_EQ(1, 1);
}
