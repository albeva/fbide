/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "pch.h"

namespace fbide {

/**
 * Config is variant type that uses std::variant for storage
 * It supports nested structure With Array or Map
 *
 * This also supports quering config using a path.
 * e.g. c["foo.bar[3].key"]
 *
 * Supported types are:
 * - wxString (including const char *)
 * - bool
 * - int
 * - double
 * - Map
 * - Array
 * - null
 */
class Config final {
public:
    /**
     * Referencable empty value
     */
    static const Config Empty;

    /**
     * Config node type for use with node.GetType() method
     */
    enum class Type: int {
        String,
        Bool,
        Int,
        Double,
        Map,
        Array,
        Null
    };

    /**
     * Map is key Config pairs of Config objects
     */
    using Map = StringMap<Config>;

    /**
     * Array is a list of Config objects
     */
    using Array = std::vector<Config>;

private:

    /**
     * Types must be in the same order as in Type enum
     */
    using Value = std::variant<wxString, bool, int, double, Map, Array>;

    // use memory pool to allocate config Value
    struct details;
    static void* allocate();
    static void deallocate(void*);

    // optimize the unique_ptr size
    using deleter = std::integral_constant<decltype(Config::deallocate)*, Config::deallocate>;
    using Container = std::unique_ptr<Value, deleter>;

    // Config value container
    Container m_val;

    #define NEW_VALUE(...) new(Config::allocate()) Value(__VA_ARGS__) /* NOLINT */

    /**
     * If wxString is constructible from T then wxString, otherwise, decay T to its base type.
     */
    template<typename T>
    using ReduceType = std::conditional_t<std::is_constructible_v<wxString, T>, wxString, std::decay_t<T>>;

    /**
     * Shortcut to std::enable_if for supported types
     */
    template<typename T>
    using CheckType = std::enable_if_t<std::is_constructible_v<Value, T>, int>;

public:

    //----------------------------------------------------------------------
    // ctors, copy, assign ...
    //----------------------------------------------------------------------

    Config() noexcept = default;
    ~Config() noexcept = default;

    Config(const Config& other) : m_val{other.m_val == nullptr ? nullptr: NEW_VALUE(*other.m_val)} {}
    Config(Config&& other) noexcept = default;

    Config& operator=(const Config& rhs) {
        if (this != &rhs) {
            m_val.reset (rhs.m_val == nullptr ? nullptr: NEW_VALUE(* rhs.m_val));
        }
        return *this;
    }
    Config& operator=(Config&& rhs) noexcept = default;

    /**
     * Create Config from value
     *
     * @tparam T where T is supported string, bool, int, double, Map or an Array
     * @param val
     */
    template<typename T, typename B = ReduceType<T>, CheckType<B> = 0>
    explicit Config(const T& val): m_val{NEW_VALUE(std::in_place_type<B>, val)} {}

    template<typename T, typename B = ReduceType<T>, CheckType<B> = 0>
    explicit Config(T&& val): m_val{NEW_VALUE(std::in_place_type<B>, std::forward<T>(val))} {}

    /**
     * Assign value to config
     *
     * @tparam T where T is supported string, bool, int, double, Map or an Array
     * @param val
     */
    template<typename T, typename B = ReduceType<T>, CheckType<B> = 0>
    inline Config& operator=(const T& rhs) {
        m_val.reset(NEW_VALUE(std::in_place_type<B>, rhs));
        return *this;
    }

    template<typename T, typename B = ReduceType<T>, CheckType<B> = 0>
    inline Config& operator=(T&& rhs) {
        m_val.reset(NEW_VALUE(std::in_place_type<B>, std::forward<T>(rhs)));
        return *this;
    }

    //----------------------------------------------------------------------
    // Load / Store
    //----------------------------------------------------------------------

    static Config LoadYaml(const wxString& path);

    //----------------------------------------------------------------------
    // Comparisons
    //----------------------------------------------------------------------

