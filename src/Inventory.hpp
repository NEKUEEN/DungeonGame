#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "Item.hpp"

constexpr int MAX_INVENTORY = 12;

class Inventory
{
public:
    Inventory() = default;

    bool addItem(const Item& item);           // return false ถ้าเต็ม
    void removeItem(int index);
    Item* getItem(int index);
    int   size() const { return (int)m_items.size(); }
    bool  isFull() const { return (int)m_items.size() >= MAX_INVENTORY; }

    // Render inventory popup
    void render(sf::RenderWindow& window, const sf::Font& font,
                int selectedIndex);

    // weapon/armor ที่ equip อยู่
    int equippedWeaponBonus  = 0;
    int equippedArmorBonus   = 0;

private:
    std::vector<Item> m_items;
};
