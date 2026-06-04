#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include "Skill.hpp"
#include "lib/json.hpp"

// ============================================================
//  SkillDB  -  Singleton โหลด skills.json
// ============================================================
class SkillDB
{
public:
    static SkillDB& instance()
    { static SkillDB db; return db; }

    bool load(const std::string& path)
    {
        std::ifstream f(path);
        if (!f.is_open())
        { std::cerr<<"[SkillDB] Cannot open: "<<path<<"\n"; return false; }

        auto j = nlohmann::json::parse(f);
        for (const auto& s : j["skills"])
        {
            SkillData d;
            d.id       = s["id"].get<std::string>();
            d.name     = s["name"].get<std::string>();
            d.desc     = s["desc"].get<std::string>();
            d.cooldown = s["cooldown"].get<int>();
            d.duration = s["duration"].get<int>();
            d.atkPct   = s["atk_pct"].get<int>();
            d.defPct   = s["def_pct"].get<int>();
            d.dodgePct = s["dodge_pct"].get<int>();
            d.damage   = s["damage"].get<int>();
            d.range    = s["range"].get<int>();

            std::string t = s["type"].get<std::string>();
            if      (t == "active_buff")   d.type = SkillType::ActiveBuff;
            else if (t == "active_ranged") d.type = SkillType::ActiveRanged;
            else                           d.type = SkillType::Passive;

            m_skills[d.id] = d;
        }
        std::cout<<"[SkillDB] Loaded "<<m_skills.size()<<" skills\n";
        return true;
    }

    const SkillData* get(const std::string& id) const
    {
        auto it = m_skills.find(id);
        return it == m_skills.end() ? nullptr : &it->second;
    }

    // return SkillData ทั้งหมด (สำหรับ init player skills)
    std::vector<SkillData> getAll() const
    {
        std::vector<SkillData> out;
        for (auto& [id, d] : m_skills) out.push_back(d);
        return out;
    }

private:
    SkillDB() = default;
    std::map<std::string, SkillData> m_skills;
};
