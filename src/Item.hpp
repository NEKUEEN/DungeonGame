#pragma once
#include <SFML/Graphics.hpp>
#include <string>

enum class ItemType
{
    Food, Potion,
    Helmet, BodyArmor, Gloves, Greaves, Boots,
    Weapon, OffWeapon,
};

struct Item
{
    ItemType    type;
    std::string name;
    std::string desc;
    int         value   = 0;
    int         col     = -1;
    int         row     = -1;

    static Item makeFood();
    static Item makePotion();
    static Item makeWeapon();
    static Item makeOffWeapon();
    static Item makeArmor();

    sf::Color   color()       const;
    bool        isUsable()    const { return type==ItemType::Food||type==ItemType::Potion; }
    bool        isEquipment() const { return !isUsable(); }
    std::string spritePath()  const;  // path ของ texture
};
