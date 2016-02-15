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


/**
 * Access nested config via path. Each path separated by '.'
 * is considered a key in the map. If key doesn't exist it is
 * silently created, if types mismatch will throw cast error
 * @throws boost::bad_any_cast
 */
Config & Config::operator[](const wxString & path)
{
    auto node = this;
    
    wxStringTokenizer tok{path, "."};
    while (tok.HasMoreTokens()) {
        auto key = tok.GetNextToken();
        
        // jump here
        loop:
        
        // if part is a number then consider this to be an array!
        unsigned long idx;
        if (key.ToULong(&idx)) {
            auto & arr = node->AsArray();
            // good index exists!
            if (idx < arr.size()) {
                node = &arr[idx];
                continue;
            }
            // is next element?
            if (idx == arr.size()) {
                // if there are more tokens then create a map or an array
                // dependign on the next token.
                if (tok.HasMoreTokens()) {
                    auto next = tok.GetNextToken();
                    // is a number? create an array
                    unsigned long idx;
                    if (next.ToULong(&idx)) {
                        arr.emplace_back(Array());
                        node = &arr.back();
                    }
                    // otherwise create a map
                    else {
                        arr.emplace_back(Map());
                        node = &arr.back();
                    }
                    // jump the looop
                    key = next;
                    goto loop;
                }
                // else create empty node null node
                else {
                    arr.emplace_back();
                    node = &arr.back();
                    break;
                }
            }
            // throw an exception
            else {
                throw std::out_of_range("Invalid index into an array");
            }
        }
        // Map
        else {
            auto & map = node->AsMap();
            
            // if key is in the map then this
            // key points to the next node
            auto iter = map.find(key);
            if (iter != map.end()) {
                node = &iter->second;
                continue;
            }
            
            // if there are more tokens then create a map or an array
            // dependign on the next token.
            if (tok.HasMoreTokens()) {
                auto next = tok.GetNextToken();
                // is a number? create an array
                unsigned long idx;
                if (next.ToULong(&idx)) {
                    node = &map.emplace(std::make_pair(key, Array())).first->second;
                }
                // otherwise create a map
                else {
                    node = &map.emplace(std::make_pair(key, Map())).first->second;
                }
                // jump the looop
                key = next;
                goto loop;
            }
            // else create empty node null node
            else {
                node = &map.emplace(std::make_pair(key, Config())).first->second;
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
