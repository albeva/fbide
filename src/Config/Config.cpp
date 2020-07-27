//
//  Config.cpp
//  fbide
//
//  Created by Albert on 14/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "Config.hpp"
#include <yaml-cpp/yaml.h>

using namespace fbide;

namespace YAML {
template<>
struct convert<Config> {
    /**
     * Convert Config to yaml
     */
    static Node encode(const Config& rhs) {
        Node node;
        return node;
    }

    /**
     * convert yaml to Config
     */
    static bool decode(const Node& node, Config& rhs) {
        // null node
        if (node.IsNull()) {
            return true;
        }

        // array
        if (node.IsSequence()) {
            auto& arr = rhs.AsArray();
            arr.reserve(node.size());
            for (const auto& child : node) {
                arr.emplace_back(child.as<Config>());
            }
            return true;
        }

        // map
        if (node.IsMap()) {
            auto& map = rhs.AsMap();
            map.reserve(node.size());
            for (const auto& child : node) {
                auto& key = child.first.Scalar();
                auto& val = child.second;
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
} // namespace YAML

namespace {

// "proper way" of getting max unsigned long that should be compatible
// with different platforms
const auto MaxULong = std::numeric_limits<unsigned long>::max();


/**
 * Simple config path parser. Keys are separated by "."
 * and array indexes enclosed within square brackets.
 *
 * EBNF:
 * path  := part { "." part }
 * part  := key | index
 * key   := <id>
 * index := "[" <num> "]" | <num>
 */
struct PathParser {
    /**
     * Possible token types
     */
    enum class Type {
        Invalid,
        Key,
        Index,
        End
    };

    /**
     * Represent a token
     */
    struct Token {
        Type type;
        wxString str;
    };

    /**
     * Create path parser
     */
    PathParser(const wxString& path) : m_path(path) {}

    /**
     * Get next token
     */
    Token Next() noexcept {
        const auto len = m_path.length();

        while (m_pos < len) {
            auto start = m_pos;
            char ch = m_path[m_pos];

            // id
            if (std::isalpha(ch)) {
                do {
                    m_pos++;
                } while (m_pos < len && std::isalnum(m_path[m_pos]));
                auto lex = m_path.SubString(start, m_pos - 1);
                return Token{ Type::Key, lex };
            }

            // digit
            if (std::isdigit(ch)) {
                do {
                    m_pos++;
                } while (m_pos < len && std::isdigit(m_path[m_pos]));
                auto lex = m_path.SubString(start, m_pos - 1);
                return Token{ Type::Index, lex };
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

            // something wrong
            return Token{ Type::Invalid, "Invalid config path" };
        }

        // the end
        return Token{ Type::End };
    }

private:
    size_t m_pos{ 0 };
    const wxString& m_path;
};

} // namespace

Config Config::Empty{};

/**
 * Load YAML file
 */
void Config::LoadYaml(const wxString& path) {
    m_val = YAML::LoadFile(path.ToStdString()).as<Config>().m_val;
}

/**
 * Access nested config via path. Each path separated by '.'
 * is considered a key in the map. If key doesn't exist it is
 * silently created, if types mismatch will throw cast error
 * @throws std::bad_any_cast
 */
Config& Config::operator[](const wxString& path) {
    auto node = this;

    PathParser parser{ path };
    for (;;) {
        auto&& t = parser.Next();

        // finish?
        if (t.type == PathParser::Type::End) {
            break;
        }

        switch (t.type) {
        case PathParser::Type::Invalid: {
            throw std::domain_error(t.str);
            break;
        }
        case PathParser::Type::Key: {
            auto& map = node->AsMap();
            auto it = map.find(t.str);
            if (it != map.end()) {
                node = &it->second;
            } else {
                auto r = map.emplace(std::make_pair(t.str, Config()));
                node = &r.first->second;
            }
            break;
        }
        case PathParser::Type::Index: {
            unsigned long index;
            if (!t.str.ToULong(&index)) {
                index = MaxULong;
            }

            auto& arr = node->AsArray();
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
        case PathParser::Type::End: {
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
const Config* Config::Get(const wxString& path) const noexcept {
    auto node = this;

    PathParser parser{ path };
    for (;;) {
        auto t = parser.Next();

        // finish?
        if (t.type == PathParser::Type::End) {
            break;
        }

        switch (t.type) {
        case PathParser::Type::Key: {
            if (!node->IsMap()) {
                return nullptr;
            }
            auto& map = node->AsMap();
            auto it = map.find(t.str);
            if (it != map.end()) {
                node = &it->second;
            } else {
                return nullptr;
            }
            break;
        }
        case PathParser::Type::Index: {
            if (!node->IsArray()) {
                return nullptr;
            }

            unsigned long index;
            if (!t.str.ToULong(&index)) {
                index = MaxULong;
            }

            auto& arr = node->AsArray();
            if (index < arr.size()) {
                node = &arr[index];
            } else {
                return nullptr;
            }
            break;
        }
        default: {
            return nullptr;
        }
        }
    }

    return node;
}

/**
 * Output the tree to std out
 */
wxString Config::ToString(size_t indent) const noexcept {
    const size_t INDENT = 4;

    auto sp = wxString(indent * INDENT, ' ');
    auto cs = wxString((indent > 0 ? indent - 1 : 0) * INDENT, ' ');

    wxString output;

    switch (GetType()) {
    case Type::Null:
        output << "Null";
        break;
    case Type::String:
        output << '"' << AsString() << '"';
        break;
    case Type::Bool:
        output << (AsBool() ? "true" : "false");
        break;
    case Type::Int:
        output << AsInt();
        break;
    case Type::Double:
        output << AsDouble();
        break;
    case Type::Map: {
        auto& map = AsMap();
        // longest key length
        size_t max = 0;
        for (auto& n : map) {
            if (n.first.length() > max) {
                max = n.first.length();
            }
        }
        if (indent > 0) {
            output << '{' << '\n';
        }
        for (auto& n : map) {
            auto& key = n.first;
            auto& val = n.second;
            output << sp << key;
            if (val.IsScalar() || val.IsEmpty()) {
                output << wxString(max - key.length(), ' ') << " = ";
            } else {
                output << ' ';
            }
            output << val.ToString(indent + 1);
            output << '\n';
        }
        if (indent > 0) {
            output << cs << '}';
        }
        break;
    }
    case Type::Array: {
        auto& arr = AsArray();
        output << '[' << '\n';
        for (auto& val : arr) {
            output << sp;
            output << val.ToString(indent + 1);
            output << '\n';
        }
        output << cs << ']';
        break;
    }
    }
    return output;
}
