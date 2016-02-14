# 1 "ConfigManager.cpp"
# 1 "<built-in>" 1
# 1 "<built-in>" 3
# 332 "<built-in>" 3
# 1 "<command line>" 1
# 1 "<built-in>" 2
# 1 "ConfigManager.cpp" 2
//
//  ConfigManager.cpp
//  fbide
//
//  Created by Albert on 06/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
//#ifdef _MSC_VER
//    #include "app_pch.hpp"
//#endif
//
//#include "ConfigManager.hpp"
//#include <boost/any.hpp>


using namespace fbide;



// Load the configuration
ConfigManager::ConfigManager()
{
}


// clean up
ConfigManager::~ConfigManager()
{
}

struct Value;

/**
 * Map is key Value pairs of Value objects
 */
typedef std::unordered_map<wxString, Value, wxStringHash, wxStringEqual> Map;

/**
 * Array is a list of Value objects
 */
typedef std::vector<Value> Array;

/**
 * Value is based of boost::any, but restricts the supported
 * typed to: wxString, bool, int, double, Array and Map
 */
struct Value
{
# 129 "ConfigManager.cpp"
    /**
     * Default Value will be null
     *
     * value.IsNull() will return true
     */
    Value() {}


    /**
     * Copy value. This will hold a new copy of the value
     */
    explicit Value(const Value & other) : m_val(other) {}


    /**
     * Move value
     */
    explicit Value(Value && other) noexcept : m_val(std::move(other)) {}

    /**
     * Check if values are equal. Map and Array can't be checked
     * and simply return false!
     */
    bool operator == (const Value & rhs) const noexcept
    {
        // addresses?
        if (this == &rhs) return true;

        // check if one or both are null
        if (IsNull()) return rhs.IsNull();
        if (rhs.IsNull()) return false;

        // different types
        if (m_val.type() != rhs.m_val.type()) return false;

        // string
        if (IsString()) {
            return boost::any_cast<wxString&>(m_val) == boost::any_cast<wxString&>(rhs.m_val);
        }
        // bool
        if (IsBool()) {
            return boost::any_cast<bool>(m_val) == boost::any_cast<bool>(rhs.m_val);
        }
        // int
        if (IsInt()) {
            return boost::any_cast<int>(m_val) == boost::any_cast<int>(rhs.m_val);
        }
        // double
        if (IsDouble()) {
            return boost::any_cast<double>(m_val) == boost::any_cast<double>(rhs.m_val);
        }
        // Map
        // Array
        return false;
    }

    /**
     * Check if value is not equal to rhs. Map and Array are not checked
     * and simply return false!
     */
    bool operator != (const Value & rhs) const noexcept
    {
        return !(*this == rhs);
    }


    /**
     * Copy assign
     */
    Value & operator = (const Value & rhs)
    {
        m_val = rhs;
        return *this;
    }


    /**
     * Move assign
     */
    Value & operator = (Value && rhs)
    {
        m_val = std::move(rhs);
        return *this;
    }

    // String
    /** \
     * Create value from type_ \
     */ explicit Value(const wxString & val) : m_val(val) {} /**
     * Create value by moving type_ \
     */ explicit Value(wxString && val) noexcept : m_val(std::move(val)) {} /** \
     * Assign type_ to value\
     */ inline Value& operator = (const wxString & rhs) { m_val = rhs; return *this; } /** \
     * Move type_ to value \
     */ inline Value & operator = (wxString && rhs) noexcept { m_val = std::move(rhs); return *this; } /** \
     * Does value hold type_? \
     */ inline bool IsString() const noexcept { return Is<wxString>(); } /** \
     * Get type_ from value. Can throw boost::bad_any_cast \
     */ inline wxString & AsString() { return this->As<wxString>(); } /** \
     * Cast value to type_&. Can throw boost::bad_any_cast \
     */ inline explicit operator wxString &() { return this->As<wxString>(); } /**
     * Cast value to type_. Can throw boost::bad_any_cast \
     */ inline explicit operator wxString() { return boost::any_cast<wxString>(m_val); }
# 216 "ConfigManager.cpp"
    explicit Value(const char * val) : m_val(wxString(val)) {}
    Value & operator = (const char * rhs)
    {
        m_val = wxString(rhs);
        return *this;
    }
    bool operator == (const char * rhs) const
    {
        return boost::any_cast<wxString&>(m_val) == rhs;
    }
    bool operator == (const wxString & rhs) const
    {
        return boost::any_cast<wxString&>(m_val) == rhs;
    }
    bool operator != (const wxString & rhs) const
    {
        return !(*this == rhs);
    }

