//
//  Config.hpp
//  fbide
//
//  Created by Albert on 14/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once

namespace fbide {
    
    
    /**
     * Config is variant type that uses boost::any for storage
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
     *
     * All types except Map and Array support comparison operators == and !=
     */
    struct Config
    {
        /**
         * Config node type for use with node.Type() method
         */
        enum class Type {
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
        typedef std::unordered_map<wxString,
                                   Config,
                                   wxStringHash,
                                   wxStringEqual> Map;
        
        
        /**
         * Array is a list of Config objects
         */
        typedef std::vector<Config> Array;
        
        
        /**
         * Load yaml file and return Config object
         */
        Config static LoadYaml(const wxString & path);
        
        
        /**
         * Path is a period('.') separated string where each part
         * is considered as a key to a Map. Thus 'foo.bar' will get node 'bar'
         * in a Map pointed by 'foo'. If any part of the path does not yet
         * exist it is silently created. If leaf (last bit of the path) doesn't
         * exist it is created as an empty node (IsNull() will return true)
         *
         * If any key points to a wrong type this will throw an exception
         *
         * If any part of the path is a number (unsigned long) then it is
         * considered to be an index into an array. Array will be created only
         * if index is next in sequence. Otherwise there will be an exception
         *
         * @throws boost::bad_any_cast
         */
        Config & operator[](const wxString & path);
        
        
        /**
         * Considers this node an array and will return element at given
         * index. If node is not an array this will throw an exception.
         *
         * This is convinience method. To get full control over the array should
         * use AsArray() method to get the underlying std::vector<Config>& out.
         *
         * @throws boost::bad_any_cast
         * @throws std::out_of_range
         */
        Config & operator[](size_t index);
        
        
        //----------------------------------------------------------------------
        // ctors, copy, assign ...
        //----------------------------------------------------------------------
        
        
        /**
         * Default Config will be null
         *
         * Config.IsNull() will return true
         */
        Config() = default;
        
        
        /**
         * Copy Config
         */
        Config(const Config & other) = default;
        
        
        /**
         * Move Config
         */
        Config(Config && other) noexcept = default;
        
        
        /**
         * Copy assign
         */
        Config & operator = (const Config & rhs) = default;
        
        
        /**
         * Move assign
         */
        Config & operator = (Config && rhs) noexcept = default;
        
        
        /**
         * Check if Configs are equal. Map and Array can't be checked
         * and will return false!
         */
        bool operator == (const Config & rhs) const noexcept;

        
        //----------------------------------------------------------------------
        // String
        //----------------------------------------------------------------------
        
        
        /**
         * Create Config from wxString
         */
        explicit Config(const wxString & val) : m_val(val) {}
        
        
        /**
         * Create Config by moving wxString
         */
        explicit Config(wxString && val) noexcept : m_val(std::move(val)) {}
        
        
        /**
         * Config from const char *
         */
        explicit Config(const char * val) : m_val(wxString(val)) {}
        
        
        /**
         * Assign wxString to Config
         */
        inline Config & operator = (const wxString & rhs)
        {
            m_val = rhs;
            return *this;
        }
        
        
        /**
         * Move wxString to Config
         */
        inline Config & operator = (wxString && rhs) noexcept
        {
            m_val = std::move(rhs);
            return *this;
        }
        
