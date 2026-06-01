#pragma once
#include <SFML/Graphics.hpp>
#include "TileMap.hpp"

struct Stats
{
    int level       = 1;
    int body        = 25;
    int mentality   = 36;
    int ability     = 1;
    int itemLevel   = 0;
    int battleIndex = 68;
    int hp          = 25;
    int hunger      = 100;

    int exp         = 0;
    int expToNext   = 100;

    // timers (ไม่แสดงใน UI)
    int hungerTimer = 0;   // นับถึง 100 แล้วลด hunger
    int regenTimer  = 0;   // นับถึง 7 แล้วรีเจน HP 1
};

class Player
{
public:
    Player(int startCol, int startRow, int tileSize);
    ~Player() { delete m_sprite; }

    bool tryMove(int dc, int dr, const TileMap& map);
    void render(sf::RenderWindow& window);
    void onTurnPassed();

    bool addExp(int amount);
    void levelUp();

    int getCol() const { return m_col; }
    int getRow() const { return m_row; }
    Stats& getStats()             { return m_stats; }
    const Stats& getStats() const { return m_stats; }

private:
    void drawHPBar(sf::RenderWindow& window);

    int   m_col, m_row, m_tileSize;
    Stats m_stats;

    sf::Texture m_texture;
    sf::Sprite* m_sprite    = nullptr;
    bool        m_hasSprite = false;
};
