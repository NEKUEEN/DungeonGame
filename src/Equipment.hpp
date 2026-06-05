#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <map>
#include "Item.hpp"

// ============================================================
//  Equipment Slot Types
// ============================================================
enum class EquipSlot
{
    Head,       // หมวก
    Body,       // เกราะตัว
    Arms,       // เกราะแขน
    Legs,       // เกราะขา
    Feet,       // รองเท้า
    MainHand,   // อาวุธหลัก
    OffHand,    // อาวุธรอง / โล่
};

// ============================================================
//  Equipment  –  เก็บ item ที่ equip อยู่ทุก slot
// ============================================================
class Equipment
{
public:
    Equipment() = default;

    bool        equip(const Item& item, EquipSlot slot);  // return false = slot ไม่ตรง
    Item        unequip(EquipSlot slot);                   // return item ที่ถอดออก
    bool        hasItem(EquipSlot slot) const;
    const Item* getItem(EquipSlot slot) const;

    // คำนวณ bonus รวมจากทุก slot
    int getTotalHpBonus() const;
    int getTotalAtkBonus() const;
    int getTotalDefBonus() const;
    int getTotalDodgeBonus() const;
    int getTotalManaBonus() const;
    int getTotalMagicDmgBonus() const;
    int getTotalMagicResBonus() const;

    void render(sf::RenderWindow& window, const sf::Font& font,
                int selectedSlot, bool focused);

    static std::string slotName(EquipSlot slot);
    static EquipSlot   itemToSlot(const Item& item);  // แปลง item type → slot ที่เหมาะ
    static bool        canEquipTo(const Item& item, EquipSlot slot);

    static constexpr int SLOT_COUNT = 7;

private:
    std::map<EquipSlot, Item> m_equipped;
};
