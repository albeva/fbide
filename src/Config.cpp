//
//  Config.cpp
//  fbide
//
//  Created by Albert on 14/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "app_pch.hpp"
#include "Config.hpp"
#include <yaml-cpp/yaml.h>

using namespace fbide;


namespace YAML {
    template<>
    struct convert<Config> {
        
        /**
         * Convert Config to yaml
         */
        static Node encode(const Config& rhs)
        {
            Node node;
            return node;
        }
        
        
        /**
         * convert yaml to Config
         */
        static bool decode(const Node& node, Config& rhs)
        {
            // null node
            if (node.IsNull()) {
                return true;
            }
            
            // array
            if (node.IsSequence()) {
                Config::Array arr;
                arr.reserve(node.size());
                for (auto & child : node) {
                    arr.emplace_back(child.as<Config>());
                }
                rhs = arr;
                return true;
            }
            
            // map
            if (node.IsMap()) {
                Config::Map map;
                map.reserve(node.size());
                for (const auto & child : node) {
                    auto & key = child.first.Scalar();
                    auto & val = child.second;
                    map.emplace(std::make_pair(key, val.as<Config>()));
                }
                rhs = map;
                return true;
            }
            
            // scalar
            if (node.IsScalar()) {
                try {
                    rhs = node.as<bool>();;
                    return true;
                } catch (...) {}
                
                try {
                    rhs = node.as<int>();;
                    return true;
                } catch (...) {}
                
                try {
                    rhs = node.as<double>();
                    return true;
                } catch (...) {}
                
                try {
                    rhs = node.as<std::string>();;
                    return true;
                } catch (...) {}
            }
            
            return false;
        }
    };
}


/**
 * Load YAML file
 */
Config Config::LoadYaml(const wxString & path)
{
    return YAML::LoadFile(path.ToStdString()).as<Config>();
}


/**
 * Check if Configs are equal. Map and Array can't be checked
 * and will return false!
 */
bool Config::operator == (const Config & rhs) const noexcept
{
    if (this == &rhs) return true;
    
    if (IsNull()) {
        return rhs.IsNull();
    }
    
    if (rhs.IsNull()) {
        return false;
    }
    
    if (m_val.type() != rhs.m_val.type()) return false;
    
    if (IsString()) {
        auto & l = boost::any_cast<wxString&>(const_cast<Config*>(this)->m_val);
        auto & r = boost::any_cast<wxString&>(const_cast<Config&>(rhs).m_val);
        return l == r;
    }
    
    if (IsBool()) {
        return boost::any_cast<bool>(m_val) == boost::any_cast<bool>(rhs.m_val);
    }
    
    if (IsInt()) {
        return boost::any_cast<int>(m_val) == boost::any_cast<int>(rhs.m_val);
    }
    
    if (IsDouble()) {
        return boost::any_cast<double>(m_val) == boost::any_cast<double>(rhs.m_val);
    }
    
    return false;
}


