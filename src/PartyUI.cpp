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
}

void PartyUI::render(sf::RenderWindow& window, const Party& party, const Player* leader)
{
    // ไม่มีปาร์ตี้ และไม่มี leader → ไม่วาดอะไร
    if (!leader && party.size() == 0) return;

    // slot 0 = player (leader)
    if (leader)
        renderLeader(window, leader);

    // slot 1+ = NPC companions
    for (int i = 0; i < party.size(); i++)
    {
        auto member = party.getMember(i);
        if (member)
            renderMember(window, member, i + 1, false);
    }
}

void PartyUI::renderLeader(sf::RenderWindow& window, const Player* player)
{
    if (!player) return;

    int y = START_Y;
    const Stats& s = player->getStats();

    sf::RectangleShape box({(float)BOX_WIDTH, (float)BOX_HEIGHT});
    box.setPosition({(float)START_X, (float)y});
    box.setFillColor(sf::Color(60, 80, 120));
    box.setOutlineThickness(2.f);
    box.setOutlineColor(sf::Color::White);
    window.draw(box);

    sf::Text nameText(m_font);
    nameText.setCharacterSize(14);
    nameText.setFillColor(sf::Color::White);
    nameText.setString("You (Leader)");
    nameText.setPosition({(float)(START_X + 8), (float)(y + 5)});
    window.draw(nameText);

    sf::Text hpText(m_font);
    hpText.setCharacterSize(12);
    hpText.setFillColor(sf::Color::White);
    std::ostringstream oss;
    oss << "HP: " << s.hp << "/" << s.maxHp;
    hpText.setString(oss.str());
    hpText.setPosition({(float)(START_X + 8), (float)(y + 25)});
    window.draw(hpText);

    float barWidth = BOX_WIDTH - 16;
    float hpPct = s.maxHp > 0 ? (float)s.hp / s.maxHp : 0.f;

    sf::RectangleShape bg({barWidth, 8});
    bg.setPosition({(float)(START_X + 8), (float)(y + 40)});
    bg.setFillColor(sf::Color(50, 50, 50));
    window.draw(bg);

    sf::RectangleShape fill({barWidth * hpPct, 8});
    fill.setPosition({(float)(START_X + 8), (float)(y + 40)});
    fill.setFillColor(hpPct > 0.3f ? sf::Color::Green : sf::Color::Red);
    window.draw(fill);

    sf::Text lvText(m_font);
    lvText.setCharacterSize(11);
    lvText.setFillColor(sf::Color::Yellow);
    lvText.setString("Lv." + std::to_string(s.level));
    lvText.setPosition({(float)(START_X + BOX_WIDTH - 45), (float)(y + 25)});
    window.draw(lvText);
}

void PartyUI::renderMember(sf::RenderWindow& window, const std::shared_ptr<NPC>& npc,
                            int slotIndex, bool isLeader)
{
    int y = START_Y + slotIndex * SLOT_HEIGHT;

    sf::RectangleShape box({(float)BOX_WIDTH, (float)BOX_HEIGHT});
    box.setPosition({(float)START_X, (float)y});

    sf::Color bgColor = sf::Color(40, 40, 50);
    if (npc->getType() == NPCType::Companion)
    {
        auto role = npc->getRole();
        if      (role == NPCRole::Warrior) bgColor = sf::Color(100, 50, 50);
        else if (role == NPCRole::Healer)  bgColor = sf::Color(50, 100, 50);
        else if (role == NPCRole::Mage)    bgColor = sf::Color(50, 50, 100);
        else if (role == NPCRole::Rogue)   bgColor = sf::Color(80, 40, 80);
    }
    box.setFillColor(bgColor);
    box.setOutlineThickness(2.f);
    box.setOutlineColor(sf::Color::White);
    window.draw(box);

    // ชื่อ + role
    std::string roleStr;
    if (npc->getType() == NPCType::Companion)
    {
        auto role = npc->getRole();
        if      (role == NPCRole::Warrior) roleStr = "Warrior";
        else if (role == NPCRole::Healer)  roleStr = "Healer";
        else if (role == NPCRole::Mage)    roleStr = "Mage";
        else if (role == NPCRole::Rogue)   roleStr = "Rogue";
    }
    std::string displayName = npc->getName();
    if (!roleStr.empty()) displayName += " (" + roleStr + ")";

    sf::Text nameText(m_font);
    nameText.setCharacterSize(14);
    nameText.setFillColor(sf::Color::White);
    nameText.setString(displayName);
    nameText.setPosition({(float)(START_X + 8), (float)(y + 5)});
    window.draw(nameText);

    // HP text
    sf::Text hpText(m_font);
    hpText.setCharacterSize(12);
    hpText.setFillColor(sf::Color::White);
    std::ostringstream oss;
    oss << "HP: " << npc->getHP() << "/" << npc->getMaxHP();
    hpText.setString(oss.str());
    hpText.setPosition({(float)(START_X + 8), (float)(y + 25)});
    window.draw(hpText);

    // HP bar
    float barWidth = BOX_WIDTH - 16;
    float hpPct = npc->getMaxHP() > 0 ? (float)npc->getHP() / npc->getMaxHP() : 0.f;

    sf::RectangleShape bg({barWidth, 8});
    bg.setPosition({(float)(START_X + 8), (float)(y + 40)});
    bg.setFillColor(sf::Color(50, 50, 50));
    window.draw(bg);

    sf::RectangleShape fill({barWidth * hpPct, 8});
    fill.setPosition({(float)(START_X + 8), (float)(y + 40)});
    fill.setFillColor(hpPct > 0.3f ? sf::Color::Green : sf::Color::Red);
    window.draw(fill);

    // Level
    sf::Text lvText(m_font);
    lvText.setCharacterSize(11);
    lvText.setFillColor(sf::Color::Yellow);
    lvText.setString("Lv." + std::to_string(npc->getLevel()));
    lvText.setPosition({(float)(START_X + BOX_WIDTH - 45), (float)(y + 25)});
    window.draw(lvText);
}
