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
     * It supports recursive Configs via Map and Array
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
         * use AsArray() method to get the underlying std::vector<Config> out.
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
        
        
        /**
         * Check if Config is not equal to rhs. Map and Array are not checked
         * and simply return false!
         */
        bool operator != (const Config & rhs) const noexcept
        {
            return !(*this == rhs);
        }
        
        
        /**
         * Default to given value if config is null
         */
        inline Config & Default(const Config & val) noexcept
        {
            if (IsNull()) {
                m_val = val.m_val;
            }
            return *this;
        }
        
        
        /**
         * Default to given value if config is null
         */
        inline Config & Default(Config && val) noexcept
        {
            if (IsNull()) {
                m_val = std::move(val.m_val);
            }
            return *this;
        }
        
        
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
        Config & operator = (const char * rhs)
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
         * Default to given value if config is null
         */
        inline Config & Default(const wxString & str) noexcept
        {
            if (IsNull()) {
                m_val = str;
            }
            return *this;
        }
        
        
        /**
         * Default to given value if config is null
         */
        inline Config & Default(wxString && str) noexcept
        {
            if (IsNull()) {
                m_val = std::move(str);
            }
            return *this;
        }
        
        
        /**
         * Default to given value if config is null
         */
        inline Config & Default(const char * str) noexcept
        {
            if (IsNull()) {
                m_val = wxString(str);
            }
            return *this;
        }
        
        
        /**
         * Get wxString from Config.
         * @throws boost::bad_any_cast
         */
        inline wxString & AsString()
        {
            return As<wxString>();
        }
        
        
        /**
         * Cast Config to wxString&
         * @throws boost::bad_any_cast
         */
        inline explicit operator wxString &()
        {
            return As<wxString>();
        }
        
        
        /**
         * Cast Config to wxString
         * @throws boost::bad_any_cast
         */
        inline explicit operator wxString()
        {
            return boost::any_cast<wxString>(m_val);
        }
        
        
        /**
         * Compare Config against wxString
         * @throws boost::bad_any_cast
         */
        bool operator == (const wxString & rhs) const
        {
            auto & l = boost::any_cast<wxString&>(
                const_cast<Config*>(this)->m_val
            );
            return l == rhs;
            return false;
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
         * Check Config equals to char *
         * @throws boost::bad_any_cast
         */
        bool operator == (const char * rhs) const
        {
            auto & l = boost::any_cast<wxString&>(
                const_cast<Config*>(this)->m_val
            );
            return l == rhs;
        }
        
        /**
         * Check if Config does not equal to char *
         * @throws boost::bad_any_cast
         */
        bool operator != (const char * rhs) const
        {
            return !(*this == rhs);
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
         * Default to given value if config is null
         */
        inline Config & Default(bool val) noexcept
        {
            if (IsNull()) {
                m_val = val;
            }
            return *this;
        }
        
        
        /**
         * Get bool from Config
         * @throws boost::bad_any_cast
         */
        inline bool & AsBool()
        {
            return As<bool>();
        }
        
        
        /**
         * Cast Config to bool&
         * @throws boost::bad_any_cast
         */
        inline explicit operator bool &()
        {
            return As<bool>();
        }
        
        
        /**
         * Cast Config to bool
         * @throws boost::bad_any_cast
         */
        inline explicit operator bool()
        {
            return boost::any_cast<bool>(m_val);
        }
        
        
        /**
         * Compare Config is ewual to bool
         * @throws boost::bad_any_cast
         */
        bool operator == (bool rhs) const
        {
            return boost::any_cast<bool>(m_val) == rhs;
        }
        
        
        /**
         * Compare Config is not equal to bool
         * @throws boost::bad_any_cast
         */
        bool operator != (bool rhs) const
        {
            return !(*this == rhs);
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
         * Default to given value if config is null
         */
        inline Config & Default(int val) noexcept
        {
            if (IsNull()) {
                m_val = val;
            }
            return *this;
        }
        
        
        /**
         * Get int from Config
         * @throws boost::bad_any_cast
         */
        inline int & AsInt()
        {
            return As<int>();
        }
        
        
        /**
         * Cast Config to int&
         * @throws boost::bad_any_cast
         */
        inline explicit operator int &()
        {
            return As<int>();
        }
        
        
        /**
         * Cast Config to int
         * @throws boost::bad_any_cast
         */
        inline explicit operator int()
        {
            return boost::any_cast<int>(m_val);
        }
        
        
        /**
         * Compare Config to int
         * @throws boost::bad_any_cast
         */
        bool operator == (int rhs) const
        {
            return boost::any_cast<int>(m_val) == rhs;
        }
        
        
        /**
         * Compare Config to not equal int
         * @throws boost::bad_any_cast
         */
        bool operator != (int rhs) const
        {
            return !(*this == rhs);
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
         * Default to given value if config is null
         */
        inline Config & Default(double val) noexcept
        {
            if (IsNull()) {
                m_val = val;
            }
            return *this;
        }
        
        
        /**
         * Get double from Config
         * @throws boost::bad_any_cast
         */
        inline double & AsDouble()
        {
            return As<double>();
        }
        
        
        /**
         * Cast Config to double&
         * @throws boost::bad_any_cast
         */
        inline explicit operator double &()
        {
            return As<double>();
        }
        
        
        /**
         * Cast Config to double
         * @throws boost::bad_any_cast
         */
        inline explicit operator double()
        {
            return boost::any_cast<double>(m_val);
        }
        
        
        /**
         * Compare Config is equal to double
         * @throws boost::bad_any_cast
         */
        bool operator == (double rhs) const
        {
            return boost::any_cast<double>(m_val) == rhs;
        }
        
        
        /**
         * Compare Config is not equal to double
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
         * Default to given value if config is null
         */
        inline Config & Default(const Map & val) noexcept
        {
            if (IsNull()) {
                m_val = val;
            }
            return *this;
        }
        
        
        /**
         * Default to given value if config is null
         */
        inline Config & Default(Map && val) noexcept
        {
            if (IsNull()) {
                m_val = std::move(val);
            }
            return *this;
        }
        
        
        /**
         * Get Map from Config
         * @throws boost::bad_any_cast
         */
        inline Map & AsMap()
        {
            return As<Map>();
        }
        
        
        /**
         * Cast Config to Map&
         * @throws boost::bad_any_cast
         */
        inline explicit operator Map &()
        {
            return As<Map>();
        }
        
        
        /**
         * Cast Config to Map
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
         * Default to given value if config is null
         */
        inline Config & Default(const Array & val) noexcept
        {
            if (IsNull()) {
                m_val = val;
            }
            return *this;
        }
        
        
        /**
         * Default to given value if config is null
         */
        inline Config & Default(Array && val) noexcept
        {
            if (IsNull()) {
                m_val = std::move(val);
            }
            return *this;
        }
        
        
        /**
         * Get Array from Config
         * @throws boost::bad_any_cast
         */
        inline Array & AsArray()
        {
            return As<Array>();
        }
        
        
        /**
         * Cast Config to Array&
         * @throws boost::bad_any_cast
         */
        inline explicit operator Array &()
        {
            return As<Array>();
        }
        
        
        /**
         * Cast Config to Array
         * @throws boost::bad_any_cast
         */
        inline explicit operator Array()
        {
            return boost::any_cast<Array>(m_val);
        }
        
        
        //----------------------------------------------------------------------
        // Utilities
        //----------------------------------------------------------------------
        
        
        /**
         * Get node type as enum value
         */
        Type GetType() const noexcept;
        
        
        /**
         * print Config tree to console out
         */
        void Dump(size_t indent = 0) const;
        
        
        /**
         * Check if this Config is nyll
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
         * Is this a scalar Config?
         */
        inline bool IsScalar() const noexcept
        {
            return !IsNull()
                && m_val.type() != typeid(Map)
                && m_val.type() != typeid(Array);
        }
        
        
        /**
         * Check if currently held Config is of given type
         *
         * if (valie.Is<wxString>()) { ... }
         */
        template<typename T>
        inline bool Is() const noexcept
        {
            return !IsNull() && m_val.type() == typeid(T);
        }
        
        
        /**
         * Cast Config to the given type. This will return a reference
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
        
        // Config holder
        boost::any m_val;
    };
}
