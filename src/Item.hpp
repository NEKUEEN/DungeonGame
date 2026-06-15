#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include "StatBonus.hpp"   // เพิ่ม

enum class ItemType
{ Food, Potion, Helmet, BodyArmor, Gloves, Greaves, Boots, Weapon, OffWeapon, Core, Material, };

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
    int bleedBonus  = 0;
    int poisonBonus = 0;
    int burnBonus   = 0;
    int resistBleed  = 0;
    int resistPoison = 0;
    int resistBurn   = 0;
    int resistStun   = 0;
    int resistSlow   = 0;
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
            .bleedBonus  = bleedBonus,
            .poisonBonus = poisonBonus,
            .burnBonus   = burnBonus,
        };
    }


    static Item makeFood();
    static Item makePotion();
    static Item makeWeapon();
    static Item makeOffWeapon();
    static Item makeArmor();

    sf::Color   color()       const;
    bool        isUsable()    const { return type==ItemType::Food||type==ItemType::Potion; }
    bool        isEquipment() const { return !isUsable()&&type!=ItemType::Material&&type!=ItemType::Core; }
    bool        isCore()      const { return type==ItemType::Core; }
    bool        isMaterial()  const { return type==ItemType::Material; }
    std::string spritePath()  const;
};
