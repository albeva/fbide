/**
 * FBIde project
 */
#pragma once

namespace fbide {

// c++14 string literal "hello"s
using namespace std::literals::string_literals;

/**
 * Hash map of string to T
 */
template<class T>
using StringMap = std::unordered_map<wxString, T>;


/**
 * Concatinate path component together separated by platform specific path
 * component separator
 */
inline wxString operator/(const wxString& lhs, const wxString& rhs) {
    return wxString(lhs).append(wxFILE_SEP_PATH).append(rhs);
}


/**
 * Append path component separated by platform specific path component separator
 */
inline wxString& operator/=(wxString& lhs, const wxString& rhs) {
    return lhs.append(wxFILE_SEP_PATH).append(rhs);
}


/**
 * wxString shorthand. "Hello"_wx
 */
inline wxString operator"" _wx(const char* s, size_t len) {
    return { s, len };
}


/**
 * is_one_of checks if type T is one of the given types
 */
template<typename T>
constexpr bool is_one_of() {
    return false;
}

template<typename T, typename U, typename... R>
constexpr bool is_one_of() {
    return std::is_same<std::decay_t<T>, U>::value || is_one_of<T, R...>();
}

/**
 * Check if Extended is sub class of Base
 */
template<typename Base, typename Extended>
constexpr bool is_extended_from() {
    return std::is_base_of_v<Base, Extended> && !std::is_same_v<Base, Extended>;
}

/**
 * Disallow copying this class
 */
#define NON_COPYABLE(Class)                  \
    Class(const Class&) = delete;            \
    Class& operator=(const Class&) = delete; \
    Class(Class&&) = delete;                 \
    Class& operator=(Class&&) = delete;

} // namespace fbide

#define LOG_VAR(VAR) wxLogMessage(wxString(#VAR) << " = " << VAR)
