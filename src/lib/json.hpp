// Minimal JSON subset for monster loading
// Full version: https://github.com/nlohmann/json
// Download json.hpp from above URL and replace this file
#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace nlohmann {

class json {
public:
    enum class value_t { null, object, array, string, number_integer, number_float, boolean };

    json() : m_type(value_t::null) {}
    json(const std::string& s) : m_type(value_t::string), m_string(s) {}
    json(int v) : m_type(value_t::number_integer), m_int(v) {}
    json(float v) : m_type(value_t::number_float), m_float(v) {}
    json(bool v) : m_type(value_t::boolean), m_bool(v) {}

    static json parse(const std::string& text);
    static json parse(std::istream& is) {
        std::string content((std::istreambuf_iterator<char>(is)),
                             std::istreambuf_iterator<char>());
        return parse(content);
    }

    json& operator[](const std::string& key) {
        m_type = value_t::object;
        return m_object[key];
    }
    const json& operator[](const std::string& key) const {
        auto it = m_object.find(key);
        if (it == m_object.end()) throw std::runtime_error("Key not found: " + key);
        return it->second;
    }
    json& operator[](size_t idx) { return m_array[idx]; }
    const json& operator[](size_t idx) const { return m_array[idx]; }

    bool contains(const std::string& key) const {
        return m_object.count(key) > 0;
    }

    size_t size() const {
        if (m_type == value_t::array)  return m_array.size();
        if (m_type == value_t::object) return m_object.size();
        return 0;
    }

    // Iterators for arrays
    std::vector<json>::iterator begin() { return m_array.begin(); }
    std::vector<json>::iterator end()   { return m_array.end(); }
    std::vector<json>::const_iterator begin() const { return m_array.begin(); }
    std::vector<json>::const_iterator end()   const { return m_array.end(); }

    // Type conversions
    std::string get_string() const { return m_string; }
    int         get_int()    const { return m_int; }
    bool        get_bool()   const { return m_bool; }

    template<typename T> T get() const;

    value_t type() const { return m_type; }
    bool is_array()  const { return m_type == value_t::array; }
    bool is_object() const { return m_type == value_t::object; }
    bool is_string() const { return m_type == value_t::string; }
    bool is_number() const { return m_type == value_t::number_integer || m_type == value_t::number_float; }

private:
    value_t m_type;
    std::string m_string;
    int   m_int   = 0;
    float m_float = 0.f;
    bool  m_bool  = false;
    std::map<std::string, json> m_object;
    std::vector<json> m_array;

    static std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        return (a == std::string::npos) ? "" : s.substr(a, b-a+1);
    }

    static json parseValue(const std::string& s, size_t& pos);
    static std::string parseString(const std::string& s, size_t& pos);
    static json parseObject(const std::string& s, size_t& pos);
    static json parseArray(const std::string& s, size_t& pos);
    static json parseNumber(const std::string& s, size_t& pos);
    static void skipWS(const std::string& s, size_t& pos) {
        while (pos < s.size() && (s[pos]==' '||s[pos]=='\t'||s[pos]=='\r'||s[pos]=='\n'))
            ++pos;
    }
};

template<> inline std::string json::get<std::string>() const { return m_string; }
template<> inline int         json::get<int>()         const { return m_type==value_t::number_float?(int)m_float:m_int; }
template<> inline float       json::get<float>()       const { return m_type==value_t::number_float?m_float:(float)m_int; }
template<> inline bool        json::get<bool>()        const { return m_bool; }

inline std::string json::parseString(const std::string& s, size_t& pos) {
    ++pos; // skip "
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\') { ++pos; result += s[pos]; }
        else result += s[pos];
        ++pos;
    }
    ++pos; // skip closing "
    return result;
}

inline json json::parseObject(const std::string& s, size_t& pos) {
    json obj; obj.m_type = value_t::object;
    ++pos; // skip {
    skipWS(s, pos);
    while (pos < s.size() && s[pos] != '}') {
        skipWS(s, pos);
        std::string key = parseString(s, pos);
        skipWS(s, pos);
        ++pos; // skip :
        skipWS(s, pos);
        obj.m_object[key] = parseValue(s, pos);
        skipWS(s, pos);
        if (pos < s.size() && s[pos] == ',') ++pos;
        skipWS(s, pos);
    }
    ++pos; // skip }
    return obj;
}

inline json json::parseArray(const std::string& s, size_t& pos) {
    json arr; arr.m_type = value_t::array;
    ++pos; // skip [
    skipWS(s, pos);
    while (pos < s.size() && s[pos] != ']') {
        arr.m_array.push_back(parseValue(s, pos));
        skipWS(s, pos);
        if (pos < s.size() && s[pos] == ',') ++pos;
        skipWS(s, pos);
    }
    ++pos; // skip ]
    return arr;
}

inline json json::parseNumber(const std::string& s, size_t& pos) {
    size_t start = pos;
    if (s[pos] == '-') ++pos;
    while (pos < s.size() && std::isdigit(s[pos])) ++pos;
    bool isFloat = false;
    if (pos < s.size() && s[pos] == '.') {
        isFloat = true; ++pos;
        while (pos < s.size() && std::isdigit(s[pos])) ++pos;
    }
    std::string numStr = s.substr(start, pos-start);
    if (isFloat) {
        json n; n.m_type = value_t::number_float;
        n.m_float = std::stof(numStr); return n;
    }
    json n; n.m_type = value_t::number_integer;
    n.m_int = std::stoi(numStr); return n;
}

inline json json::parseValue(const std::string& s, size_t& pos) {
    skipWS(s, pos);
    if (pos >= s.size()) return json();
    char c = s[pos];
    if (c == '"') { json j; j.m_type=value_t::string; j.m_string=parseString(s,pos); return j; }
    if (c == '{') return parseObject(s, pos);
    if (c == '[') return parseArray(s, pos);
    if (c == '-' || std::isdigit(c)) return parseNumber(s, pos);
    if (s.substr(pos,4)=="true")  { pos+=4; json j; j.m_type=value_t::boolean; j.m_bool=true;  return j; }
    if (s.substr(pos,5)=="false") { pos+=5; json j; j.m_type=value_t::boolean; j.m_bool=false; return j; }
    if (s.substr(pos,4)=="null")  { pos+=4; return json(); }
    return json();
}

inline json json::parse(const std::string& text) {
    size_t pos = 0;
    return parseValue(text, pos);
}

} // namespace nlohmann