// Map and Array creation and conversion
// - if node doesn't exist it is created
// - if node is Null it is converted
// - if node type mismtahces then throw an exception
//
// Last part of the file is a leaf.
// - if cast expression exists it is enfored to be of given type
// - if no cast expression speccified then return null
//
// key value path:
// "foo.bar"
// - current scope must be a map. If scope is null then it is turned
//   into a map! Otherwise throw an exception
// - foo *must* be a map. If it does not exist
//   create one. If foo is not a map it will throw an exception
// - bar is considered a key. if it does not exist a null
//   is created and added to the map
// - returns: bar
//
// Array access
// "arr[0].bar"
// - arr must be an array. If it does not exist it is created.
//   if arr exists but is not an array it will throw an exception.
//   if index is not in the range then it will be added to the array
//   if index == size(). Thhrow an exception otherwise
// - returns: bar
//
// Append without index
// arr[].bar
// - if no index is specified then new object is appended
//   to the array
// - returns: bar
//
// Access array directly
// [0].bar
// - corrent scope must be an array. If this scope is Null
//   then it is converted into an array!
// - same rule apply about index as above
// - returns: bar
//
// Access map node
// "bar = {}"
// - if bar is null then it is converted into a map.
//   if bar doesn't exist it is added and returned
//   if there is type mismtahc it will throw an exception
// returns: bar
//
// Access array node
// "arr = []"
// - if arr is null it is converted into array
//   if arr doesn't exist it is created
//   if there is type mismtahc it will throw an exception
// - returns: arr
//
// Create and return new nested array
// "arr[] = []"
// - combine rules from above
// - returns: nested array
//
// "foo[]" and "foo.[]" are equivelent
//
// path = part { "." part } [ "=" as ]
// part = <id> [ idx ]
//      | idx
// idx  = "[" [<int>] "]"
// as   = map
//      | arr
// map  = "{}"
// arr  = "[]"
struct PathParser {
    
    /**
     * Possible token types
     */
    enum class Typ {
        Invalid,
        Id,
        Index,
        Map,
        Arr,
        End
    };
    
    
    /**
     * Represent a token
     */
    struct Tok
    {
        Typ      type;
        wxString str;
        size_t   idx;
    };
    
    
    /**
     * Create new parser instance. This will own the string
     * by reference!
     */
    PathParser(const wxString & path) : m_path(path) {}
    
    
    /**
     * Get next token
     */
    Tok Next() noexcept
    {
        const auto len = m_path.length();
        
        while (m_pos < len) {
            auto start = m_pos;
            char ch    = m_path[m_pos];
            
            // id
            if (std::isalpha(ch)) {
                do {
                    m_pos++;
                } while (m_pos < len && std::isalpha(m_path[m_pos]));
                auto lex = m_path.SubString(start, m_pos - 1);
                return Tok{Typ::Id, lex};
            }
            
            // index
            if (ch == '[') {
                start++;
                
                // while is number
                do {
                    m_pos++;
                } while (m_pos < len && std::isdigit(m_path[m_pos]));
                
                // extract the number
                auto lex = m_path.SubString(start, m_pos - 1);
                
                // extract index
                size_t index;
                if (!lex.ToULong(&index)) {
                    index = SIZE_T_MAX;
                }
                
                // closing
                if (m_path[m_pos] != ']') {
                    return Tok{Typ::Invalid, "Excpected closing ']'"};
                }
                m_pos++;
                
                // done
                return Tok{Typ::Index, lex, index};
            }
            
            // dot. skip
            if (ch == '.') {
                m_pos++;
                continue;
            }
            
            // '=' ?
            if (ch == '=') {
                m_pos++;
                if (m_pos == len - 2) {
                    if (m_path[m_pos] == '[' && m_path[m_pos+1] == ']') {
                        m_pos += 2;
                        return Tok{Typ::Arr};
                    }
                    if (m_path[m_pos] == '{' && m_path[m_pos+1] == '}') {
                        m_pos += 2;
                        return Tok{Typ::Map};
                    }
                }
                return Tok{Typ::Invalid, "Path must end with ={} or =[]"};
            }
            
            break;
        }
        
        return Tok{Typ::End};
    }
    
    
    /**
     * Check if there is more
     */
    bool HasMore() const noexcept
    {
        return m_pos < m_path.length();
    }
    
    
private:
    size_t           m_pos{0};
    const wxString & m_path;
};



/**
 * Access nested config via path. Each path separated by '.'
 * is considered a key in the map. If key doesn't exist it is
 * silently created, if types mismatch will throw cast error
 * @throws boost::bad_any_cast
 */
