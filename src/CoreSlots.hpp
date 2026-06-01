#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <optional>
#include "Item.hpp"
#include "DropTable.hpp"

// ============================================================
//  CoreStats  –  bonus stats จากแกน
// ============================================================
struct CoreStats
{
    int body        = 0;
    int mentality   = 0;
    int ability     = 0;
    int battleIndex = 0;
};

// ============================================================
//  CoreSlots  –  ช่องใส่แกน จำนวน = level ของ player
// ============================================================
class CoreSlots
{
public:
    CoreSlots() = default;

    // อัพเดทจำนวน slot ตาม level (เรียกตอน level up)
    void setSlotCount(int level) { m_slots.resize(level); }
    int  getSlotCount() const { return (int)m_slots.size(); }

    // ใส่/ถอดแกน
    bool        equip(int slotIdx, const Item& core);
    Item        unequip(int slotIdx);          // return แกนที่ถอด
    bool        hasCore(int slotIdx) const;
    const Item* getCore(int slotIdx) const;

    // คำนวณ bonus รวมจากทุก slot
    CoreStats   getTotalBonus() const;

    // Render (วาดใน UI)
    void render(sf::RenderWindow& window, const sf::Font& font,
                float startX, float startY, float panelW,
                int selectedSlot, bool focused);

private:
    CoreStats readStats(const Item& core) const;

    std::vector<std::optional<Item>> m_slots;  // nullptr = ว่าง
};