    [[nodiscard]] bool operator == (const Config& rhs) const noexcept {
        if (!m_val) {
            return rhs.m_val == nullptr;
        }
        return *m_val == *rhs.m_val;
    }

    [[nodiscard]] inline bool operator != (const Config& rhs) const noexcept {
        return !(*this == rhs);
    }

    template<typename T, typename B = ReduceType<T>, CheckType<B> = 0>
    [[nodiscard]] inline bool operator==(const T& rhs) const noexcept {
        if (!Is<B>()) {
            return false;
        }
        return As<B>() == rhs;
    }

    template<typename T, CheckType<T> = 0>
    [[nodiscard]] inline bool operator!=(const T& rhs) const noexcept {
        return !(*this == rhs);
    }

    //----------------------------------------------------------------------
    // String
    //----------------------------------------------------------------------

    /**
     * Does Config hold wxString?
     */
    [[nodiscard]] inline bool IsString() const noexcept {
        return Is<wxString>();
    }

    /**
     * Get wxString from Config. Null is converted to string!
     *
     * @throws std::bad_variant_access
     */
    [[nodiscard]] inline wxString& AsString() {
        return As<wxString>();
    }

    [[nodiscard]] inline const wxString& AsString() const {
        return As<wxString>();
    }

    //----------------------------------------------------------------------
    // Bool
    //----------------------------------------------------------------------

    /**
     * Does Config hold bool?
     */
    [[nodiscard]] inline bool IsBool() const noexcept {
        return Is<bool>();
    }

    /**
     * Get bool from Config. Null is converted to bool!
     * @throws std::bad_variant_access
     */
    [[nodiscard]] inline bool& AsBool() {
        return As<bool>();
    }

    [[nodiscard]] inline bool AsBool() const {
        return As<bool>();
    }

    //----------------------------------------------------------------------
    // Int
    //----------------------------------------------------------------------

    /**
     * Does Config hold int?
     */
    [[nodiscard]] inline bool IsInt() const noexcept {
        return Is<int>();
    }

    /**
     * Get int from Config. Null is converted to int!
     * @throws std::bad_variant_access
     */
    [[nodiscard]] inline int& AsInt() {
        return As<int>();
    }

    [[nodiscard]] inline int AsInt() const {
        return As<int>();
    }

    //----------------------------------------------------------------------
    // Double
    //----------------------------------------------------------------------

    /**
     * Does Config hold double?
     */
    [[nodiscard]] inline bool IsDouble() const noexcept {
        return Is<double>();
    }

    /**
     * Get double from Config. Null is converted to double!
     * @throws std::bad_variant_access
     */
    [[nodiscard]] inline double& AsDouble() {
        return As<double>();
    }

    [[nodiscard]] inline double AsDouble() const {
        return As<double>();
    }

    //----------------------------------------------------------------------
    // Map
    //----------------------------------------------------------------------

    /**
     * Does Config hold Map?
     */
    [[nodiscard]] inline bool IsMap() const noexcept {
        return Is<Map>();
    }

    /**
     * Get Map from Config. Null is converted to map!
     * @throws std::bad_variant_access
     */
    [[nodiscard]] inline Map& AsMap() {
        return As<Map>();
    }

    [[nodiscard]] inline const Map& AsMap() const {
        return As<Map>();
    }

    //----------------------------------------------------------------------
    // Array
    //----------------------------------------------------------------------

    /**
     * Does Config hold Array?
     */
    [[nodiscard]] inline bool IsArray() const noexcept {
        return Is<Array>();
    }

    /**
     * Get Array from Config. Null is converted to array!
     * @throws std::bad_variant_access
     */
    [[nodiscard]] inline Array& AsArray() {
        return As<Array>();
    }

    [[nodiscard]] inline const Array& AsArray() const {
        return As<Array>();
    }

    //----------------------------------------------------------------------
    // Utilities
    //----------------------------------------------------------------------

