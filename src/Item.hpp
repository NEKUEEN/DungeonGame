#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include "StatBonus.hpp"   // เพิ่ม

enum class ItemType
{ Food, Potion, Helmet, BodyArmor, Gloves, Greaves, Boots, Weapon, OffWeapon, Core, Material, Ammo, };

// ── Weapon damage types: ฟัน(Slash) แทง(Pierce) ทุบ(Blunt) ฟาด(Cleave) ──
enum class DamageType { Slash, Pierce, Blunt, Cleave };

inline DamageType damageTypeFromString(const std::string& s)
{
    if (s == "pierce") return DamageType::Pierce;
    if (s == "blunt")  return DamageType::Blunt;
    if (s == "cleave") return DamageType::Cleave;
    return DamageType::Slash; // default
}

inline std::string damageTypeToString(DamageType t)
{
    switch (t)
    {
        case DamageType::Pierce: return "pierce";
        case DamageType::Blunt:  return "blunt";
        case DamageType::Cleave: return "cleave";
        default:                 return "slash";
    }
}

struct Item
{
    std::string id;          // ← เพิ่ม: ใช้ lookup core_stats จาก DropTable
    ItemType    type;
    std::string name;
    std::string desc;
    std::string spriteName;
    int         value      = 0;
    int         hpBonus    = 0;
    int         atkBonus   = 0;
    int         defBonus   = 0;
    int         dodgeBonus = 0;
    int         manaBonus  = 0;  // เพิ่ม: สำหรับ Core ที่มี bonus mana
    int         magicDmgBonus = 0; // เพิ่ม: สำหรับ Core ที่มี bonus magic damage
    int         magicResBonus = 0;
    int         spdBonus   = 0;  // +/- speed (บวก=เร็ว, ลบ=ช้า) // เพิ่ม: สำหรับ Core ที่มี bonus magic resistance
    DamageType  damageType = DamageType::Slash;  // ← เพิ่ม: ประเภทดาเมจของอาวุธ (ฟัน/แทง/ทุบ/ฟาด)
    int slashDmgBonus  = 0;
    int pierceDmgBonus = 0;
    int bluntDmgBonus  = 0;
    int cleaveDmgBonus = 0;
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
    std::string onHitStatus;
    int         onHitPower    = 0;
    int         onHitDuration = 0;
    int         onHitChance   = 0;
    int baseAtkAut = 100;  // Aut พื้นฐานของอาวุธ (70=dagger, 100=sword, 140=axe)
    bool        stackable  = false;
    int         col        = -1;
    int         row        = -1;

    StatBonus toStatBonus() const {
        return {
            .hp    = hpBonus,
            .atk   = atkBonus,
            .def   = defBonus,
            .dodge = dodgeBonus,
            .mana  = manaBonus,
            .matk  = magicDmgBonus,   // เปลี่ยน
            .magicRes = magicResBonus,
            .spd   = spdBonus,
            .slashDmgBonus  = slashDmgBonus,
            .pierceDmgBonus = pierceDmgBonus,
            .bluntDmgBonus  = bluntDmgBonus,
            .cleaveDmgBonus = cleaveDmgBonus,
            .bleedBonus  = bleedBonus,
            .poisonBonus = poisonBonus,
            .burnBonus   = burnBonus,
            .bleedDurReduce = bleedDurReduce
        };
    }


    static Item makeFood();
    static Item makePotion();
    static Item makeWeapon();
    static Item makeOffWeapon();
    static Item makeArmor();
    static Item makeAmmo();

    sf::Color   color()       const;
    bool        isUsable()    const { return type==ItemType::Food||type==ItemType::Potion; }
    bool        isEquipment() const { return !isUsable()&&type!=ItemType::Material&&type!=ItemType::Core; }
    bool        isCore()      const { return type==ItemType::Core; }
    bool        isMaterial()  const { return type==ItemType::Material; }
    bool        isAmmo() const { return type == ItemType::Ammo; }
    std::string spritePath()  const;
};
