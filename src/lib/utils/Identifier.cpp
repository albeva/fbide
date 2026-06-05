//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Identifier.hpp"
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace fbide;

auto fbide::generateUuid() -> boost::uuids::uuid {
    // `random_generator` holds a thread-local Mersenne Twister seeded
    // from the system entropy source on first use, so concurrent calls
    // from different threads don't contend.
    static thread_local boost::uuids::random_generator generator;
    return generator();
}

auto fbide::uuidToString(const boost::uuids::uuid& uuid) -> std::string {
    return boost::uuids::to_string(uuid);
}

auto fbide::uuidFromString(const std::string_view text) -> boost::uuids::uuid {
    // `string_generator` throws std::runtime_error on malformed input.
    return boost::uuids::string_generator {}(text.begin(), text.end());
}
