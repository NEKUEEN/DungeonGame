#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <optional>
#include "Item.hpp"
#include "DropTable.hpp"
#include "StatBonus.hpp"   // <-- เพิ่ม

// ********** ลบ struct CoreStats เดิมออก **********

class CoreSlots
{
public:
    CoreSlots() = default;
    void setSlotCount(int level) { m_slots.resize(level); }
    int  getSlotCount() const { return (int)m_slots.size(); }

    bool        equip(int slotIdx, const Item& core);
    Item        unequip(int slotIdx);
    bool        hasCore(int slotIdx) const;
    const Item* getCore(int slotIdx) const;

    StatBonus   getTotalBonus() const;   // เปลี่ยน return type

    void render(sf::RenderWindow& window, const sf::Font& font,
                float startX, float startY, float panelW,
                int selectedSlot, bool focused);

private:
    StatBonus readStats(const Item& core) const;   // เปลี่ยน return type

    std::vector<std::optional<Item>> m_slots;
};