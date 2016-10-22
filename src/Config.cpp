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
                auto & arr = rhs.AsArray();
                arr.reserve(node.size());
                for (const auto & child : node) {
                    arr.emplace_back(child.as<Config>());
                }
                return true;
            }
            
            // map
            if (node.IsMap()) {
                auto & map = rhs.AsMap();
                map.reserve(node.size());
                for (const auto & child : node) {
                    auto & key = child.first.Scalar();
                    auto & val = child.second;
                    map.emplace(std::make_pair(key, val.as<Config>()));
                }
                return true;
            }
            
            // scalar
            if (node.IsScalar()) {
                try {
                    rhs = node.as<bool>();
                    return true;
                } catch (...) {}
                
                try {
                    rhs = node.as<int>();
                    return true;
                } catch (...) {}
                
                try {
                    rhs = node.as<double>();
                    return true;
                } catch (...) {}
                
                try {
                    rhs = node.as<std::string>();
                    return true;
                } catch (...) {}
            }
            
            return false;
        }
    };
}

namespace {

	// "proper way" of getting max unsigned long that should be compatible
	// with different platforms
	const auto MaxULong = std::numeric_limits<unsigned long>::max();


	/**
	 * Simple config path parser. Keys are separated by "."
	 * and array indexes enclosed within square brackets.
	 * Path can end with "cast" to force last node to be either
	 * array or a map.
	 *
	 * EBNF:
	 * Path  := [ part { "." part } ] [ "=" cast ]
	 * part  := key | index
	 * key   := <id>
	 * index := "[" [ <num> ] "]"
	 * cast  := map | arr
	 * map   := "{}"
	 * arr   := "[]"
	 */
	struct PathParser {

		/**
		 * Possible token types
		 */
		enum class Type {
			Invalid,
			Key,
			Index,
			Map,
			Array,
			End
		};


		/**
		 * Represent a token
		 */
		struct Token
		{
			Type     type;
			wxString str;
		};


		/**
		 * Create path parser
		 */
		PathParser(const wxString & path) : m_path(path) {}


		/**
		 * Get next token
		 */
		Token Next() noexcept
		{
			const auto len = m_path.length();

			while (m_pos < len) {
				auto start = m_pos;
				char ch = m_path[m_pos];

				// id
				if (std::isalpha(ch)) {
					do {
						m_pos++;
					} while (m_pos < len && std::isalpha(m_path[m_pos]));
					auto lex = m_path.SubString(start, m_pos - 1);
					return Token{ Type::Key, lex };
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
					unsigned long index;
					if (!lex.ToULong(&index)) {
						index = MaxULong;
					}

					// closing
					if (m_path[m_pos] != ']') {
						return Token{ Type::Invalid, "Excpected closing ']'" };
					}
					m_pos++;

					// done
					return Token{ Type::Index, lex };
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
						if (m_path[m_pos] == '[' && m_path[m_pos + 1] == ']') {
							m_pos += 2;
							return Token{ Type::Array };
						}
						if (m_path[m_pos] == '{' && m_path[m_pos + 1] == '}') {
							m_pos += 2;
							return Token{ Type::Map };
						}
					}
					return Token{ Type::Invalid, "Path must end with ={} or =[]" };
				}

				// something wrong
				return Token{ Type::Invalid, "Invalid config path" };
			}

			// the end
			return Token{ Type::End };
		}

	private:
		size_t           m_pos{ 0 };
		const wxString & m_path;
	};

}


/**
 * Load YAML file
 */
void Config::LoadYaml(const wxString & path)
{
    m_val = YAML::LoadFile(path.ToStdString()).as<Config>().m_val;
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
        auto & l = boost::any_cast<const wxString&>(m_val);
        auto & r = boost::any_cast<const wxString&>(rhs.m_val);
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
    
    PathParser parser{path};
    for(;;) {
        auto && t = parser.Next();
        
        // finish?
        if (t.type == PathParser::Type::End) {
            break;
        }
        
        switch (t.type) {
            case PathParser::Type::Invalid:
            {
                throw std::domain_error(t.str);
                break;
            }
            case PathParser::Type::Key:
            {
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
            case PathParser::Type::Index:
            {
                unsigned long index;
                if (!t.str.ToULong(&index)) {
                    index = MaxULong;
                }
                
                auto & arr = node->AsArray();
                if (index < arr.size()) {
                    node = &arr[index];
                } else if (index == arr.size() || index == MaxULong) {
                    arr.emplace_back(Config());
                    node = &arr.back();
                } else {
                    throw std::out_of_range("Invalid index in config path");
                }
                break;
            }
            case PathParser::Type::Map:
            {
                node->AsMap(); // force check. Will throw an exception
                break;
            }
            case PathParser::Type::Array:
            {
                node->AsArray();
                break;
            }
            case PathParser::Type::End:
            {
                break;
            }
        }
    }
    
    return *node;
}


/**
 * Attempt to find node at given path.
 * If node is not found or there is path mismatch this will
 * return a nullptr. Does not throw nor modify Config
 */
const Config * Config::Get(const wxString & path) const noexcept
{
    auto node = this;
    
    PathParser parser{path};
    for(;;) {
        auto t = parser.Next();
        
        // finish?
        if (t.type == PathParser::Type::End) {
            break;
        }
        
        switch (t.type) {
            case PathParser::Type::Key:
            {
                if (!node->IsMap()) {
                    return nullptr;
                }
                auto & map = boost::any_cast<const Map&>(node->m_val);
                auto it = map.find(t.str);
                if (it != map.end()) {
                    node = &it->second;
                } else {
                    return nullptr;
                }
                break;
            }
            case PathParser::Type::Index:
            {
                if (!node->IsArray()) {
                    return nullptr;
                }
                
                unsigned long index;
                if (!t.str.ToULong(&index)) {
                    index = MaxULong;
                }
                
                auto & arr = boost::any_cast<const Array&>(node->m_val);
                if (index < arr.size()) {
                    node = &arr[index];
                } else {
                    return nullptr;
                }
                break;
            }
            default:
            {
                return nullptr;
            }
        }
    }
    
    return node;
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
    auto cs = std::string((indent > 0 ? indent - 1 : 0) * INDENT, ' ');
    
    switch (GetType()) {
        case Type::Null:
            std::cout << "Null";
            break;
        case Type::String:
            std::cout << self->AsString();
            break;
        case Type::Bool:
            std::cout << std::boolalpha << self->AsBool();
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
