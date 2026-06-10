#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <map>
#include "Item.hpp"
#include "StatBonus.hpp"

enum class EquipSlot { Head, Body, Arms, Legs, Feet, MainHand, OffHand };

class Equipment
{
public:
    Equipment() = default;

    bool        equip(const Item& item, EquipSlot slot);
    Item        unequip(EquipSlot slot);
    bool        hasItem(EquipSlot slot) const;
    const Item* getItem(EquipSlot slot) const;

    // ใหม่: คืน StatBonus รวมจากทุกช่อง
    StatBonus getTotalBonus() const;

    // ฟังก์ชันเดิมสามารถเก็บไว้หรือจะเรียกใช้ getTotalBonus ก็ได้
    int getTotalHpBonus() const { return getTotalBonus().hp; }
    int getTotalAtkBonus() const { return getTotalBonus().atk; }
    int getTotalDefBonus() const { return getTotalBonus().def; }
    int getTotalDodgeBonus() const { return getTotalBonus().dodge; }
    int getTotalManaBonus() const { return getTotalBonus().mana; }
    int getTotalMagicDmgBonus() const { return getTotalBonus().matk; }
    int getTotalMagicResBonus() const { return getTotalBonus().magicRes; }
    int getTotalSpdBonus() const { return getTotalBonus().spd; }

    void render(sf::RenderWindow& window, const sf::Font& font,
                int selectedSlot, bool focused);

    static std::string slotName(EquipSlot slot);
    static EquipSlot   itemToSlot(const Item& item);
    static bool        canEquipTo(const Item& item, EquipSlot slot);

    static constexpr int SLOT_COUNT = 7;

private:
    std::map<EquipSlot, Item> m_equipped;
};