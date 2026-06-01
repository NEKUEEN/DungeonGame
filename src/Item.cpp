#include "Item.hpp"
#include <cstdlib>

Item Item::makeFood()
{
    static const char* names[]={"Bread","Meat","Apple","Mushroom","Cheese"};
    Item i; i.type=ItemType::Food;
    i.name=names[std::rand()%5]; i.desc="Fills your stomach.";
    i.value=30+std::rand()%30; return i;
}

Item Item::makePotion()
{
    Item i; i.type=ItemType::Potion;
    i.name="Health Potion"; i.desc="Restores HP.";
    i.value=8+std::rand()%8; return i;
}

Item Item::makeWeapon()
{
    static const char* names[]={"Dagger","Sword","Axe","Spear","Club"};
    Item i; i.type=ItemType::Weapon;
    i.name=names[std::rand()%5]; i.desc="Main weapon.";
    i.value=2+std::rand()%6; return i;
}

Item Item::makeOffWeapon()
{
    static const char* names[]={"Shield","Buckler","Torch","Dagger","Wand"};
    Item i; i.type=ItemType::OffWeapon;
    i.name=names[std::rand()%5]; i.desc="Off-hand item.";
    i.value=1+std::rand()%4; return i;
}

Item Item::makeArmor()
{
    // สุ่ม armor piece
    int r=std::rand()%5;
    Item i;
    if(r==0){i.type=ItemType::Helmet;   i.name="Helmet";     i.value=1+std::rand()%3;}
    else if(r==1){i.type=ItemType::BodyArmor;i.name="Armor"; i.value=2+std::rand()%5;}
    else if(r==2){i.type=ItemType::Gloves;  i.name="Gloves"; i.value=1+std::rand()%3;}
    else if(r==3){i.type=ItemType::Greaves; i.name="Greaves";i.value=1+std::rand()%3;}
    else{         i.type=ItemType::Boots;   i.name="Boots";  i.value=1+std::rand()%3;}
    i.desc="Armor piece.";
    return i;
}

sf::Color Item::color() const
{
    switch(type)
    {
        case ItemType::Food:      return sf::Color(200,160,60);
        case ItemType::Potion:    return sf::Color(60,160,220);
        case ItemType::Weapon:    return sf::Color(180,180,220);
        case ItemType::OffWeapon: return sf::Color(150,150,200);
        case ItemType::Helmet:    return sf::Color(160,120,80);
        case ItemType::BodyArmor: return sf::Color(140,100,60);
        case ItemType::Gloves:    return sf::Color(120,90,60);
        case ItemType::Greaves:   return sf::Color(130,100,70);
        case ItemType::Boots:     return sf::Color(110,80,50);
        default:                  return sf::Color(200,200,200);
    }
}

std::string Item::spritePath() const
{
    switch(type)
    {
        case ItemType::Food:      return "assets/textures/item_food.png";
        case ItemType::Potion:    return "assets/textures/item_potion.png";
        case ItemType::Weapon:    return "assets/textures/item_weapon.png";
        case ItemType::OffWeapon: return "assets/textures/item_offweapon.png";
        case ItemType::Helmet:    return "assets/textures/item_helmet.png";
        case ItemType::BodyArmor: return "assets/textures/item_armor.png";
        case ItemType::Gloves:    return "assets/textures/item_gloves.png";
        case ItemType::Greaves:   return "assets/textures/item_greaves.png";
        case ItemType::Boots:     return "assets/textures/item_boots.png";
        default:                  return "assets/textures/item_food.png";
    }
}
