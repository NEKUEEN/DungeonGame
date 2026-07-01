#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>

class Party;
class NPC;
class Player;

class PartyUI
{
public:
    PartyUI();

    // render(window, party, leader)
    // leader = m_player (แสดง slot 0 เป็น player จริง)
    void render(sf::RenderWindow& window, const Party& party, const Player* leader = nullptr);

private:
    // วาด slot ที่เป็น NPC
    void renderMember(sf::RenderWindow& window, const std::shared_ptr<NPC>& npc,
                      int slotIndex, bool isLeader);

    // วาด slot 0 ที่เป็น Player
    void renderLeader(sf::RenderWindow& window, const Player* player);

    sf::Font m_font;
    static constexpr int SLOT_HEIGHT = 70;
    static constexpr int START_X     = 20;
    static constexpr int START_Y     = 20;
    static constexpr int BOX_WIDTH   = 200;
    static constexpr int BOX_HEIGHT  = 60;
};
