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
    int  manaBonus  = 0;
    int  magicDmgBonus = 0;
    int  magicResBonus = 0;
    int  spdBonus   = 0;
    std::string damageType = "slash";  // ← เพิ่ม: "slash" | "pierce" | "blunt" | "cleave" (สำหรับ weapon)
    // ── Weapon damage-type bonus (ใส่ตรงๆ บนตัว item เอง เช่น weapon/armor พิเศษ) ──
    int slashDmgBonus  = 0;
    int pierceDmgBonus = 0;
    int bluntDmgBonus  = 0;
    int cleaveDmgBonus = 0;
    int bleedBonus = 0;
    int poisonBonus = 0;
    int burnBonus = 0;
    int resistBleed  = 0;
    int resistPoison = 0;
    int resistBurn   = 0;
    int resistStun   = 0;
    int resistSlow   = 0;
    int bleedDmgReduce  = 0;
    int bleedDurReduce  = 0;
    int poisonDmgReduce = 0;
    int poisonDurReduce = 0;
    int burnDmgReduce   = 0;
    int burnDurReduce   = 0;
    int stunDurReduce   = 0;
    int slowDurReduce   = 0;
    int baseAtkAut = 100;
    bool stackable  = true;

    // core_stats (เฉพาะ Core items)
    int coreHp    = 0;
    int coreAtk   = 0;
    int coreDef   = 0;
    int coreDodge = 0;
    int coreMana  = 0;
    int coreMagicDmg = 0;
    int coreMagicRes = 0;
    int coreSpdBonus = 0;
    int coreSlashDmg  = 0;
    int corePierceDmg = 0;
    int coreBluntDmg  = 0;
    int coreCleaveDmg = 0;
    // core status bonus
    int coreBleedBonus  = 0;
    int corePoisonBonus = 0;
    int coreBurnBonus   = 0;
    int coreResistBleed  = 0;
    int coreResistPoison = 0;
    int coreResistBurn   = 0;
    int coreResistStun   = 0;
    int coreResistSlow   = 0;
    // core reduce
    int coreBleedDmgReduce  = 0;
    int coreBleedDurReduce  = 0;
    int corePoisonDmgReduce = 0;
    int corePoisonDurReduce = 0;
    int coreBurnDmgReduce   = 0;
    int coreBurnDurReduce   = 0;
    int coreStunDurReduce   = 0;
    int coreSlowDurReduce   = 0;

    // skills ที่ core นี้ให้เมื่อ equip
    std::vector<std::string> coreSkills;

    // on-hit status effect (สำหรับ weapon)
    std::string onHitStatus;   // "bleed", "poison", "stun" ฯลฯ
    int         onHitPower    = 0;
    int         onHitDuration = 0;
    int         onHitChance   = 0;  // 0-100 (%)
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
            d.manaBonus  = it.contains("mana")  ? it["mana"].get<int>()  : 0;
            d.magicDmgBonus  = it.contains("magic_dmg")  ? it["magic_dmg"].get<int>()  : 0;
            d.magicResBonus  = it.contains("magic_res")  ? it["magic_res"].get<int>()  : 0;
            d.spdBonus    = it.contains("spd")   ? it["spd"].get<int>()   : 0;
            d.damageType  = it.contains("damage_type") ? it["damage_type"].get<std::string>() : "slash";
            d.slashDmgBonus  = it.contains("slash_dmg")  ? it["slash_dmg"].get<int>()  : 0;
            d.pierceDmgBonus = it.contains("pierce_dmg") ? it["pierce_dmg"].get<int>() : 0;
            d.bluntDmgBonus  = it.contains("blunt_dmg")  ? it["blunt_dmg"].get<int>()  : 0;
            d.cleaveDmgBonus = it.contains("cleave_dmg") ? it["cleave_dmg"].get<int>() : 0;
            d.bleedBonus  = it.contains("bleed_bonus")  ? it["bleed_bonus"].get<int>()  : 0;
            d.poisonBonus = it.contains("poison_bonus") ? it["poison_bonus"].get<int>() : 0;
            d.burnBonus   = it.contains("burn_bonus")   ? it["burn_bonus"].get<int>()   : 0;
            d.resistBleed  = it.contains("resist_bleed")  ? it["resist_bleed"].get<int>()  : 0;
            d.resistPoison = it.contains("resist_poison") ? it["resist_poison"].get<int>() : 0;
            d.resistBurn   = it.contains("resist_burn")   ? it["resist_burn"].get<int>()   : 0;
            d.resistStun   = it.contains("resist_stun")   ? it["resist_stun"].get<int>()   : 0;
            d.resistSlow   = it.contains("resist_slow")   ? it["resist_slow"].get<int>()   : 0;
            //resist
            d.bleedDmgReduce  = it.contains("bleed_dmg_reduce")  ? it["bleed_dmg_reduce"].get<int>()  : 0;
            d.bleedDurReduce = it.contains("bleed_dur_reduce") ? it["bleed_dur_reduce"].get<int>() : 0;
            d.poisonDmgReduce   = it.contains("poison_dmg_reduce")   ? it["poison_dmg_reduce"].get<int>()   : 0;
            d.poisonDurReduce  = it.contains("poison_dur_reduce")  ? it["poison_dur_reduce"].get<int>()  : 0;
            d.burnDmgReduce = it.contains("burn_dmg_reduce") ? it["burn_dmg_reduce"].get<int>() : 0;
            d.burnDurReduce   = it.contains("burn_dur_reduce")   ? it["burn_dur_reduce"].get<int>()   : 0;
            d.stunDurReduce   = it.contains("stun_dur_reduce")   ? it["stun_dur_reduce"].get<int>()   : 0;
            d.slowDurReduce   = it.contains("slow_dur_reduce")   ? it["slow_dur_reduce"].get<int>()   : 0;
            d.onHitStatus   = it.contains("on_hit_status")   ? it["on_hit_status"].get<std::string>() : "";
            d.onHitPower    = it.contains("on_hit_power")     ? it["on_hit_power"].get<int>()    : 0;
            d.onHitDuration = it.contains("on_hit_duration")  ? it["on_hit_duration"].get<int>() : 0;
            d.onHitChance   = it.contains("on_hit_chance")    ? it["on_hit_chance"].get<int>()   : 0;
            d.baseAtkAut = it.contains("base_atk_aut")  ? it["base_atk_aut"].get<int>()  : 100;
            // core_stats
            if (it.contains("core_stats"))
            {
                const auto& cs = it["core_stats"];
                d.coreHp    = cs.contains("hp")    ? cs["hp"].get<int>()    : 0;
                d.coreAtk   = cs.contains("atk")   ? cs["atk"].get<int>()   : 0;
                d.coreDef   = cs.contains("def")   ? cs["def"].get<int>()   : 0;
                d.coreDodge = cs.contains("dodge") ? cs["dodge"].get<int>() : 0;
                d.coreMana  = cs.contains("mana")  ? cs["mana"].get<int>()  : 0;
                d.coreMagicDmg  = cs.contains("magic_dmg")  ? cs["magic_dmg"].get<int>()  : 0;
                d.coreMagicRes  = cs.contains("magic_res")  ? cs["magic_res"].get<int>()  : 0;
                d.coreSpdBonus = cs.contains("spd")   ? cs["spd"].get<int>()   : 0;
                d.coreSlashDmg  = cs.contains("slash_dmg")  ? cs["slash_dmg"].get<int>()  : 0;
                d.corePierceDmg = cs.contains("pierce_dmg") ? cs["pierce_dmg"].get<int>() : 0;
                d.coreBluntDmg  = cs.contains("blunt_dmg")  ? cs["blunt_dmg"].get<int>()  : 0;
                d.coreCleaveDmg = cs.contains("cleave_dmg") ? cs["cleave_dmg"].get<int>() : 0;
                d.coreBleedBonus  = cs.contains("bleed_bonus")  ? cs["bleed_bonus"].get<int>()  : 0;
                d.corePoisonBonus = cs.contains("poison_bonus") ? cs["poison_bonus"].get<int>() : 0;
                d.coreBurnBonus   = cs.contains("burn_bonus")   ? cs["burn_bonus"].get<int>()   : 0;
                d.coreResistBleed  = cs.contains("resist_bleed")  ? cs["resist_bleed"].get<int>()  : 0;
                d.coreResistPoison = cs.contains("resist_poison") ? cs["resist_poison"].get<int>() : 0;
                d.coreResistBurn   = cs.contains("resist_burn")   ? cs["resist_burn"].get<int>()   : 0;
                d.coreResistStun   = cs.contains("resist_stun")   ? cs["resist_stun"].get<int>()   : 0;
                d.coreResistSlow   = cs.contains("resist_slow")   ? cs["resist_slow"].get<int>()   : 0;
                d.coreBleedDmgReduce  = cs.contains("bleed_dmg_reduce")  ? cs["bleed_dmg_reduce"].get<int>()  : 0;
                d.coreBleedDurReduce  = cs.contains("bleed_dur_reduce")  ? cs["bleed_dur_reduce"].get<int>()  : 0;
                d.corePoisonDmgReduce = cs.contains("poison_dmg_reduce") ? cs["poison_dmg_reduce"].get<int>() : 0;
                d.corePoisonDurReduce = cs.contains("poison_dur_reduce") ? cs["poison_dur_reduce"].get<int>() : 0;
                d.coreBurnDmgReduce   = cs.contains("burn_dmg_reduce")   ? cs["burn_dmg_reduce"].get<int>()   : 0;
                d.coreBurnDurReduce   = cs.contains("burn_dur_reduce")   ? cs["burn_dur_reduce"].get<int>()   : 0;
                d.coreStunDurReduce   = cs.contains("stun_dur_reduce")   ? cs["stun_dur_reduce"].get<int>()   : 0;
                d.coreSlowDurReduce   = cs.contains("slow_dur_reduce")   ? cs["slow_dur_reduce"].get<int>()   : 0;
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
