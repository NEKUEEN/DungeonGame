#pragma once
#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include "lib/json.hpp"

struct StatusEffectData
{
    std::string id;
    std::string name;
    std::string icon;
    int r = 255, g = 255, b = 255;
};

class StatusEffectDB
{
public:
    static StatusEffectDB& instance()
    { static StatusEffectDB db; return db; }

    bool load(const std::string& path)
    {
        std::ifstream f(path);
        if (!f.is_open())
        { std::cerr << "[StatusEffectDB] Cannot open: " << path << "\n"; return false; }

        auto j = nlohmann::json::parse(f);
        for (const auto& e : j["effects"])
        {
            StatusEffectData d;
            d.id   = e["id"].get<std::string>();
            d.name = e["name"].get<std::string>();
            d.icon = e["icon"].get<std::string>();
            auto c = e["color"];
            d.r = c[0].get<int>();
            d.g = c[1].get<int>();
            d.b = c[2].get<int>();
            m_data[d.id] = d;
        }
        std::cout << "[StatusEffectDB] Loaded " << m_data.size() << " effects\n";
        return true;
    }

    const StatusEffectData* get(const std::string& id) const
    {
        auto it = m_data.find(id);
        return it == m_data.end() ? nullptr : &it->second;
    }

private:
    StatusEffectDB() = default;
    std::map<std::string, StatusEffectData> m_data;
};