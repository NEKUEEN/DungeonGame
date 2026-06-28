#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "TileMap.hpp"
#include "Skill.hpp"
#include <algorithm>
#include <SkillDB.hpp>
#include "StatusEffect.hpp"

struct Stats
{
    int level = 1;

    // ── Base stats (ก่อน equipment/core) ──
    int maxHp    = 0;   // จะถูก sync โดย recalcAllStats (รวม bonus แล้ว)
    int baseMaxHp = 0;  // ค่าดิบก่อน bonus — ใช้ใน recalcAllStats เท่านั้น
    int maxAtk   = 0;
    int maxDef   = 0;
    int maxDodge = 0;
    int maxSpd = 0;
    // ── Body & Carry Weight ──
    int bodyWeight      = 70;
    int strength        = 10;
    int maxCarryWeight  = 50;
    int equipWeight     = 0;
    
    // ── Speed system (Aut-based) ──
    int       moveSpd     = 0;    // bonus จาก equipment/race/skill → move
    int       atkSpd      = 0;    // bonus จาก equipment/race/skill → attack
    int       moveAut     = 100;  // Aut ต่อการเดิน 1 ก้าว (ต่ำ = เร็ว)
    int       atkAut      = 100;  // Aut ต่อการโจมตี 1 ครั้ง (ต่ำ = เร็ว)
    long long nextActTime = 0;    // Aut สะสมที่จะได้ขยับครั้งถัดไป

    // ── Computed by Game::computeBody() ──
    int body = 0;

    // ── HP bar ──
    int hp = 0;

    // ── Mentality ──
    int  maxMentality = 0;
    int  mentality    = 0;

    // ── Mentality sub-stats ──
    int maxMagicDmg  = 0;
    int maxMana      = 0;
    int baseMaxMana  = 0;  // ค่าดิบก่อน bonus
    int mana = 0;
    int manaRegen = 1;   // เตรียมไว้ก่อน
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


    int maxStamina = 100;
    int stamina = 100;
    int staminaRegen = 0;

    // ── Status Resist (0-100, หน่วยเป็น %) ──
    int resistBleed  = 0;
    int resistPoison = 0;
    int resistBurn   = 0;
    int resistStun   = 0;
    int resistSlow   = 0;

    // ── Active status effects ──
    std::vector<StatusEffect> statusEffects;
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
    void setSprite(const std::string& textureKey); // ← เพิ่ม
    // เพิ่มใน public section ของ Player
    void tickStatusEffects(int& hpDelta, std::string& effectName);

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
        {
            // ไม่เช็ค existing แล้ว — สร้าง instance ใหม่เสมอ
        const SkillData* sd = SkillDB::instance().get(skillId);
        if (!sd) return false;

        SkillInstance inst;
        inst.data     = *sd;
        inst.fromCore = true;  // ← set ตรงนี้
        m_skills.push_back(inst);

        // passive ไม่ใส่ hotbar
        if (sd->type == SkillType::Passive)
            return true;


        for (int i = 0; i < 9; ++i)
            if (m_hotbar[i].empty())
            { m_hotbar[i] = skillId; return true; }

            return false;
        }
       //if (!findSkill(skillId))
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