        /**
         * Assign const char *
         */
        inline Config & operator = (const char * rhs)
        {
            m_val = wxString(rhs);
            return *this;
        }

        
        /**
         * Does Config hold wxString?
         */
        inline bool IsString() const noexcept
        {
            return Is<wxString>();
        }
        
        
        /**
         * Get wxString from Config. Null is converted to string!
         * @throws boost::bad_any_cast
         */
        inline wxString & AsString()
        {
            return As<wxString>();
        }
        
        
        /**
         * Cast Config to wxString&. Null is converted to string!
         * @throws boost::bad_any_cast
         */
        inline operator wxString &()
        {
            return As<wxString>();
        }
        
        
        //----------------------------------------------------------------------
        // Bool
        //----------------------------------------------------------------------
        
        
        /**
         * Create Config from bool
         */
        explicit Config(bool val) : m_val(val) {}
        
        
        /**
         * Assign bool to Config
         */
        inline Config& operator = (const bool & rhs)
        {
            m_val = rhs;
            return *this;
        }
        
        
        /**
         * Does Config hold bool?
         */
        inline bool IsBool() const noexcept
        {
            return Is<bool>();
        }
        
        
        /**
         * Get bool from Config. Null is converted to bool!
         * @throws boost::bad_any_cast
         */
        inline bool & AsBool()
        {
            return As<bool>();
        }
        
        
        /**
         * Cast Config to bool&. Null is converted to bool!
         * @throws boost::bad_any_cast
         */
        inline operator bool &()
        {
            return As<bool>();
        }
        
        
        //----------------------------------------------------------------------
        // Int
        //----------------------------------------------------------------------
        
        
        /**
         * Create Config from int
         */
        explicit Config(int val) : m_val(val) {}
        
        
        /**
         * Assign int to Config
         */
        inline Config& operator = (int rhs)
        {
            m_val = rhs; return *this;
        }
        
        
        /**
         * Does Config hold int?
         */
        inline bool IsInt() const noexcept
        {
            return Is<int>();
        }
        
        
        /**
         * Get int from Config. Null is converted to int!
         * @throws boost::bad_any_cast
         */
        inline int & AsInt()
        {
            return As<int>();
        }
        
        
        /**
         * Cast Config to int&. Null is converted to int!
         * @throws boost::bad_any_cast
         */
        inline operator int &()
        {
            return As<int>();
        }
        
        
        //----------------------------------------------------------------------
        // Double
        //----------------------------------------------------------------------
        
        
        /**
         * Create Config from bool
         */
        explicit Config(double val) : m_val(val) {}
        
        
        /**
         * Assign bool to Config
         */
        inline Config& operator = (double rhs)
        {
            m_val = rhs;
            return *this;
        }
        
        
        /**
         * Does Config hold double?
         */
        inline bool IsDouble() const noexcept
        {
            return Is<double>();
        }
        
        
        /**
         * Get double from Config. Null is converted to double!
         * @throws boost::bad_any_cast
         */
        inline double & AsDouble()
        {
            return As<double>();
        }
        
        
        /**
         * Cast Config to double&. Null is converted to double!
         * @throws boost::bad_any_cast
         */
        inline operator double &()
        {
            return As<double>();
        }
        
        
        //----------------------------------------------------------------------
        // Map
        //----------------------------------------------------------------------
        
        
        /**
         * Create Config from Map
         */
        Config(const Map & val) : m_val(val) {}
        
        
        /**
         * Create Config by moving Map
         */
        Config(Map && val) noexcept : m_val(std::move(val)) {}
        
        
        /**
         * Assign Map to Config
         */
        inline Config& operator = (const Map & rhs)
        {
            m_val = rhs; return *this;
        }
        
        
        /**
         * Move Map to Config
         */
        inline Config & operator = (Map && rhs) noexcept
        {
            m_val = std::move(rhs);
            return *this;
        }
        
        
        /**
         * Does Config hold Map?
         */
        inline bool IsMap() const noexcept
        {
            return Is<Map>();
        }
        
        
        /**
         * Get Map from Config. Null is converted to map!
         * @throws boost::bad_any_cast
         */
        inline Map & AsMap()
        {
            return As<Map>();
        }
        
        
        /**
         * Cast Config to Map&. Null is converted to map!
         * @throws boost::bad_any_cast
         */
        inline operator Map &()
        {
            return As<Map>();
        }
        
        
        //----------------------------------------------------------------------
        // Array
        //----------------------------------------------------------------------
        
        
        /**
         * Create Config from Array
         */
        Config(const Array & val) : m_val(val) {}
        
        
        /**
         * Create Config by moving Array
         */
        Config(Array && val) noexcept : m_val(std::move(val)) {}
        
        
        /**
         * Assign Array to Config
         */
        inline Config& operator = (const Array & rhs)
        {
            m_val = rhs;
            return *this;
        }
        
        
        /**
         * Move Array to Config
         */
        inline Config & operator = (Array && rhs) noexcept
        {
            m_val = std::move(rhs);
            return *this;
        }
        
        
        /**
         * Does Config hold Array?
         */
        inline bool IsArray() const noexcept
        {
            return Is<Array>();
        }

        
        /**
         * Get Array from Config. Null is converted to array!
         * @throws boost::bad_any_cast
         */
        inline Array & AsArray()
        {
            return As<Array>();
        }
        
        
        /**
         * Cast Config to Array&. Null is converted to array!
         * @throws boost::bad_any_cast
         */
        inline operator Array &()
        {
            return As<Array>();
        }
        
        
        //----------------------------------------------------------------------
        // Utilities
        //----------------------------------------------------------------------
        
        
        /**
         * print Config tree to console out
         */
        void Dump(size_t indent = 0) const;
        
        
        /**
         * Get node type as enum value
         */
        Type GetType() const noexcept;
        
        
        /**
         * Is this a scalar Config?
         */
        inline bool IsScalar() const noexcept
        {
            return !IsNull()
                && m_val.type() != typeid(Map)
                && m_val.type() != typeid(Array);
        }
        
        
        /**
         * Check if this Config is null
         */
        inline bool IsNull() const noexcept
        {
            return m_val.empty();
        }
        
        
        /**
         * Clear the Config to null
         */
        inline void Clear() noexcept
        {
            m_val.clear();
        }
        
        
        /**
         * Check if currently held Config is of given type
         *
         * if (value.Is<wxString>()) { ... }
         */
        template<typename T>
        inline bool Is() const noexcept
        {
            return !IsNull() && m_val.type() == typeid(T);
        }
        
        
        /**
         * Compare rhs of type T against held value.
         * Will return false if types don't match.
         *
         * When comparing against const char * we need to coerce the type
         * to wxString - hence the conditional_t stuff.
         */
        template<typename T, typename U = std::conditional_t<is_one_of<T, char*, std::string>(), wxString, T>>
        inline bool operator == (const T& rhs) const noexcept
        {
            if (!Is<U>()) return false;
            auto & lhs = boost::any_cast<const U&>(m_val);
            return lhs == rhs;
        }
        
        
        /**
         * Inequality check
         */
        template<typename T>
        inline bool operator != (const T& rhs) const noexcept
        {
            return !(*this == rhs);
        }
        
        
    private:

        
        /**
         * Return Config as given type. Null is propagated
         * to the type while other type mismatches will throw
         * an exception.
         *
         * @throws boost::bad_any_cast
         */
        template<typename T>
        inline T& As()
        {
            if (IsNull()) {
                m_val = T();
            }
            return boost::any_cast<T&>(m_val);
        }
        
        // Config holder
        boost::any m_val;
    };
}
