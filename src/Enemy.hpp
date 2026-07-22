#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <optional>
#include <queue>
#include <unordered_map>
#include "Skill.hpp"
#include "StatusEffect.hpp"
#include "Item.hpp"

// ── เพิ่ม Rare เข้าไประหว่าง Normal และ Elite ──
enum class EnemyRank { Normal, Rare, Elite, Boss };
enum class EnemyAIType { Melee, Ranged, Caster, Coward };

class Enemy
{
public:
    Enemy(const std::string& monsterId, int col, int row, int tileSize, int dungeonFloor);
    ~Enemy() { delete m_sprite; }

    bool updateAI(int playerCol, int playerRow,
                  const class TileMap& map,
                  const std::vector<Enemy*>& others,
                  const std::vector<std::pair<int,int>>& blockedTiles = {});  // ← เพิ่ม param + default

    void render(sf::RenderWindow& window);

    int  getAttack()  const { return m_attack; }
    int  getDefense() const { return m_defense; }
    int  getExp()     const { return m_exp; }
    void takeDamage(int dmg) { m_hp -= dmg; }
    bool isDead()     const { return m_hp <= 0; }

    // ── Weapon damage-type resist (%, ลบ = แพ้ทาง) ──
    int getResist(DamageType type) const
    {
        switch (type)
        {
            case DamageType::Slash:  return m_slashResist;
            case DamageType::Pierce: return m_pierceResist;
            case DamageType::Blunt:  return m_bluntResist;
            case DamageType::Cleave: return m_cleaveResist;
        }
        return 0;
    }

    int         getCol()    const { return m_col; }
    int         getRow()    const { return m_row; }
    void        setPos(int col, int row) { m_col = col; m_row = row; }  // ← ใช้กับ knockback
    EnemyRank   getRank()   const { return m_rank; }
    EnemyAIType getAIType() const { return m_aiType; }
    std::string getName()   const { return m_name; }
    std::string getFamily() const { return m_family; }
    std::string getId()     const { return m_id; }
    bool        isAlerted() const { return m_alerted; }
    int         getPreferredRange() const { return m_preferredRange; }

    // ── Aut-based speed ──
    long long getNextActTime() const { return m_nextActTime; }
    void advanceActTime()
    {
        long long aut = m_moveAut;
        if (hasStatus(StatusType::Slow)) aut += m_moveAut / 2;  // ช้าลง 1.5x ไม่ว่าจะตีหรือเดินไล่
        m_nextActTime += aut;
    }
    int  getMoveAut()          const { return m_moveAut; }

    void applyStatus(const StatusEffect& se);
    void tickStatusEffects(int& hpDelta, std::string& effectName);  // คืน hp ที่เปลี่ยน
    const std::vector<StatusEffect>& getStatusEffects() const { return m_statusEffects; }
    bool hasStatus(StatusType type) const;

    std::vector<SkillInstance>&       getSkills()       { return m_skills; }
    const std::vector<SkillInstance>& getSkills() const { return m_skills; }
    SkillInstance* findSkill(const std::string& id);
    void tickSkills() { for (auto& sk : m_skills) sk.tick(); }

    struct PendingSkill { std::string skillId; int targetCol; int targetRow; };
    bool hasPendingSkill() const { return m_pendingSkill.has_value(); }
    PendingSkill consumePendingSkill() { auto ps = *m_pendingSkill; m_pendingSkill.reset(); return ps; }

private:
    void loadSprite(const std::string& spriteName);
    void drawHPBar(sf::RenderWindow& window);
    sf::Color rankColor() const;

    std::pair<int,int> astar(int fromC, int fromR, int toC, int toR,
                              const TileMap& map) const;

    bool stepToward(int toC, int toR, const TileMap& map,
                    const std::vector<Enemy*>& others,
                const std::vector<std::pair<int,int>>& blockedTiles);  // ← เพิ่ม);

    bool isTileOccupied(int c, int r, const std::vector<Enemy*>& others,
                const std::vector<std::pair<int,int>>& blockedTiles) const;

    bool hasLineOfSight(int x0, int y0, int x1, int y1,
                    const TileMap& map) const;

    std::string m_id, m_name, m_family;
    EnemyRank   m_rank   = EnemyRank::Normal;
    EnemyAIType m_aiType = EnemyAIType::Melee;

    int m_col, m_row, m_tileSize;
    int m_hp, m_maxHp, m_attack, m_defense, m_exp;

    int m_preferredRange = 1;
    int m_alertRange     = 8;
    int m_attackInterval = 1;
    int m_attackTimer    = 0;

    int       m_spd         = 0;    // เก็บไว้อ่านจาก JSON (ใช้แปลงเป็น moveAut)
    int       m_moveAut     = 100;  // Aut ต่อการเดิน 1 ก้าว
    long long m_nextActTime = 0;

    // ── Weapon damage-type resist ──
    int m_slashResist  = 0;
    int m_pierceResist = 0;
    int m_bluntResist  = 0;
    int m_cleaveResist = 0;

    bool m_alerted      = false;
    int  m_lastKnownCol = -1;
    int  m_lastKnownRow = -1;
    bool m_searching    = false;
    int  m_searchTimer  = 0;

    std::vector<SkillInstance> m_skills;
    std::optional<PendingSkill> m_pendingSkill;
    std::vector<StatusEffect> m_statusEffects;

    sf::Texture m_texture;
    sf::Sprite* m_sprite    = nullptr;
    bool        m_hasSprite = false;
};
