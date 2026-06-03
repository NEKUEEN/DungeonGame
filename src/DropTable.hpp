#pragma once
#include <string>
#include <vector>
#include <random>
#include <iostream>
#include <fstream>
#include <map>
#include <optional>
#include "lib/json.hpp"

// ============================================================
//  DropEntry  –  item id + โอกาส drop (0.0 - 1.0)
// ============================================================
struct DropEntry
{
    std::string itemId;
    float       chance = 0.f;
};

// ============================================================
//  ItemData  –  ข้อมูล item จาก items.json
// ============================================================
struct ItemData
{
    std::string id;
    std::string name;
    std::string type;
    std::string desc;
    std::string sprite;
    int  value     = 1;
    bool stackable = true;
};

// ============================================================
//  DropTable  –  Singleton
//  โหลด items.json + drop data จาก monsters.json
// ============================================================
class DropTable
{
public:
    static DropTable& instance()
    { static DropTable dt; return dt; }

    // โหลด items.json
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
            d.value     = it["value"].get<int>();
            d.stackable = it["stackable"].get<bool>();
            m_items[d.id] = d;
            m_itemsByType[d.type].push_back(d.id);
        }
        std::cout<<"[DropTable] Loaded "<<m_items.size()<<" items\n";
        return true;
    }

    // register drop table ของมอนตัวหนึ่ง
    void registerDrops(const std::string& monsterId,
                       const std::vector<DropEntry>& drops)
    {
        m_drops[monsterId] = drops;
    }

    // สุ่ม drop — return list ของ item ids ที่ drop
    std::vector<std::string> roll(const std::string& monsterId)
    {
        std::vector<std::string> result;
        auto it = m_drops.find(monsterId);
        if (it == m_drops.end()) return result;

        for (const auto& entry : it->second)
        {
            float r = m_dist(m_rng);
            if (r <= entry.chance)
                result.push_back(entry.itemId);
        }
        return result;
    }

    // ดึงข้อมูล item
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

    mutable std::mt19937                      m_rng;
    std::uniform_real_distribution<float>     m_dist;
    std::map<std::string, std::vector<DropEntry>> m_drops;
    std::map<std::string, ItemData>               m_items;
    std::map<std::string, std::vector<std::string>> m_itemsByType;
};
