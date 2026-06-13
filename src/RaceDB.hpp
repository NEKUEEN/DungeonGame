#pragma once
#include <string>
#include <vector>
#include <map>
#include "lib/json.hpp"
#include <fstream>

struct RacePassive {
    std::string id;
    std::string desc;
};

struct RaceStatBonus {
    int hp=0, atk=0, def=0;
    int mana=0, matk=0, mdef=0;
    int dodge=0, spd=0;
};

struct RaceData {
    std::string id;
    std::string name;
    std::string sprite;
    RaceStatBonus statBonus;
    RacePassive passive;
    std::string startSkill;
};

class RaceDB {
public:
    static RaceDB& instance() { static RaceDB db; return db; }

    void load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return;
        auto j = nlohmann::json::parse(f);
        for (auto& r : j["races"]) {
            RaceData d;
            d.id         = r["id"].get<std::string>();
            d.name       = r["name"].get<std::string>();
            d.sprite     = r["sprite"].get<std::string>();
            auto& sb     = r["statBonus"];
            d.statBonus.hp    = sb["hp"].get<int>();
            d.statBonus.atk   = sb["atk"].get<int>();
            d.statBonus.def   = sb["def"].get<int>();
            d.statBonus.mana  = sb["mana"].get<int>();
            d.statBonus.matk  = sb["matk"].get<int>();
            d.statBonus.mdef  = sb["mdef"].get<int>();
            d.statBonus.dodge = sb["dodge"].get<int>();
            d.statBonus.spd   = sb["spd"].get<int>();
            d.passive.id   = r["passive"]["id"].get<std::string>();
            d.passive.desc = r["passive"]["desc"].get<std::string>();
            d.startSkill   = r["startSkill"].get<std::string>();
            m_races.push_back(d);
            m_map[d.id] = (int)m_races.size()-1;
        }
    }

    const std::vector<RaceData>& getAll() const { return m_races; }
    const RaceData* get(const std::string& id) const {
        auto it = m_map.find(id);
        if (it == m_map.end()) return nullptr;
        return &m_races[it->second];
    }

private:
    std::vector<RaceData> m_races;
    std::map<std::string, int> m_map;
};