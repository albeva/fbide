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

/**
 * Simple config path parser. Keys are separated by "."
 */
struct PathParser {
    /**
     * Create path parser
     */
    PathParser(const wxString& path) noexcept: m_path(path) {}

    /**
     * Get next token
     */
    int Next() noexcept {
        const auto len = m_path.length();
        auto start = m_pos;
        while (m_pos < len && m_path[m_pos] != '.') m_pos++;
        return m_pos - start;
    }

    size_t m_pos{ 0 };
    const wxString& m_path;
};

} // namespace

const Config Config::Empty{};

/**
 * Load YAML file
 */
Config Config::LoadYaml(const wxString& path) {
    return YAML::LoadFile(path.ToStdString()).as<Config>();
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
        const auto len = parser.Next();

        if (len == 0) {
            break;
        }

        auto& map = node->AsMap();
        auto str = path.Mid(parser.m_pos - len, len);
        parser.m_pos += 1;

        auto it = map.find(str);
        if (it != map.end()) {
            node = &it->second;
        } else {
            auto r = map.emplace(str, Config());
            node = &r.first->second;
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
        const auto len = parser.Next();
        if (len == 0) {
            break;
        }

        if (!node->IsMap()) {
            return nullptr;
        }

        auto& map = node->AsMap();
        auto str = path.Mid(parser.m_pos - len, len);
        parser.m_pos += 1;

        auto it = map.find(str);
        if (it != map.end()) {
            node = &it->second;
        } else {
            return nullptr;
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
