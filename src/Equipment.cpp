#include "Equipment.hpp"
#include <algorithm>

bool Equipment::equip(const Item& item, EquipSlot slot)
{
    if (!canEquipTo(item, slot)) return false;
    m_equipped[slot] = item;
    return true;
}

Item Equipment::unequip(EquipSlot slot)
{
    auto it = m_equipped.find(slot);
    if (it == m_equipped.end()) return Item{};
    Item item = it->second;
    m_equipped.erase(it);
    return item;
}

bool Equipment::hasItem(EquipSlot slot) const
{
    return m_equipped.count(slot) > 0;
}

const Item* Equipment::getItem(EquipSlot slot) const
{
    auto it = m_equipped.find(slot);
    if (it == m_equipped.end()) return nullptr;
    return &it->second;
}

int Equipment::getTotalHpBonus() const
{
    int bonus = 0;
    for (auto& [slot, item] : m_equipped)
        bonus += item.hpBonus;
    return bonus;
}

int Equipment::getTotalAtkBonus() const
{
    int bonus = 0;
    for (auto& [slot, item] : m_equipped)
        bonus += item.atkBonus;
    return bonus;
}

int Equipment::getTotalDefBonus() const
{
    int bonus = 0;
    for (auto& [slot, item] : m_equipped)
        bonus += item.defBonus;
    return bonus;
}

int Equipment::getTotalDodgeBonus() const
{
    int bonus = 0;
    for (auto& [slot, item] : m_equipped)
        bonus += item.dodgeBonus;
    return bonus;
}

int Equipment::getTotalManaBonus() const
{
    int bonus = 0;
    for (auto& [slot, item] : m_equipped)
        bonus += item.manaBonus;
    return bonus;
}

int Equipment::getTotalMagicDmgBonus() const
{
    int bonus = 0;
    for (auto& [slot, item] : m_equipped)
        bonus += item.magicDmgBonus;
    return bonus;
}

int Equipment::getTotalMagicResBonus() const
{
    int bonus = 0;
    for (auto& [slot, item] : m_equipped)
        bonus += item.magicResBonus;
    return bonus;
}


// ============================================================
//  Render  –  แสดง equipment slots
// ============================================================
void Equipment::render(sf::RenderWindow& window, const sf::Font& font,
                       int selectedSlot, bool focused)
{
    auto winSize = window.getSize();
    float panelW = 220.f;
    float px     = (float)winSize.x - panelW;

    // slots layout
    const EquipSlot slots[] = {
        EquipSlot::Head, EquipSlot::Body, EquipSlot::Arms,
        EquipSlot::Legs, EquipSlot::Feet,
        EquipSlot::MainHand, EquipSlot::OffHand
    };

    float slotW = 28.f, slotH = 22.f, gap = 3.f;
    float startX = px + 8.f;
    float startY = 0.f;  // จะ set จากข้างนอก

    // หา startY จาก window height - log - inv area
    // เราวาดใน INV_GRID_H zone
    // ส่งค่ามาจาก Game แทนดีกว่า — วาดตรงนี้ก็ได้โดยใช้ viewport coords
}

std::string Equipment::slotName(EquipSlot slot)
{
    switch(slot)
    {
        case EquipSlot::Head:     return "Head";
        case EquipSlot::Body:     return "Body";
        case EquipSlot::Arms:     return "Arms";
        case EquipSlot::Legs:     return "Legs";
        case EquipSlot::Feet:     return "Feet";
        case EquipSlot::MainHand: return "Main";
        case EquipSlot::OffHand:  return "Sub";
        default:                  return "??";
    }
}

EquipSlot Equipment::itemToSlot(const Item& item)
{
    switch(item.type)
    {
        case ItemType::Helmet:    return EquipSlot::Head;
        case ItemType::BodyArmor: return EquipSlot::Body;
        case ItemType::Gloves:    return EquipSlot::Arms;
        case ItemType::Greaves:   return EquipSlot::Legs;
        case ItemType::Boots:     return EquipSlot::Feet;
        case ItemType::Weapon:    return EquipSlot::MainHand;
        case ItemType::OffWeapon: return EquipSlot::OffHand;
        default:                  return EquipSlot::Body;
    }
}

bool Equipment::canEquipTo(const Item& item, EquipSlot slot)
{
    return itemToSlot(item) == slot;
}
