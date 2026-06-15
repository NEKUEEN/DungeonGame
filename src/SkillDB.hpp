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
            d.hotbarSlot = s.contains("hotbar_slot") ? s["hotbar_slot"].get<int>() : -1;
            d.icon = s.contains("icon") ? s["icon"].get<std::string>() : "";

            // type
            std::string t = s["type"].get<std::string>();
            if      (t == "active_buff")   d.type = SkillType::ActiveBuff;
            else if (t == "active_ranged") d.type = SkillType::ActiveRanged;
            else if (t == "active_heal")   d.type = SkillType::ActiveHeal;
            else if (t == "active_warp")   d.type = SkillType::ActiveWarp;
            else if (t == "active_aoe")    d.type = SkillType::ActiveAoe;
            else                           d.type = SkillType::Passive;

            // effect — รองรับทั้ง format ใหม่ (effect object) และเก่า (flat fields)
            if (s.contains("effect"))
            {
                const auto& e = s["effect"];
                if (e.contains("atk_pct"))      d.effect.atkPct      = e["atk_pct"].get<int>();
                if (e.contains("matk_pct"))     d.effect.matkPct     = e["matk_pct"].get<int>();
                if (e.contains("hp_pct"))       d.effect.hpPct       = e["hp_pct"].get<int>();
                if (e.contains("def_pct"))       d.effect.defPct      = e["def_pct"].get<int>();
                if (e.contains("dodge_pct"))     d.effect.dodgePct    = e["dodge_pct"].get<int>();
                if (e.contains("heal_flat"))     d.effect.healFlat    = e["heal_flat"].get<int>();
                if (e.contains("heal_pct"))      d.effect.healPct     = e["heal_pct"].get<int>();
                if (e.contains("damage_pct"))    d.effect.damagePct   = e["damage_pct"].get<int>();
                if (e.contains("range"))         d.effect.range       = e["range"].get<int>();
                if (e.contains("speed_bonus"))   d.effect.speedBonus  = e["speed_bonus"].get<int>();
                if (e.contains("atk_speed_pct")) d.effect.atkSpeedPct = e["atk_speed_pct"].get<int>();
                if (e.contains("aoe_radius"))    d.effect.aoeRadius   = e["aoe_radius"].get<int>();
                if (e.contains("scaling_stat")) d.effect.scalingStat = e["scaling_stat"].get<std::string>();
                if (e.contains("mana_cost"))      d.effect.manaCost    = e["mana_cost"].get<int>();
                if (e.contains("stamina_cost"))   d.effect.staminaCost = e["stamina_cost"].get<int>();
                if (e.contains("apply_status"))     d.effect.applyStatus    = e["apply_status"].get<std::string>();
                if (e.contains("status_power"))     d.effect.statusPower    = e["status_power"].get<int>();
                if (e.contains("status_duration"))  d.effect.statusDuration = e["status_duration"].get<int>();

            }
            else
            {
                // backward-compat: flat fields (skills.json เก่า)
                if (s.contains("atk_pct"))   d.effect.atkPct    = s["atk_pct"].get<int>();
                if (s.contains("matk_pct"))  d.effect.matkPct   = s["matk_pct"].get<int>();
                if (s.contains("hp_pct"))    d.effect.hpPct     = s["hp_pct"].get<int>();
                if (s.contains("def_pct"))   d.effect.defPct    = s["def_pct"].get<int>();
                if (s.contains("dodge_pct")) d.effect.dodgePct  = s["dodge_pct"].get<int>();
                if (s.contains("damage"))    d.effect.damagePct = s["damage"].get<int>();
                if (s.contains("range"))     d.effect.range     = s["range"].get<int>();
            }

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
