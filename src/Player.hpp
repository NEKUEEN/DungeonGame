#pragma once
#include <SFML/Graphics.hpp>
#include "TileMap.hpp"

struct Stats
{
    int level       = 1;

    // ── Base stats (ก่อน equipment/core) ──
    int maxHp       = 25;
    int maxAtk      = 5;
    int maxDef      = 2;
    int maxDodge    = 2;

    // ── Computed by Game::computeBody() ──
    int body        = 0;   // maxHp*0.4 + maxAtk*0.3 + maxDef*0.2 + maxDodge*0.1

    // ── HP bar (เลือดจริง ใต้ตัวละคร) ──
    int hp          = 25;  // เริ่ม = maxHp

    // ── Mentality (เลือดสำรอง — แสดงค่า max, ไม่ลดตาม dmg) ──
    int maxMentality = 20;
    int mentality    = 20;
    bool hpDepleted  = false;  // flag: hp ลงถึง 0 → drain mentality ทุก turn

    // ── Ability (จาก core bonus) ──
    int ability     = 0;

    // ── Item Level (sum grade จาก equipment — set จาก Game) ──
    int itemLevel   = 0;

    // ── Battle Index (computed ใน Game) ──
    int battleIndex = 0;

    // ── Hunger ──
    int hunger      = 100;

    // ── EXP ──
    int exp         = 0;
    int expToNext   = 100;

    // ── Timers (ไม่แสดงใน UI) ──
    int hungerTimer = 0;
    int regenTimer  = 0;
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
