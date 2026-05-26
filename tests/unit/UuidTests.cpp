//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <unordered_set>
#include <gtest/gtest.h>
#include "utils/Uuid.hpp"

using namespace fbide;

TEST(UuidTest, DefaultIsNil) {
    constexpr Uuid uuid;
    EXPECT_FALSE(static_cast<bool>(uuid));
    for (const auto byte : uuid.bytes()) {
        EXPECT_EQ(byte, 0U);
    }
}

TEST(UuidTest, GeneratedIsNonNil) {
    const auto uuid = Uuid::generate();
    EXPECT_TRUE(static_cast<bool>(uuid));
}

TEST(UuidTest, GeneratedHasV4VersionAndRfc4122Variant) {
    const auto uuid = Uuid::generate();
    const auto& bytes = uuid.bytes();
    // RFC 4122 §4.4 — version 4 in high nibble of byte 6.
    EXPECT_EQ(bytes[6] & 0xF0U, 0x40U);
    // RFC 4122 §4.1.1 — variant 10xx in top two bits of byte 8.
    EXPECT_EQ(bytes[8] & 0xC0U, 0x80U);
}

TEST(UuidTest, GeneratedAreUnique) {
    // Statistical test — collisions for 1k v4 UUIDs are astronomically
    // unlikely. If this ever fails the PRNG seeding is broken.
    constexpr int kCount = 1000;
    std::unordered_set<Uuid> seen;
    seen.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        EXPECT_TRUE(seen.insert(Uuid::generate()).second);
    }
}

TEST(UuidTest, ToStringIsCanonical) {
    const auto uuid = Uuid::generate();
    const auto text = uuid.toString();
    ASSERT_EQ(text.size(), 36U);
    EXPECT_EQ(text[8], '-');
    EXPECT_EQ(text[13], '-');
    EXPECT_EQ(text[18], '-');
    EXPECT_EQ(text[23], '-');
    // Hex digits everywhere else.
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue;
        }
        const auto ch = text[i];
        EXPECT_TRUE((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')) << "at position " << i;
    }
}

TEST(UuidTest, ParseRoundTrip) {
    const auto original = Uuid::generate();
    const auto parsed = Uuid::parse(original.toString());
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, original);
}

TEST(UuidTest, ParseAcceptsUpperCase) {
    const auto lower = Uuid::parse("550e8400-e29b-41d4-a716-446655440000");
    const auto upper = Uuid::parse("550E8400-E29B-41D4-A716-446655440000");
    ASSERT_TRUE(lower.has_value());
    ASSERT_TRUE(upper.has_value());
    EXPECT_EQ(*lower, *upper);
}

TEST(UuidTest, ParseRejectsWrongLength) {
    EXPECT_FALSE(Uuid::parse("").has_value());
    EXPECT_FALSE(Uuid::parse("550e8400-e29b-41d4-a716-44665544000").has_value());     // 35
    EXPECT_FALSE(Uuid::parse("550e8400-e29b-41d4-a716-4466554400000").has_value());   // 37
}

TEST(UuidTest, ParseRejectsMissingDashes) {
    EXPECT_FALSE(Uuid::parse("550e8400_e29b-41d4-a716-446655440000").has_value());
    EXPECT_FALSE(Uuid::parse("550e8400e29b-41d4-a716-446655440000-").has_value());
}

TEST(UuidTest, ParseRejectsNonHexDigits) {
    EXPECT_FALSE(Uuid::parse("550e8400-e29b-41d4-a716-44665544zzzz").has_value());
}

TEST(UuidTest, NilToStringIsZeros) {
    constexpr Uuid uuid;
    EXPECT_EQ(uuid.toString(), "00000000-0000-0000-0000-000000000000");
}

TEST(UuidTest, EqualityFromExplicitBytes) {
    constexpr Uuid::Bytes raw {
        0x55, 0x0e, 0x84, 0x00,
        0xe2, 0x9b, 0x41, 0xd4,
        0xa7, 0x16, 0x44, 0x66,
        0x55, 0x44, 0x00, 0x00,
    };
    constexpr Uuid lhs { raw };
    constexpr Uuid rhs { raw };
    EXPECT_EQ(lhs, rhs);
    EXPECT_EQ(lhs.toString(), "550e8400-e29b-41d4-a716-446655440000");
}

TEST(UuidTest, Hashable) {
    std::unordered_map<Uuid, int> map;
    const auto first = Uuid::generate();
    const auto second = Uuid::generate();
    map[first] = 1;
    map[second] = 2;
    EXPECT_EQ(map[first], 1);
    EXPECT_EQ(map[second], 2);
}