    /**
     * Path is a period('.') separated string where each part
     * is considered as a key to a Map. Thus 'foo.bar' will get node 'bar'
     * in a Map pointed by 'foo'. If any part of the path does not yet
     * exist it is silently created. If leaf (last bit of the path) doesn't
     * exist it is created as an empty node (IsEmpty() will return true)
     *
     * @throws std::bad_variant_access
     */
    [[nodiscard]] Config& operator[](const wxString& path);

    /**
     * Fetch pointer to config node at given path. Does
     * not modify structure. Will return nullptr if no path found
     */
    [[nodiscard]] const Config* Get(const wxString& path) const noexcept;

    /**
     * Get config object by reference or `Config::Empty` if none found
     */
    [[nodiscard]] const Config& GetOrEmpty(const wxString& path) const noexcept {
        const auto* res = Get(path);
        return res == nullptr ? Config::Empty : *res;
    }

    /**
     * Get Config tree
     */
    [[nodiscard]] wxString ToString(size_t indent = 0) const noexcept;

    /**
     * Get node type as enum value
     */
    [[nodiscard]] inline Type GetType() const noexcept {
        if (!m_val) {
            return Type::Null;
        }
        return static_cast<Type>(m_val->index());
    }

    /**
     * Is this a scalar Config?
     */
    [[nodiscard]] inline bool IsScalar() const noexcept {
        auto type = GetType();
        return type != Type::Null
            && type != Type::Map
            && type != Type::Array;
    }

    /**
     * Check if config contains empty value: Null, empty array, string or a map
     * Ints, doubles and bools do not count as empty value
     */
    [[nodiscard]] inline bool IsEmpty() const noexcept {
        auto type = GetType();
        switch (type) {
        case Type::Null:
            return true;
        case Type::Array:
            return AsArray().empty();
        case Type::Map:
            return AsMap().empty();
        case Type::String:
            return AsString().empty();
        default:
            return false;
        }
    }

    /**
     * Return true if config has no value.
     */
    [[nodiscard]] inline bool IsNull() const noexcept {
        return GetType() == Type::Null;
    }

    /**
     * Clear the Config to null
     */
    inline void Clear() noexcept {
        m_val.reset();
    }

    /**
     * Get value at given path if it exists, or return given default value.
     *
     * This returns by value!
     */
    template<typename T, typename B = ReduceType<T>, CheckType<B> = 0>
    [[nodiscard]] inline B Get(const wxString& path, const T& def) const noexcept {
        const auto *node = Get(path);
        if (node == nullptr || !node->Is<B>()) {
            return def;
        }
        return node->As<B>();
    }

    template<typename T, typename B = ReduceType<T>, CheckType<B> = 0>
    [[nodiscard]] inline B Get(const wxString& path, T&& def) const noexcept {
        const auto *node = Get(path);
        if (node == nullptr || !node->Is<B>()) {
            return std::forward<T>(def);
        }
        return node->As<B>();
    }

private:

    /**
     * Check if currently held Config is of given type
     *
     * if (value.Is<wxString>()) { ... }
     */
    template<typename T, CheckType<T> = 0>
    [[nodiscard]] inline bool Is() const noexcept {
        if (!m_val) {
            return false;
        }
        return std::holds_alternative<T>(*m_val);
    }

    /**
     * Return Config as given type. Null is propagated
     * to the type while other type mismatches will throw
     * an exception.
     *
     * @throws std::bad_variant_access
     */
    template<typename T, CheckType<T> = 0>
    [[nodiscard]] inline T& As() {
        if (IsEmpty()) {
            *this = T();
        }
        return std::get<T>(*m_val);
    }

    template<typename T, CheckType<T> = 0>
    [[nodiscard]] inline const T& As() const {
        return std::get<T>(*m_val);
    }

    #undef NEW_VALUE
};
} // namespace fbide
