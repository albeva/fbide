//
//  Config.hpp
//  fbide
//
//  Created by Albert on 14/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "app_pch.hpp"

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
     * Config node type for use with node.GetType() method
     */
    enum class Type: int {
        Null,
        String,
        Bool,
        Int,
        Double,
        Map,
        Array
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
     * Types must be in the same order as Type enum
     */
    using Value = std::variant<std::monostate, wxString, bool, int, double, Map, Array>;

    /**
     * re-usable Shortcut to std::enable_if for supported types
     */
    template<typename T>
    using EnableIf = typename std::enable_if_t<is_one_of<T, wxString, bool, int, double, Map, Array>(), int>;

public:

    //----------------------------------------------------------------------
    // ctors, copy, assign ...
    //----------------------------------------------------------------------

    Config() noexcept = default;
    Config(const Config& other) = default;
    Config(Config&& other) noexcept = default;

    Config& operator=(const Config& rhs) = default;
    Config& operator=(Config&& rhs) noexcept = default;

    /**
     * Create Config from value
     *
     * @tparam T where T is wxString, bool, int, double, Map or Array
     * @param val
     */
    template<typename T, typename = EnableIf<T>>
    explicit Config(const T& val): m_val(val) {}

    template<typename T, typename = EnableIf<T>>
    explicit Config(T&& val) noexcept: m_val(std::move(val)) {}

    /**
     * Assign value to config
     *
     * @tparam T where T is wxString, const*, std::string, bool, int, double, Map or an Array
     * @param val
     */
    template<
        typename T,
        typename U = std::conditional_t<is_one_of<T, char*, std::string>(), wxString, T>,
        typename B = std::decay_t<U>,
        EnableIf<B> = 0>
    inline Config& operator=(const T& rhs) {
        m_val.emplace<B>(rhs);
        return *this;
    }

    template<
        typename T,
        typename U = std::conditional_t<is_one_of<T, char*, std::string>(), wxString, T>,
        typename B = std::decay_t<U>,
        EnableIf<B> = 0>
    inline Config& operator=(T&& rhs) noexcept {
        m_val.emplace<B>(std::move(rhs));
        return *this;
    }

    /**
     * Load yaml file into this config object
     */
    void LoadYaml(const wxString& path);

    /**
     * Check if Configs are equal
     */
    [[nodiscard]] bool operator==(const Config& rhs) const noexcept {
        return m_val == rhs.m_val;
    }

    [[nodiscard]] inline bool operator != (const Config& rhs) const noexcept {
        return m_val != rhs.m_val;
    }

    //----------------------------------------------------------------------
    // String
    //----------------------------------------------------------------------

    explicit Config(const std::string& val): m_val(std::in_place_type<wxString>, val) {}
    explicit Config(std::string&& val): m_val(std::in_place_type<wxString>, std::move(val)) {}
    explicit Config(const char* val): m_val(std::in_place_type<wxString>, val) {}

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

    [[nodiscard]] inline const bool AsBool() const {
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
     * If any key points to a wrong type this will throw an exception
     *
     * If any part of the path is a number (unsigned long) then it is
     * considered to be an index into an array. Array will be created only
     * if index is next in sequence. Otherwise there will be an exception
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
     * print Config tree to console out
     */
    void Dump(size_t indent = 0) const;

    /**
     * Get node type as enum value
     */
    [[nodiscard]] inline Type GetType() const noexcept {
        return static_cast<Type>(m_val.index());
    }

    /**
     * Is this a scalar Config?
     */
    [[nodiscard]] inline bool IsScalar() const noexcept {
        return !IsEmpty()
               && GetType() != Type::Map
               && GetType() != Type::Array;
    }

    /**
     * Check if this Config is null
     */
    [[nodiscard]] inline bool IsEmpty() const noexcept {
        return GetType() == Type::Null;
    }

    /**
     * Clear the Config to null
     */
    inline void Clear() noexcept {
        m_val = std::monostate();
    }

    /**
     * Check if currently held Config is of given type
     *
     * if (value.Is<wxString>()) { ... }
     */
    template<typename T, EnableIf<T> = 0>
    [[nodiscard]] inline bool Is() const noexcept {
        return std::holds_alternative<T>(m_val);
    }

    /**
     * Compare rhs of type T against held value.
     * Will return false if types don't match.
     *
     * When comparing against const char * we need to coerce the type
     * to wxString - hence the conditional_t stuff.
     */
    template<
        typename T,
        typename U = std::conditional_t<is_one_of<T, char*, std::string>(), wxString, T>,
        typename B = std::decay_t<U>,
        EnableIf<B> = 0>
    [[nodiscard]] inline bool operator==(const T& rhs) const noexcept {
        if (!Is<B>())
            return false;
        return As<B>() == rhs;
    }

    /**
     * Inequality check
     */
    template<typename T, EnableIf<T> = 0>
    [[nodiscard]] inline bool operator!=(const T& rhs) const noexcept {
        return !(*this == rhs);
    }

    /**
     * Get requested string value and return default if none exists.
     * will *not* modify Config structure as ["path"] does
     */
    template<typename T, EnableIf<T> = 0>
    [[nodiscard]] inline const T& Get(const wxString& path, const T& def) const noexcept {
        auto node = Get(path);
        if (node == nullptr || !node->Is<T>()) {
            return def;
        }
        return node->As<T>();
    }

private:
    /**
     * Return Config as given type. Null is propagated
     * to the type while other type mismatches will throw
     * an exception.
     *
     * @throws std::bad_variant_access
     */
    template<typename T, EnableIf<T> = 0>
    [[nodiscard]] inline T& As() {
        if (IsEmpty()) {
            m_val = T();
        }
        return std::get<T>(m_val);
    }

    template<typename T, EnableIf<T> = 0>
    [[nodiscard]] inline const T& As() const {
        return std::get<T>(m_val);
    }

    // Config holder
    Value m_val;
};
} // namespace fbide