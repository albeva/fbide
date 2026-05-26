//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Uuid.hpp"
#include <stdexcept>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace fbide;

namespace {

/// Copy a `boost::uuids::uuid`'s 16 bytes into our flat `Uuid::Bytes`
/// array. Both layouts are 16 octets in the same order — RFC 4122
/// big-endian-ish — so a straight `std::ranges::copy` is the canonical
/// conversion.
auto toBytes(const boost::uuids::uuid& uuid) -> Uuid::Bytes {
    Uuid::Bytes bytes;
    std::ranges::copy(uuid, bytes.begin());
    return bytes;
}

/// Inverse of `toBytes` — assemble a Boost UUID from our flat array.
auto toBoost(const Uuid::Bytes& bytes) -> boost::uuids::uuid {
    boost::uuids::uuid uuid;
    std::ranges::copy(bytes, uuid.begin());
    return uuid;
}

} // namespace

auto Uuid::generate() -> Uuid {
    // Boost's `random_generator` keeps a thread-local Mersenne Twister
    // seeded from the system entropy source on first use, so multiple
    // calls from different threads don't contend.
    static thread_local boost::uuids::random_generator generator;
    return Uuid { toBytes(generator()) };
}

auto Uuid::parse(const std::string_view text) -> std::optional<Uuid> {
    // `string_generator` throws on malformed input; the wrapper turns
    // that into a `nullopt` so callers can use the optional idiom.
    try {
        const boost::uuids::string_generator generator;
        return Uuid { toBytes(generator(text.begin(), text.end())) };
    } catch (const std::runtime_error&) {
        return std::nullopt;
    }
}

auto Uuid::toString() const -> std::string {
    return boost::uuids::to_string(toBoost(m_bytes));
}
