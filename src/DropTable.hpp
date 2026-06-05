#pragma once
#include <string>
#include <vector>
#include <random>
#include <iostream>
#include <fstream>
#include <map>
#include <optional>
#include "lib/json.hpp"

struct DropEntry
{
    std::string itemId;
    float       chance = 0.f;
};

struct ItemData
{
    std::string id;
    std::string name;
    std::string type;
    std::string desc;
    std::string sprite;
    int  value      = 1;
    int  hpBonus    = 0;
    int  atkBonus   = 0;
    int  defBonus   = 0;
    int  dodgeBonus = 0;
    bool stackable  = true;

    // core_stats (เฉพาะ Core items)
    int coreHp    = 0;
    int coreAtk   = 0;
    int coreDef   = 0;
    int coreDodge = 0;

    // skills ที่ core นี้ให้เมื่อ equip
    std::vector<std::string> coreSkills;
};

class DropTable
{
public:
    static DropTable& instance()
    { static DropTable dt; return dt; }

    bool loadItems(const std::string& path)
    {
        std::ifstream f(path);
        if (!f.is_open())
        { std::cerr<<"[DropTable] Cannot open: "<<path<<"\n"; return false; }

        auto j = nlohmann::json::parse(f);
        for (const auto& it : j["items"])
        {
            ItemData d;
            d.id        = it["id"].get<std::string>();
            d.name      = it["name"].get<std::string>();
            d.type      = it["type"].get<std::string>();
            d.desc      = it["desc"].get<std::string>();
            d.sprite    = it["sprite"].get<std::string>();
            d.value      = it["value"].get<int>();
            d.stackable  = it["stackable"].get<bool>();
            d.hpBonus    = it.contains("hp")    ? it["hp"].get<int>()    : 0;
            d.atkBonus   = it.contains("atk")   ? it["atk"].get<int>()   : 0;
            d.defBonus   = it.contains("def")   ? it["def"].get<int>()   : 0;
            d.dodgeBonus = it.contains("dodge") ? it["dodge"].get<int>() : 0;

            // core_stats
            if (it.contains("core_stats"))
            {
                const auto& cs = it["core_stats"];
                d.coreHp    = cs.contains("hp")    ? cs["hp"].get<int>()    : 0;
                d.coreAtk   = cs.contains("atk")   ? cs["atk"].get<int>()   : 0;
                d.coreDef   = cs.contains("def")   ? cs["def"].get<int>()   : 0;
                d.coreDodge = cs.contains("dodge") ? cs["dodge"].get<int>() : 0;
            }

            // skills ที่ core ให้
            if (it.contains("skills"))
                for (const auto& sk : it["skills"])
                    d.coreSkills.push_back(sk.get<std::string>());

            m_items[d.id] = d;
            m_itemsByType[d.type].push_back(d.id);
        }
        std::cout<<"[DropTable] Loaded "<<m_items.size()<<" items\n";
        return true;
    }

    void registerDrops(const std::string& monsterId,
                       const std::vector<DropEntry>& drops)
    {
        m_drops[monsterId] = drops;
    }

    std::vector<std::string> roll(const std::string& monsterId)
    {
        std::vector<std::string> result;
        auto it = m_drops.find(monsterId);
        if (it == m_drops.end()) return result;

        for (const auto& entry : it->second)
        {
            float r = m_dist(m_rng);
            if (r < entry.chance)
            {
                if (m_items.find(entry.itemId) == m_items.end())
                    std::cerr << "[DropTable] Unknown drop item id '" << entry.itemId
                              << "' for monster '" << monsterId << "'\n";
                else
                    result.push_back(entry.itemId);
            }
        }
        return result;
    }

    const ItemData* getItem(const std::string& id) const
    {
        auto it = m_items.find(id);
        return it == m_items.end() ? nullptr : &it->second;
    }

    std::optional<std::string> getRandomItemIdByType(const std::string& type) const
    {
        auto it = m_itemsByType.find(type);
        if (it == m_itemsByType.end() || it->second.empty())
            return std::nullopt;
        std::uniform_int_distribution<int> dist(0, (int)it->second.size() - 1);
        return it->second[dist(m_rng)];
    }

private:
    DropTable()
        : m_rng(std::random_device{}())
        , m_dist(0.f, 1.f)
    {}

    mutable std::mt19937                          m_rng;
    std::uniform_real_distribution<float>         m_dist;
    std::map<std::string, std::vector<DropEntry>> m_drops;
    std::map<std::string, ItemData>               m_items;
    std::map<std::string, std::vector<std::string>> m_itemsByType;
};
