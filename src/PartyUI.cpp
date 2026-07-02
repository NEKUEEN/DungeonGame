#include "PartyUI.hpp"
#include "Party.hpp"
#include "Player.hpp"
#include "NPC.hpp"
#include <sstream>
#include <iostream>

PartyUI::PartyUI()
{
    if (!m_font.openFromFile("assets/fonts/font.ttf"))
        std::cerr << "[PartyUI] Warning: Could not load font\n";

    // สร้าง slot ทั้งหมดครั้งเดียวตอน startup — จะได้ไม่ต้อง alloc ตอน runtime
    m_slots.reserve(MAX_SLOTS);
    for (int i = 0; i < MAX_SLOTS; i++)
        m_slots.push_back(std::make_unique<SlotUI>(m_font));
}

void PartyUI::render(sf::RenderWindow& window, const Party& party, const Player* leader)
{
    // ปิดอยู่ → ไม่วาดอะไรเลย ไม่เสีย perf แม้แต่นิดเดียว
    if (!m_visible) return;

    if (!leader && party.size() == 0) return;

    // slot 0 = player (leader)
    if (leader)
        renderLeader(window, leader);

    // slot 1+ = NPC companions
    for (int i = 0; i < party.size(); i++)
    {
        auto member = party.getMember(i);
        if (member)
            renderMember(window, member, i + 1);
    }
}

void PartyUI::updateSlotVisuals(SlotUI& slot, int y, const std::string& name,
                                 int hp, int maxHp, int level, const sf::Color& bgColor)
{
    // ตำแหน่ง/สีกล่อง — set ทุกเฟรมได้ ราคาถูก ไม่เกี่ยวกับ glyph rebuild
    slot.box.setPosition({(float)START_X, (float)y});
    slot.box.setFillColor(bgColor);
    slot.box.setOutlineThickness(2.f);
    slot.box.setOutlineColor(sf::Color::White);

    // ── ชื่อ: setString ใหม่เฉพาะตอนชื่อเปลี่ยนจริง ──
    if (!slot.initialized || slot.lastName != name)
    {
        slot.nameText.setCharacterSize(14);
        slot.nameText.setFillColor(sf::Color::White);
        slot.nameText.setString(name);
        slot.lastName = name;
    }
    slot.nameText.setPosition({(float)(START_X + 8), (float)(y + 5)});

    // ── HP text: setString ใหม่เฉพาะตอน hp/maxHp เปลี่ยน ──
    if (!slot.initialized || slot.lastHp != hp || slot.lastMaxHp != maxHp)
    {
        std::ostringstream oss;
        oss << "HP: " << hp << "/" << maxHp;
        slot.hpText.setCharacterSize(12);
        slot.hpText.setFillColor(sf::Color::White);
        slot.hpText.setString(oss.str());
        slot.lastHp = hp;
        slot.lastMaxHp = maxHp;
    }
    slot.hpText.setPosition({(float)(START_X + 8), (float)(y + 25)});

    // ── Level text: setString ใหม่เฉพาะตอน level เปลี่ยน ──
    if (!slot.initialized || slot.lastLevel != level)
    {
        slot.lvText.setCharacterSize(11);
        slot.lvText.setFillColor(sf::Color::Yellow);
        slot.lvText.setString("Lv." + std::to_string(level));
        slot.lastLevel = level;
    }
    slot.lvText.setPosition({(float)(START_X + BOX_WIDTH - 45), (float)(y + 25)});

    // HP bar — เป็น RectangleShape ไม่มี glyph ต้อง rebuild อะไร set ทุกเฟรมได้สบาย
    float barWidth = BOX_WIDTH - 16;
    float hpPct = maxHp > 0 ? (float)hp / maxHp : 0.f;

    slot.hpBg.setSize({barWidth, 8});
    slot.hpBg.setPosition({(float)(START_X + 8), (float)(y + 40)});
    slot.hpBg.setFillColor(sf::Color(50, 50, 50));

    slot.hpFill.setSize({barWidth * hpPct, 8});
    slot.hpFill.setPosition({(float)(START_X + 8), (float)(y + 40)});
    slot.hpFill.setFillColor(hpPct > 0.3f ? sf::Color::Green : sf::Color::Red);

    slot.initialized = true;
}

void PartyUI::renderLeader(sf::RenderWindow& window, const Player* player)
{
    if (!player) return;

    const Stats& s = player->getStats();
    SlotUI& slot = *m_slots[0];

    updateSlotVisuals(slot, START_Y, "You (Leader)", s.hp, s.maxHp, s.level,
                       sf::Color(60, 80, 120));

    window.draw(slot.box);
    window.draw(slot.nameText);
    window.draw(slot.hpText);
    window.draw(slot.hpBg);
    window.draw(slot.hpFill);
    window.draw(slot.lvText);
}

void PartyUI::renderMember(sf::RenderWindow& window, const std::shared_ptr<NPC>& npc, int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= MAX_SLOTS) return; // กันเกิน (party เต็มที่ 6 คน)
    if (!npc) return;

    sf::Color bgColor = sf::Color(40, 40, 50);
    std::string roleStr;
    if (npc->getType() == NPCType::Companion)
    {
        switch (npc->getRole())
        {
            case NPCRole::Warrior: bgColor = sf::Color(100, 50, 50); roleStr = "Warrior"; break;
            case NPCRole::Healer:  bgColor = sf::Color(50, 100, 50); roleStr = "Healer";  break;
            case NPCRole::Mage:    bgColor = sf::Color(50, 50, 100); roleStr = "Mage";    break;
            case NPCRole::Rogue:   bgColor = sf::Color(80, 40, 80);  roleStr = "Rogue";   break;
        }
    }

    std::string displayName = npc->getName();
    if (!roleStr.empty()) displayName += " (" + roleStr + ")";

    int y = START_Y + slotIndex * SLOT_HEIGHT;
    SlotUI& slot = *m_slots[slotIndex];

    updateSlotVisuals(slot, y, displayName, npc->getHP(), npc->getMaxHP(), npc->getLevel(), bgColor);

    window.draw(slot.box);
    window.draw(slot.nameText);
    window.draw(slot.hpText);
    window.draw(slot.hpBg);
    window.draw(slot.hpFill);
    window.draw(slot.lvText);
}
