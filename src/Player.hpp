#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "TileMap.hpp"
#include "Skill.hpp"

struct Stats
{
    int level = 1;

    // ── Base stats (ก่อน equipment/core) ──
    int maxHp    = 25;
    int maxAtk   = 5;
    int maxDef   = 2;
    int maxDodge = 2;

    // ── Computed by Game::computeBody() ──
    int body = 0;

    // ── HP bar ──
    int hp = 25;

    // ── Mentality ──
    int  maxMentality = 20;
    int  mentality    = 20;
    bool hpDepleted   = false;

    // ── Ability (จาก core bonus) ──
    int ability = 0;

    // ── Item Level ──
    int itemLevel = 0;

    // ── Battle Index ──
    int battleIndex = 0;

    // ── Hunger ──
    int hunger = 100;

    // ── EXP ──
    int exp       = 0;
    int expToNext = 100;

    // ── Timers ──
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

    Stats&       getStats()       { return m_stats; }
    const Stats& getStats() const { return m_stats; }

    // ── Skills ──
    std::vector<SkillInstance>&       getSkills()       { return m_skills; }
    const std::vector<SkillInstance>& getSkills() const { return m_skills; }

    SkillInstance* findSkill(const std::string& id)
    {
        for (auto& sk : m_skills)
            if (sk.data.id == id) return &sk;
        return nullptr;
    }

    const SkillInstance* findSkill(const std::string& id) const
    {
        for (const auto& sk : m_skills)
            if (sk.data.id == id) return &sk;
        return nullptr;
    }

private:
    void drawHPBar(sf::RenderWindow& window);

    int         m_col, m_row, m_tileSize;
    Stats       m_stats;
    sf::Texture m_texture;
    sf::Sprite* m_sprite    = nullptr;
    bool        m_hasSprite = false;

    std::vector<SkillInstance> m_skills;
};