    // Int
    /** \
     * Create value from type_ \
     */ explicit Value(const int & val) : m_val(val) {} /**
     * Create value by moving type_ \
     */ explicit Value(int && val) noexcept : m_val(std::move(val)) {} /** \
     * Assign type_ to value\
     */ inline Value& operator = (const int & rhs) { m_val = rhs; return *this; } /** \
     * Move type_ to value \
     */ inline Value & operator = (int && rhs) noexcept { m_val = std::move(rhs); return *this; } /** \
     * Does value hold type_? \
     */ inline bool IsInt() const noexcept { return Is<int>(); } /** \
     * Get type_ from value. Can throw boost::bad_any_cast \
     */ inline int & AsInt() { return this->As<int>(); } /** \
     * Cast value to type_&. Can throw boost::bad_any_cast \
     */ inline explicit operator int &() { return this->As<int>(); } /**
     * Cast value to type_. Can throw boost::bad_any_cast \
     */ inline explicit operator int() { return boost::any_cast<int>(m_val); }
# 237 "ConfigManager.cpp"
    bool operator == (int rhs) const
    {
        return boost::any_cast<int>(m_val) == rhs;
    }
    bool operator != (int rhs) const
    {
        return !(*this == rhs);
    }

    // Bool
    /** \
     * Create value from type_ \
     */ explicit Value(const bool & val) : m_val(val) {} /**
     * Create value by moving type_ \
     */ explicit Value(bool && val) noexcept : m_val(std::move(val)) {} /** \
     * Assign type_ to value\
     */ inline Value& operator = (const bool & rhs) { m_val = rhs; return *this; } /** \
     * Move type_ to value \
     */ inline Value & operator = (bool && rhs) noexcept { m_val = std::move(rhs); return *this; } /** \
     * Does value hold type_? \
     */ inline bool IsBool() const noexcept { return Is<bool>(); } /** \
     * Get type_ from value. Can throw boost::bad_any_cast \
     */ inline bool & AsBool() { return this->As<bool>(); } /** \
     * Cast value to type_&. Can throw boost::bad_any_cast \
     */ inline explicit operator bool &() { return this->As<bool>(); } /**
     * Cast value to type_. Can throw boost::bad_any_cast \
     */ inline explicit operator bool() { return boost::any_cast<bool>(m_val); }
# 248 "ConfigManager.cpp"
    bool operator == (bool rhs) const
    {
        return boost::any_cast<bool>(m_val) == rhs;
    }
    bool operator != (bool rhs) const
    {
        return !(*this == rhs);
    }

    // Double
    /** \
     * Create value from type_ \
     */ explicit Value(const double & val) : m_val(val) {} /**
     * Create value by moving type_ \
     */ explicit Value(double && val) noexcept : m_val(std::move(val)) {} /** \
     * Assign type_ to value\
     */ inline Value& operator = (const double & rhs) { m_val = rhs; return *this; } /** \
     * Move type_ to value \
     */ inline Value & operator = (double && rhs) noexcept { m_val = std::move(rhs); return *this; } /** \
     * Does value hold type_? \
     */ inline bool IsDouble() const noexcept { return Is<double>(); } /** \
     * Get type_ from value. Can throw boost::bad_any_cast \
     */ inline double & AsDouble() { return this->As<double>(); } /** \
     * Cast value to type_&. Can throw boost::bad_any_cast \
     */ inline explicit operator double &() { return this->As<double>(); } /**
     * Cast value to type_. Can throw boost::bad_any_cast \
     */ inline explicit operator double() { return boost::any_cast<double>(m_val); }
# 259 "ConfigManager.cpp"
    bool operator == (double rhs) const
    {
        return boost::any_cast<double>(m_val) == rhs;
    }
    bool operator != (double rhs) const
    {
        return !(*this == rhs);
    }

