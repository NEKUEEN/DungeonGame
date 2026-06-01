#include "CoreSlots.hpp"
#include <algorithm>

bool CoreSlots::equip(int slotIdx, const Item& core)
{
    if (slotIdx < 0 || slotIdx >= (int)m_slots.size()) return false;
    if (core.type != ItemType::Core) return false;  // ใส่ได้เฉพาะ Core
    m_slots[slotIdx] = core;
    return true;
}

Item CoreSlots::unequip(int slotIdx)
{
    if (slotIdx < 0 || slotIdx >= (int)m_slots.size()) return Item{};
    if (!m_slots[slotIdx].has_value()) return Item{};
    Item item = m_slots[slotIdx].value();
    m_slots[slotIdx].reset();
    return item;
}

bool CoreSlots::hasCore(int slotIdx) const
{
    if (slotIdx < 0 || slotIdx >= (int)m_slots.size()) return false;
    return m_slots[slotIdx].has_value();
}

const Item* CoreSlots::getCore(int slotIdx) const
{
    if (slotIdx < 0 || slotIdx >= (int)m_slots.size()) return nullptr;
    if (!m_slots[slotIdx].has_value()) return nullptr;
    return &m_slots[slotIdx].value();
}

// ============================================================
//  readStats  –  อ่าน core_stats จาก DropTable
// ============================================================
CoreStats CoreSlots::readStats(const Item& core) const
{
    CoreStats cs;
    // หา item id จากชื่อ (ใช้ name map ผ่าน DropTable)
    // วิธีง่ายกว่า: เก็บ id ไว้ใน Item แทนแต่ตอนนี้ไม่มี
    // ใช้ hardcode ตาม name ก่อน แล้วค่อยแก้ตอนมี id
    if (core.name == "Goblin Core")
    { cs.mentality=3; cs.ability=1; cs.battleIndex=5; }
    else if (core.name == "Orc Core")
    { cs.body=5; cs.battleIndex=8; }
    else if (core.name == "Rat Core")
    { cs.body=1; cs.mentality=1; cs.battleIndex=2; }
    return cs;
}

CoreStats CoreSlots::getTotalBonus() const
{
    CoreStats total;
    for (const auto& slot : m_slots)
    {
        if (!slot.has_value()) continue;
        CoreStats cs = readStats(slot.value());
        total.body        += cs.body;
        total.mentality   += cs.mentality;
        total.ability     += cs.ability;
        total.battleIndex += cs.battleIndex;
    }
    return total;
}

// ============================================================
//  Render  –  แสดง core slots
// ============================================================
void CoreSlots::render(sf::RenderWindow& window, const sf::Font& font,
                       float startX, float startY, float panelW,
                       int selectedSlot, bool focused)
{
    // Title
    sf::Text title(font, "CORE SLOTS", 9);
    title.setFillColor(sf::Color(100,200,255));
    title.setPosition({startX, startY});
    window.draw(title);

    float sy = startY + 16.f;
    const float SZ = 26.f, GAP = 4.f;

    for (int i = 0; i < (int)m_slots.size(); ++i)
    {
        float sx = startX + i * (SZ + GAP);
        bool  sel = (i == selectedSlot && focused);

        // slot bg
        sf::RectangleShape slot({SZ, SZ});
        slot.setFillColor(sel ? sf::Color(30,60,80) : sf::Color(15,30,40));
        slot.setOutlineColor(sel ? sf::Color(100,200,255) : sf::Color(40,80,100));
        slot.setOutlineThickness(1.5f);
        slot.setPosition({sx, sy});
        window.draw(slot);

        // แกนใน slot
        if (m_slots[i].has_value())
        {
            const Item& core = m_slots[i].value();
            // ลอง load sprite
            sf::Texture tex;
            if (!core.spriteName.empty() &&
                tex.loadFromFile("assets/textures/"+core.spriteName))
            {
                sf::Sprite spr(tex);
                auto sz=tex.getSize();
                float sc=SZ*0.85f/std::max((float)sz.x,(float)sz.y);
                spr.setScale({sc,sc});
                spr.setPosition({sx+SZ*0.08f,sy+SZ*0.08f});
                window.draw(spr);
            }
            else
            {
                sf::CircleShape dot(SZ*0.3f);
                dot.setFillColor(core.color());
                dot.setOutlineColor(sf::Color(100,200,255,150));
                dot.setOutlineThickness(1.f);
                dot.setPosition({sx+SZ*0.2f, sy+SZ*0.2f});
                window.draw(dot);
            }
        }
        else
        {
            // ว่าง แสดงเลข slot
            sf::Text num(font, std::to_string(i+1), 8);
            num.setFillColor(sf::Color(50,80,100));
            num.setPosition({sx+SZ/2.f-4.f, sy+SZ/2.f-5.f});
            window.draw(num);
        }
    }

    // แสดงชื่อแกนที่ selected
    if (selectedSlot >= 0 && selectedSlot < (int)m_slots.size()
        && m_slots[selectedSlot].has_value())
    {
        const Item& core = m_slots[selectedSlot].value();
        CoreStats cs = readStats(core);
        std::string info = core.name;
        if (cs.body)        info += " B+"+std::to_string(cs.body);
        if (cs.mentality)   info += " M+"+std::to_string(cs.mentality);
        if (cs.ability)     info += " A+"+std::to_string(cs.ability);

        sf::Text infoTxt(font, info, 8);
        infoTxt.setFillColor(sf::Color(100,200,255));
        infoTxt.setPosition({startX, sy + SZ + 4.f});
        window.draw(infoTxt);
    }
    else
    {
        sf::Text empty(font, "Slot "+std::to_string(selectedSlot+1)+": empty", 8);
        empty.setFillColor(sf::Color(60,80,100));
        empty.setPosition({startX, sy + SZ + 4.f});
        window.draw(empty);
    }
}
