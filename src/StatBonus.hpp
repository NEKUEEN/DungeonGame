// ==================== StatBonus.hpp (สมบูรณ์ เปลี่ยน magicDmg -> matk) ====================
#pragma once
#include <string>

struct StatBonus {
    int hp    = 0;
    int atk   = 0;
    int def   = 0;
    int dodge = 0;
    int mana  = 0;
    int matk  = 0;      // เปลี่ยนจาก magicDmg
    int magicRes = 0;
    int spd   = 0;
    // ── Status bonus ──
    int bleedBonus  = 0;
    int poisonBonus = 0;
    int burnBonus   = 0;
    int resistBleed  = 0;
    int resistPoison = 0;
    int resistBurn   = 0;
    int resistStun   = 0;
    int resistSlow   = 0;
    //resist
    int bleedDmgReduce  = 0;
    int bleedDurReduce  = 0;
    int poisonDmgReduce = 0;
    int poisonDurReduce = 0;
    int burnDmgReduce   = 0;
    int burnDurReduce   = 0;
    int stunDurReduce   = 0;
    int slowDurReduce   = 0;

    StatBonus& operator+=(const StatBonus& other) {
        hp    += other.hp;
        atk   += other.atk;
        def   += other.def;
        dodge += other.dodge;
        mana  += other.mana;
        matk  += other.matk;
        magicRes += other.magicRes;
        spd   += other.spd;
        bleedBonus  += other.bleedBonus;
        poisonBonus += other.poisonBonus;
        burnBonus   += other.burnBonus;
        resistBleed  += other.resistBleed;
        resistPoison += other.resistPoison;
        resistBurn   += other.resistBurn;
        resistStun   += other.resistStun;
        resistSlow   += other.resistSlow;
        bleedDmgReduce += other.bleedDmgReduce;
        bleedDurReduce += other.bleedDurReduce;
        poisonDmgReduce += other.poisonDmgReduce;
        poisonDurReduce += other.poisonDurReduce;
        burnDmgReduce += other.burnDmgReduce;
        burnDurReduce += other.burnDurReduce;
        stunDurReduce += other.stunDurReduce;
        slowDurReduce += other.slowDurReduce;
        return *this;
    }

    StatBonus operator+(const StatBonus& other) const {
        StatBonus res = *this;
        res += other;
        return res;
    }

    void clear() {
        hp = atk = def = dodge = mana = matk = magicRes = spd = bleedBonus = poisonBonus = burnBonus = 0;
    }

    std::string toString() const {
        return "HP+" + std::to_string(hp) + " ATK+" + std::to_string(atk) +
               " DEF+" + std::to_string(def) + " DODGE+" + std::to_string(dodge) +
               " MANA+" + std::to_string(mana) + " MATK+" + std::to_string(matk) +
               " MRES+" + std::to_string(magicRes) + " SPD+" + std::to_string(spd);
    }
};