#pragma once
#include <SFML/Graphics.hpp>
#include <string>

enum class ItemType
{
    Food, Potion,
    Helmet, BodyArmor, Gloves, Greaves, Boots,
    Weapon, OffWeapon,
    Core,       // แกนมอน — ใส่ Core Slots ได้
    Material,   // หินเวทย์ ฯลฯ — ไว้ขาย ไม่ equip
};

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
    int         col        = -1;
    int         row        = -1;
    bool        stackable  = false;

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