Config & Config::operator[](const wxString & path)
{
    auto node = this;
    
    PathParser parser{path};
    for(;;) {
        auto && t = parser.Next();
        
        // finish?
        if (t.type == PathParser::Typ::End) {
            break;
        }
        
        switch (t.type) {
            case PathParser::Typ::Invalid:
            {
                throw std::domain_error(t.str);
                break;
            }
            case PathParser::Typ::Id:
            {
                if (node->IsNull()) {
                    *node = Map();
                }
                auto & map = node->AsMap();
                auto it = map.find(t.str);
                if (it != map.end()) {
                    node = &it->second;
                } else {
                    auto r = map.emplace(std::make_pair(t.str, Config()));
                    node = &r.first->second;
                }
                break;
            }
            case PathParser::Typ::Index:
            {
                const auto idx = t.idx;
                if (node->IsNull()) {
                    *node = Array();
                }
                auto & arr = node->AsArray();
                if (idx < arr.size()) {
                    node = &arr[idx];
                } else if (idx == arr.size() || idx == SIZE_T_MAX) {
                    arr.emplace_back(Config());
                    node = &arr.back();
                } else {
                    throw std::out_of_range("Invalid index in config path");
                }
                break;
            }
            case PathParser::Typ::Map:
            {
                if (node->IsNull()) {
                    *node = Map();
                }
                node->AsMap(); // force check. Will throw an exception
                break;
            }
            case PathParser::Typ::Arr:
            {
                if (node->IsNull()) {
                    *node = Array();
                }
                node->AsArray();
                std::cout << std::boolalpha << node->IsArray() << '\n';
                break;
            }
            case PathParser::Typ::End:
            {
                break;
            }
        }
    }
    
    return *node;
}


/**
 * Access item in the array. Node type *must* be Array and if not
 * will throw an exception
 * @throws boost::bad_any_cast
 */
Config & Config::operator[](size_t index)
{
    return AsArray()[index];
}


/**
 * Get node type
 */
Config::Type Config::GetType() const noexcept
{
    if (IsString()) return Type::String;
    if (IsBool())   return Type::Bool;
    if (IsInt())    return Type::Int;
    if (IsDouble()) return Type::Double;
    if (IsMap())    return Type::Map;
    if (IsArray())  return Type::Array;
    return Type::Null;
}


/**
 * Output the tree to std out
 */
void Config::Dump(size_t indent) const
{
    const size_t INDENT = 4;
    
    auto self = const_cast<Config*>(this);
    auto sp = std::string(indent * INDENT, ' ');
    auto cs = std::string((indent> 0 ? indent - 1 : 0) * INDENT, ' ');
    
    switch (GetType()) {
        case Type::Null:
            std::cout << "Null";
            break;
        case Type::String:
            std::cout << self->AsString();
            break;
        case Type::Bool:
            std::cout << (self->AsBool() ? "true" : "false");
            break;
        case Type::Int:
            std::cout << self->AsInt();
            break;
        case Type::Double:
            std::cout << self->AsDouble();
            break;
        case Type::Map:
        {
            auto & map = self->AsMap();
            // longest key length
            size_t max = 0;
            for (auto & n : map) {
                if (n.first.length() > max) {
                    max = n.first.length();
                }
            }
            if (indent > 0) {
                std::cout << '{' << std::endl;
            }
            for (auto & n : map) {
                auto & key = n.first;
                auto & val = n.second;
                std::cout << sp << key;
                if (val.IsScalar() || val.IsNull()) {
                    std::cout << std::string(max - key.length(), ' ') << " = ";
                } else {
                    std::cout << ' ';
                }
                val.Dump(indent + 1);
                std::cout << std::endl;
            }
            if (indent > 0) {
                std::cout << cs << '}';
            }
            break;
        }
        case Type::Array:
        {
            auto & arr = self->AsArray();
            std::cout << '[' << std::endl;
            for (auto & val : arr) {
                std::cout << sp;
                val.Dump(indent + 1);
                std::cout << std::endl;
            }
            std::cout << cs << ']';
            break;
        }
    }
}
