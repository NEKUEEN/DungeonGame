#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include "lib/json.hpp"
#include "DropTable.hpp"

// ============================================================
//  MonsterData  –  ข้อมูลมอนแต่ละตัวจาก JSON
// ============================================================
struct MonsterData
{
    std::string id;
    std::string name;
    std::string family;   // "Goblin", "Orc", "Rat" ฯลฯ
    std::string rank;     // "Normal", "Elite", "Boss"
    std::string sprite;   // ชื่อไฟล์รูป เช่น "goblin_warrior.png"
    int hp       = 10;
    int attack   = 3;
    int defense  = 0;
    int exp      = 1;
};

// ============================================================
//  MonsterDB  –  Singleton โหลดและเก็บ monster data ทั้งหมด
// ============================================================
class MonsterDB
{
public:
    static MonsterDB& instance()
    { static MonsterDB db; return db; }

    // โหลดจาก JSON file
    bool load(const std::string& path)
    {
        std::ifstream f(path);
        if (!f.is_open())
        { std::cerr<<"[MonsterDB] Cannot open: "<<path<<"\n"; return false; }

        try
        {
            auto j = nlohmann::json::parse(f);
            for (const auto& m : j["monsters"])
            {
                MonsterData d;
                d.id      = m["id"].get<std::string>();
                d.name    = m["name"].get<std::string>();
                d.family  = m["family"].get<std::string>();
                d.rank    = m["rank"].get<std::string>();
                d.sprite  = m["sprite"].get<std::string>();
                d.hp      = m["hp"].get<int>();
                d.attack  = m["attack"].get<int>();
                d.defense = m["defense"].get<int>();
                d.exp     = m["exp"].get<int>();
                m_monsters[d.id] = d;
                m_byFamily[d.family].push_back(d.id);
                m_byRank[d.rank].push_back(d.id);

                // register drop table
                if (m.contains("drops"))
                {
                    std::vector<DropEntry> drops;
                    for (const auto& dr : m["drops"])
                    {
                        DropEntry e;
                        e.itemId = dr["item"].get<std::string>();
                        e.chance = dr["chance"].get<float>();
                        drops.push_back(e);
                    }
                    DropTable::instance().registerDrops(d.id, drops);
                }
            }
            std::cout<<"[MonsterDB] Loaded "<<m_monsters.size()<<" monsters\n";
            return true;
        }
        catch (const std::exception& e)
        { std::cerr<<"[MonsterDB] Parse error: "<<e.what()<<"\n"; return false; }
    }

    // ดึงข้อมูลมอนตาม id
    const MonsterData* get(const std::string& id) const
    {
        auto it = m_monsters.find(id);
        return it == m_monsters.end() ? nullptr : &it->second;
    }

    // ดึง id ทั้งหมดของ family นั้น
    std::vector<std::string> getFamily(const std::string& family) const
    {
        auto it = m_byFamily.find(family);
        return it == m_byFamily.end() ? std::vector<std::string>{} : it->second;
    }

    // ดึง id ทั้งหมดของ rank นั้น
    std::vector<std::string> getByRank(const std::string& rank) const
    {
        auto it = m_byRank.find(rank);
        return it == m_byRank.end() ? std::vector<std::string>{} : it->second;
    }

    // ดึง id ทั้งหมด
    std::vector<std::string> getAllIds() const
    {
        std::vector<std::string> ids;
        for (auto& [id, _] : m_monsters) ids.push_back(id);
        return ids;
    }

    int count() const { return (int)m_monsters.size(); }

private:
    MonsterDB() = default;
    std::map<std::string, MonsterData>              m_monsters;
    std::map<std::string, std::vector<std::string>> m_byFamily;
    std::map<std::string, std::vector<std::string>> m_byRank;
};
