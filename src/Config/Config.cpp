/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */
#include "Config.hpp"

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wconversion"
    #pragma GCC diagnostic ignored "-Wshadow"
    #include <yaml-cpp/yaml.h>
    #pragma GCC diagnostic pop
#else
    #include <yaml-cpp/yaml.h>
#endif

#if __has_include(<memory_resource>)
    #include <memory_resource>
#else
#define NO_MEMORY_RESOURCE
#endif

using namespace fbide;

namespace YAML {
template<>
struct convert<Config> {
    /**
     * Convert Config to yaml
     */
    static Node encode(const Config& /* rhs */) {
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

    explicit PathParser(const wxString& path) noexcept: m_path(path) {}

    size_t Next() noexcept {
        const auto len = m_path.length();
        auto start = m_pos;
        while (m_pos < len && m_path[m_pos] != '.') {
            m_pos++;
        }
        return m_pos - start;
    }

    size_t m_pos{ 0 };
    const wxString& m_path;
};

} // namespace

const Config Config::Empty{};

#ifdef NO_MEMORY_RESOURCE
struct Config::details {};
void* Config::allocate() {
    return malloc(sizeof(Value));
}

void Config::deallocate(void* value) {
    free(value);
}
#else
struct Config::details {
    inline static std::pmr::unsynchronized_pool_resource p_configPool{ // NOLINT
        std::pmr::pool_options{0, sizeof(Config::Value)}
    };
};

void* Config::allocate() {
    return Config::details::p_configPool.allocate(sizeof(Value));
}

void Config::deallocate(void* value) {
    Config::details::p_configPool.deallocate(value, sizeof(Value));
}
#endif

/**
 * Load YAML file
 */
Config Config::LoadYaml(const wxString& path) {
    if (!wxFileExists(path)) {
        wxLogError("File '" + path + "' not found"); // NOLINT
        return {};
    }
    auto file = YAML::LoadFile(path.ToStdString());
    if (file.IsNull()) {
        return {};
    }
    return file.as<Config>();
}

/**
 * Access nested config via path. Each path separated by '.'
 * is considered a key in the map. If key doesn't exist it is
 * silently created, if types mismatch will throw cast error
 * @throws std::bad_any_cast
 */
Config& Config::operator[](const wxString& path) {
    auto *node = this;

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
    const auto *node = this;

    PathParser parser{ path };
    for (;;) {
        const auto len = parser.Next();
        if (len == 0) {
            break;
        }

        if (!node->IsMap()) {
            return nullptr;
        }

        const auto& map = node->AsMap();
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

    const wxString sp(indent * INDENT, ' ');
    const wxString cs((indent > 0 ? indent - 1 : 0) * INDENT, ' ');

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
        const auto& map = AsMap();
        if (map.empty()) {
            output << "{}";
            break;
        }

        // longest key length
        size_t max = 0;
        for (const auto& n : map) {
            if (n.first.length() > max) {
                max = n.first.length();
            }
        }
        if (indent > 0) {
            output << '{' << '\n';
        }
        for (const auto& n : map) {
            const auto& key = n.first;
            const auto& val = n.second;
            output << sp << key;
            if (val.IsScalar() || val.IsNull()) {
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
        const auto& arr = AsArray();
        if (arr.empty()) {
            output << "[]";
            break;
        }

        output << '[' << '\n';
        for (const auto& val : arr) {
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
