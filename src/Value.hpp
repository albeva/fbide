//
//  Value.hpp
//  fbide
//
//  Created by Albert on 14/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once

namespace fbide {
    
    /**
     * Value is variant type that uses boost::any for storage
     * It supports recursive values via Map and Array
     *
     * Supported types are:
     * - wxString
     * - bool
     * - int
     * - double
     * - Map
     * - Array
     *
     * All types except Map and Array support comparison operators == and !=
     */
    struct Value
    {
        
        /**
         * Map is key Value pairs of Value objects
         */
        typedef std::unordered_map<wxString,
                                   Value,
                                   wxStringHash,
                                   wxStringEqual> Map;
        
        /**
         * Array is a list of Value objects
         */
        typedef std::vector<Value> Array;
        
        
        /**
         * Default Value will be null
         *
         * value.IsNull() will return true
         */
        Value() {}
        
        
        /**
         * Copy value
         */
        explicit Value(const Value & other) : m_val(other) {}
        
        
        /**
         * Move value
         */
        explicit Value(Value && other) noexcept : m_val(std::move(other)) {}
        
        
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
        
        
        /**
         * Check if values are equal. Map and Array can't be checked
         * and will return false!
         */
        bool operator == (const Value & rhs) const noexcept
        {
            // addresses?
            if (this == &rhs) return true;
            
            // check if one or both are null
            if (IsNull())     return rhs.IsNull();
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
        
        
        //----------------------------------------------------------------------
        // String
        //----------------------------------------------------------------------
        
        /**
         * Create value from wxString
         */
        explicit Value(const wxString & val) : m_val(val) {}
        
        
        /**
         * Create value by moving wxString
         */
        explicit Value(wxString && val) noexcept : m_val(std::move(val)) {}
        
        /**
         * Value from const char *
         */
        explicit Value(const char * val) : m_val(wxString(val)) {}
        
        
        /**
         * Assign wxString to value
         */
        inline Value& operator = (const wxString & rhs)
        {
            m_val = rhs;
            return *this;
        }
        
        
        /**
         * Move wxString to value
         */
        inline Value & operator = (wxString && rhs) noexcept
        {
            m_val = std::move(rhs);
            return *this;
        }
        
        /**
         * Assign const char *
         */
        Value & operator = (const char * rhs)
        {
            m_val = wxString(rhs);
            return *this;
        }
        
        
        /**
         * Does value hold wxString?
         */
        inline bool IsString() const noexcept
        {
            return Is<wxString>();
        }
        
        
        /**
         * Get wxString from value.
         * @throws boost::bad_any_cast
         */
        inline wxString & AsString()
        {
            return As<wxString>();
        }
        
        
        /**
         * Cast value to wxString&
         * @throws boost::bad_any_cast
         */
        inline explicit operator wxString &()
        {
            return As<wxString>();
        }
        
        
        /**
         * Cast value to wxString
         * @throws boost::bad_any_cast
         */
        inline explicit operator wxString()
        {
            return boost::any_cast<wxString>(m_val);
        }
        
        
        /**
         * Compare value against wxString
         * @throws boost::bad_any_cast
         */
        bool operator == (const wxString & rhs) const
        {
            return boost::any_cast<wxString&>(m_val) == rhs;
        }
        
        
        /**
         * Compare non equality
         * @throws boost::bad_any_cast
         */
        bool operator != (const wxString & rhs) const
        {
            return !(*this == rhs);
        }
        
        
        /**
         * Check value equals to char *
         * @throws boost::bad_any_cast
         */
        bool operator == (const char * rhs) const
        {
            return boost::any_cast<wxString&>(m_val) == rhs;
        }
        
        /**
         * Check if value does not equal to char *
         * @throws boost::bad_any_cast
         */
        bool operator != (const char * rhs) const
        {
            return !(*this == rhs);
        }
        
        
        //----------------------------------------------------------------------
        // Int
        //----------------------------------------------------------------------
        
        /**
         * Create value from int
         */
        explicit Value(const int & val) : m_val(val) {}
        
        
        /**
         * Assign int to value
         */
        inline Value& operator = (const int & rhs)
        {
            m_val = rhs; return *this;
        }
        
        
        /**
         * Does value hold int?
         */
        inline bool IsInt() const noexcept
        {
            return Is<int>();
        }
        
        
        /**
         * Get int from value
         * @throws boost::bad_any_cast
         */
        inline int & AsInt()
        {
            return As<int>();
        }
        
        
        /**
         * Cast value to int&
         * @throws boost::bad_any_cast
         */
        inline explicit operator int &()
        {
            return As<int>();
        }
        
        
        /**
         * Cast value to int
         * @throws boost::bad_any_cast
         */
        inline explicit operator int()
        {
            return boost::any_cast<int>(m_val);
        }
        
        /**
         * Compare value to int
         * @throws boost::bad_any_cast
         */
        bool operator == (int rhs) const
        {
            return boost::any_cast<int>(m_val) == rhs;
        }
        
        /**
         * Compare value to not equal int
         * @throws boost::bad_any_cast
         */
        bool operator != (int rhs) const
        {
            return !(*this == rhs);
        }
        
        
        //----------------------------------------------------------------------
        // Bool
        //----------------------------------------------------------------------
        
        /**
         * Create value from bool
         */
        explicit Value(const bool & val) : m_val(val) {}
        
        
        /**
         * Assign bool to value
         */
        inline Value& operator = (const bool & rhs)
        {
            m_val = rhs;
            return *this;
        }
        
        
        /**
         * Does value hold bool?
         */
        inline bool IsBool() const noexcept
        {
            return Is<bool>();
        }
        
        
        /**
         * Get bool from value
         * @throws boost::bad_any_cast
         */
        inline bool & AsBool()
        {
            return As<bool>();
        }
        
        
        /**
         * Cast value to bool&
         * @throws boost::bad_any_cast
         */
        inline explicit operator bool &()
        {
            return As<bool>();
        }
        
        
        /**
         * Cast value to bool
         * @throws boost::bad_any_cast
         */
        inline explicit operator bool()
        {
            return boost::any_cast<bool>(m_val);
        }
        
        
        /**
         * Compare value is ewual to bool
         * @throws boost::bad_any_cast
         */
        bool operator == (bool rhs) const
        {
            return boost::any_cast<bool>(m_val) == rhs;
        }
        
        
        /**
         * Compare value is not equal to bool
         * @throws boost::bad_any_cast
         */
        bool operator != (bool rhs) const
        {
            return !(*this == rhs);
        }
        
        
        //----------------------------------------------------------------------
        // Double
        //----------------------------------------------------------------------
        
        /**
         * Create value from bool
         */
        explicit Value(const double & val) : m_val(val) {}
        
        
        /**
         * Assign bool to value
         */
        inline Value& operator = (const double & rhs)
        {
            m_val = rhs;
            return *this;
        }
        
        
        /**
         * Does value hold double?
         */
        inline bool IsDouble() const noexcept
        {
            return Is<double>();
        }
        
        
        /**
         * Get double from value
         * @throws boost::bad_any_cast
         */
        inline double & AsDouble()
        {
            return As<double>();
        }
        
        
        /**
         * Cast value to double&
         * @throws boost::bad_any_cast
         */
        inline explicit operator double &()
        {
            return As<double>();
        }
        
        
        /**
         * Cast value to double
         * @throws boost::bad_any_cast
         */
        inline explicit operator double()
        {
            return boost::any_cast<double>(m_val);
        }
        
        
        /**
         * Compare value is equal to double
         * @throws boost::bad_any_cast
         */
        bool operator == (double rhs) const
        {
            return boost::any_cast<double>(m_val) == rhs;
        }
        
        
        /**
         * Compare value is not equal to double
         * @throws boost::bad_any_cast
         */
        bool operator != (double rhs) const
        {
            return !(*this == rhs);
        }
        
        
        //----------------------------------------------------------------------
        // Map
        //----------------------------------------------------------------------
        
        /**
         * Create value from Map
         */
        explicit Value(const Map & val) : m_val(val) {}
        
        
        /**
         * Create value by moving Map
         */
        explicit Value(Map && val) noexcept : m_val(std::move(val)) {}
        
        
        /**
         * Assign Map to value
         */
        inline Value& operator = (const Map & rhs)
        {
            m_val = rhs; return *this;
        }
        
        
        /**
         * Move Map to value
         */
        inline Value & operator = (Map && rhs) noexcept
        {
            m_val = std::move(rhs);
            return *this;
        }
        
        
        /**
         * Does value hold Map?
         */
        inline bool IsMap() const noexcept
        {
            return Is<Map>();
        }
        
        
        /**
         * Get Map from value
         * @throws boost::bad_any_cast
         */
        inline Map & AsMap()
        {
            return As<Map>();
        }
        
        
        /**
         * Cast value to Map&
         * @throws boost::bad_any_cast
         */
        inline explicit operator Map &()
        {
            return As<Map>();
        }
        
        
        /**
         * Cast value to Map
         * @throws boost::bad_any_cast
         */
        inline explicit operator Map()
        {
            return boost::any_cast<Map>(m_val);
        }
        
        
        //----------------------------------------------------------------------
        // Array
        //----------------------------------------------------------------------
        
        /**
         * Create value from Array
         */
        explicit Value(const Array & val) : m_val(val) {}
        
        
        /**
         * Create value by moving Array
         */
        explicit Value(Array && val) noexcept : m_val(std::move(val)) {}
        
        
        /**
         * Assign Array to value
         */
        inline Value& operator = (const Array & rhs)
        {
            m_val = rhs;
            return *this;
        }
        
        
        /**
         * Move Array to value
         */
        inline Value & operator = (Array && rhs) noexcept
        {
            m_val = std::move(rhs);
            return *this;
        }
        
        
        /**
         * Does value hold Array?
         */
        inline bool IsArray() const noexcept
        {
            return Is<Array>();
        }
        
        
        /**
         * Get Array from value
         * @throws boost::bad_any_cast
         */
        inline Array & AsArray()
        {
            return As<Array>();
        }
        
        
        /**
         * Cast value to Array&
         * @throws boost::bad_any_cast
         */
        inline explicit operator Array &()
        {
            return As<Array>();
        }
        
        
        /**
         * Cast value to Array
         * @throws boost::bad_any_cast
         */
        inline explicit operator Array()
        {
            return boost::any_cast<Array>(m_val);
        }
        
        
        /**
         * Check if this value is nyll
         */
        inline bool IsNull() const noexcept
        {
            return m_val.empty();
        }
        
        
        /**
         * Clear the value to null
         */
        inline void Clear() noexcept
        {
            m_val.clear();
        }
        
        
        /**
         * Is this a scalar value?
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
         * @throws boost::bad_any_cast
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
    
}