    // Map
    /** \
     * Create value from type_ \
     */ explicit Value(const Map & val) : m_val(val) {} /**
     * Create value by moving type_ \
     */ explicit Value(Map && val) noexcept : m_val(std::move(val)) {} /** \
     * Assign type_ to value\
     */ inline Value& operator = (const Map & rhs) { m_val = rhs; return *this; } /** \
     * Move type_ to value \
     */ inline Value & operator = (Map && rhs) noexcept { m_val = std::move(rhs); return *this; } /** \
     * Does value hold type_? \
     */ inline bool IsMap() const noexcept { return Is<Map>(); } /** \
     * Get type_ from value. Can throw boost::bad_any_cast \
     */ inline Map & AsMap() { return this->As<Map>(); } /** \
     * Cast value to type_&. Can throw boost::bad_any_cast \
     */ inline explicit operator Map &() { return this->As<Map>(); } /**
     * Cast value to type_. Can throw boost::bad_any_cast \
     */ inline explicit operator Map() { return boost::any_cast<Map>(m_val); }
# 271 "ConfigManager.cpp"
    // Array
    /** \
     * Create value from type_ \
     */ explicit Value(const Array & val) : m_val(val) {} /**
     * Create value by moving type_ \
     */ explicit Value(Array && val) noexcept : m_val(std::move(val)) {} /** \
     * Assign type_ to value\
     */ inline Value& operator = (const Array & rhs) { m_val = rhs; return *this; } /** \
     * Move type_ to value \
     */ inline Value & operator = (Array && rhs) noexcept { m_val = std::move(rhs); return *this; } /** \
     * Does value hold type_? \
     */ inline bool IsArray() const noexcept { return Is<Array>(); } /** \
     * Get type_ from value. Can throw boost::bad_any_cast \
     */ inline Array & AsArray() { return this->As<Array>(); } /** \
     * Cast value to type_&. Can throw boost::bad_any_cast \
     */ inline explicit operator Array &() { return this->As<Array>(); } /**
     * Cast value to type_. Can throw boost::bad_any_cast \
     */ inline explicit operator Array() { return boost::any_cast<Array>(m_val); }
# 275 "ConfigManager.cpp"
    /**
     * This valud is null
     */
    inline bool IsNull() const noexcept
    {
        return m_val.empty();
    }


    /**
     * Clear the value. Will be null
     */
    inline void Clear() noexcept
    {
        m_val.clear();
    }


    /**
     * this value is scalar
     */
    inline bool IsScalar() const noexcept
    {
        return !IsNull() &&
                m_val.type() != typeid(Map) &&
                m_val.type() == typeid(Array);
    }


    /**
     * Check if currently held value is of given type
     *
     * if (valie.Is<wxString>()) { ... }
     */
    template<typename T>
    inline bool Is() const noexcept
    {
        return !IsNull() && m_val.type() == typeid(T);
    }


    /**
     * Cast Value to the given type. This will return a reference
     * of the given type!
     *
     * auto & map = value.As<Map>();
     */
    template<typename T>
    inline T& As()
    {
        return boost::any_cast<T&>(m_val);
    }

private:

    // value holder
    mutable boost::any m_val;
};




// Load configuration
void ConfigManager::Load(const wxString & path)
{
    if (!::wxFileExists(path)) {
        throw std::invalid_argument("fbide config file '" + path + "' not found");
    }


    Value v{true};

    if (v.IsBool()) {
        if (v != false) {
            std::cout << "v == true\n";
        }
        std::cout << "v is bool\n" << v.As<bool>();
    }



    if (v.IsNull()) {
        std::cout << "empty\n";
    }

    if (v.IsString()) {
        if (v == "std::string") {
            std::cout << "YAY\n";
        }
        std::cout << v.AsString();
    }


}
