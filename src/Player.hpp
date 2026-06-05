#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "TileMap.hpp"
#include "Skill.hpp"
#include <algorithm>
#include <SkillDB.hpp>

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
    int  maxMentality = 0;
    int  mentality    = 0;
    bool hpDepleted   = false;

    // ── Mentality sub-stats ──
    int maxMagicDmg  = 0;
    int maxMana      = 20;   // เตรียมไว้ก่อน
    int maxMagicRes  = 0;
    // อนาคต: int maxPoisonRes = 0; int maxMagicSpd = 0;

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

    // ── Action Points ──
    int maxAP     = 1;   // ปกติ 1 AP/turn
    int currentAP = 0;   // AP เหลือใน turn นี้

    // ── Attack Speed ──
    // AP cost ของการโจมตี x100 (100 = ปกติ = หัก 1 AP, 50 = หัก 0.5 AP)
    int atkSpeedCost  = 100;
    int atkAPAccum    = 0;   // accumulator สำหรับ fractional AP cost
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
    void setPos(int col, int row) { m_col = col; m_row = row; }

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

    // ── Hotbar ──
    const std::string& getHotbar(int idx) const
    {
        static const std::string empty = "";
        if (idx < 0 || idx >= 9) return empty;
        return m_hotbar[idx];
    }

    void setHotbar(int idx, const std::string& skillId)
    {
        if (idx >= 0 && idx < 9) m_hotbar[idx] = skillId;
    }

    // เพิ่ม skill จาก core → หา hotbar slot ว่างแรก
    bool addCoreSkill(const std::string& skillId)
    {
        if (!findSkill(skillId))
        {
            const SkillData* sd = SkillDB::instance().get(skillId);
            if (!sd) return false;
            SkillInstance inst;
            inst.data = *sd;
            m_skills.push_back(inst);
        }
        for (int i = 0; i < 9; ++i)
            if (m_hotbar[i].empty())
            { m_hotbar[i] = skillId; return true; }
        return false;  // hotbar เต็ม
    }

    // ลบ skill ออกจาก hotbar + m_skills (ตอนถอดคอร์)
    void removeCoreSkill(const std::string& skillId)
    {
        for (int i = 0; i < 9; ++i)
            if (m_hotbar[i] == skillId)
                m_hotbar[i] = "";

        m_skills.erase(
            std::remove_if(m_skills.begin(), m_skills.end(),
                [&](const SkillInstance& sk){ return sk.data.id == skillId; }),
            m_skills.end());
    }

private:
    void drawHPBar(sf::RenderWindow& window);

    int         m_col, m_row, m_tileSize;
    Stats       m_stats;
    sf::Texture m_texture;
    sf::Sprite* m_sprite    = nullptr;
    bool        m_hasSprite = false;

    std::vector<SkillInstance> m_skills;

    // Hotbar — เก็บ skill id ที่ assign ไว้ใน slot 0-8
    // "" = ว่าง
    std::string m_hotbar[9];
};
