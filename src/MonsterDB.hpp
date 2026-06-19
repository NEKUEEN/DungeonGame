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
    std::string sprite;
    int hp       = 10;
    int attack   = 3;
    int defense  = 0;
    int exp      = 1;
    int spd      = 0;   // ความเร็ว: บวก=เร็ว, ลบ=ช้า (boss/ตัวใหญ่)

    // ── Weapon damage-type resist (%) ──
    // ค่าบวก = ต้านทาน (ลดดาเมจที่ได้รับ), ค่าลบ = แพ้ทาง (เพิ่มดาเมจที่ได้รับ)
    int slashResist  = 0;
    int pierceResist = 0;
    int bluntResist  = 0;
    int cleaveResist = 0;

    // ── AI ──
    // "melee" (default) | "ranged" | "caster" | "coward"
    std::string aiType        = "melee";
    int         preferredRange = 1;  // Ranged/Caster รักษาระยะนี้
    int         alertRange     = 8;  // ระยะที่เริ่ม alert (เห็น player)
    int attackInterval = 1;  // ระยะเวลาระหว่างการโจมตี (หน่วยเป็นวินาที) สำหรับมอนที่มีสกิล เช่น caster ที่ไม่โจมตีทุกเทิร์น

    // ── Skills ──
    std::vector<std::string> skills;  // skill ids จาก skills.json
    // ── Status on hit ──
    std::string applyStatus    = "";
    int         statusPower    = 0;
    int         statusDuration = 0;
};

// ============================================================
//  MonsterDB  –  Singleton โหลดและเก็บ monster data ทั้งหมด
// ============================================================
class MonsterDB
{
public:
    static MonsterDB& instance()
    { static MonsterDB db; return db; }

    bool load(const std::string& path)
    {
        std::ifstream f(path);
        if (!f.is_open())
        { std::cerr << "[MonsterDB] Cannot open: " << path << "\n"; return false; }

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

                // ── AI fields (optional — backward-compat) ──
                d.spd            = m.contains("spd")             ? m["spd"].get<int>()              : 0;
                d.aiType         = m.contains("ai_type")         ? m["ai_type"].get<std::string>()  : "melee";
                d.preferredRange = m.contains("preferred_range") ? m["preferred_range"].get<int>()  : 1;
                d.alertRange     = m.contains("alert_range")     ? m["alert_range"].get<int>()      : 8;
                d.attackInterval = m.contains("attack_interval") ? m["attack_interval"].get<int>() : 1;
                d.applyStatus    = m.contains("apply_status")    ? m["apply_status"].get<std::string>() : "";
                d.statusPower    = m.contains("status_power")    ? m["status_power"].get<int>()         : 0;
                d.statusDuration = m.contains("status_duration") ? m["status_duration"].get<int>()      : 0;

                // ── Weapon damage-type resist (optional) ──
                d.slashResist  = m.contains("slash_resist")  ? m["slash_resist"].get<int>()  : 0;
                d.pierceResist = m.contains("pierce_resist") ? m["pierce_resist"].get<int>() : 0;
                d.bluntResist  = m.contains("blunt_resist")  ? m["blunt_resist"].get<int>()  : 0;
                d.cleaveResist = m.contains("cleave_resist") ? m["cleave_resist"].get<int>() : 0;

                // ── Skills (optional) ──
                if (m.contains("skills"))
                    for (const auto& sk : m["skills"])
                        d.skills.push_back(sk.get<std::string>());

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
            std::cout << "[MonsterDB] Loaded " << m_monsters.size() << " monsters\n";
            return true;
        }
        catch (const std::exception& e)
        { std::cerr << "[MonsterDB] Parse error: " << e.what() << "\n"; return false; }
    }

    const MonsterData* get(const std::string& id) const
    {
        auto it = m_monsters.find(id);
        return it == m_monsters.end() ? nullptr : &it->second;
    }

    std::vector<std::string> getFamily(const std::string& family) const
    {
        auto it = m_byFamily.find(family);
        return it == m_byFamily.end() ? std::vector<std::string>{} : it->second;
    }

    std::vector<std::string> getByRank(const std::string& rank) const
    {
        auto it = m_byRank.find(rank);
        return it == m_byRank.end() ? std::vector<std::string>{} : it->second;
    }

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
