#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <optional>
#include "Item.hpp"
#include "DropTable.hpp"

// ============================================================
//  CoreStats  –  bonus sub-stats จากแกน
// ============================================================
struct CoreStats
{
    int hp    = 0;
    int atk   = 0;
    int def   = 0;
    int dodge = 0;
    int mana = 0;
    int magicDmg = 0;
    int magicRes = 0;
    // อนาคต: int speed = 0; int atkSpd = 0;
    // อนาคต mentality: int magicDmg = 0; int mana = 0; int magicRes = 0;
};

// ============================================================
//  CoreSlots  –  ช่องใส่แกน จำนวน = level ของ player
// ============================================================
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

    CoreStats   getTotalBonus() const;

    void render(sf::RenderWindow& window, const sf::Font& font,
                float startX, float startY, float panelW,
                int selectedSlot, bool focused);

private:
    CoreStats readStats(const Item& core) const;

    std::vector<std::optional<Item>> m_slots;
};
