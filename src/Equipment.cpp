// ==================== Equipment.cpp (สมบูรณ์ เปลี่ยน magicDmg -> matk) ====================
#include "Equipment.hpp"
#include <algorithm>

bool Equipment::equip(const Item& item, EquipSlot slot) {
    if (!canEquipTo(item, slot)) return false;
    m_equipped[slot] = item;
    return true;
}

Item Equipment::unequip(EquipSlot slot) {
    auto it = m_equipped.find(slot);
    if (it == m_equipped.end()) return Item{};
    Item item = it->second;
    m_equipped.erase(it);
    return item;
}

bool Equipment::hasItem(EquipSlot slot) const {
    return m_equipped.count(slot) > 0;
}

const Item* Equipment::getItem(EquipSlot slot) const {
    auto it = m_equipped.find(slot);
    if (it == m_equipped.end()) return nullptr;
    return &it->second;
}

StatBonus Equipment::getTotalBonus() const {
    StatBonus total;
    for (auto& [slot, item] : m_equipped) {
        total.hp    += item.hpBonus;
        total.atk   += item.atkBonus;
        total.def   += item.defBonus;
        total.dodge += item.dodgeBonus;
        total.mana  += item.manaBonus;
        total.matk  += item.magicDmgBonus;    // เปลี่ยน
        total.magicRes += item.magicResBonus;
        total.spd   += item.spdBonus;
    }
    return total;
}

void Equipment::render(sf::RenderWindow& window, const sf::Font& font, int selectedSlot, bool focused) {
    // ปล่อยว่างหรือ implement ภายหลัง
}

std::string Equipment::slotName(EquipSlot slot) {
    switch(slot) {
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

EquipSlot Equipment::itemToSlot(const Item& item) {
    switch(item.type) {
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

bool Equipment::canEquipTo(const Item& item, EquipSlot slot) {
    return itemToSlot(item) == slot;
}