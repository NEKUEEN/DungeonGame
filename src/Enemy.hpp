#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <optional>
#include <queue>
#include <unordered_map>
#include "Skill.hpp"

enum class EnemyRank { Normal, Elite, Boss };
enum class EnemyAIType { Melee, Ranged, Caster, Coward };

class Enemy
{
public:
    Enemy(const std::string& monsterId, int col, int row, int tileSize, int dungeonFloor);
    ~Enemy() { delete m_sprite; }

    bool updateAI(int playerCol, int playerRow,
                  const class TileMap& map,
                  const std::vector<Enemy*>& others);

    void render(sf::RenderWindow& window);

    int  getAttack()  const { return m_attack; }
    int  getDefense() const { return m_defense; }
    int  getExp()     const { return m_exp; }
    void takeDamage(int dmg) { m_hp -= dmg; }
    bool isDead()     const { return m_hp <= 0; }

    int         getCol()    const { return m_col; }
    int         getRow()    const { return m_row; }
    EnemyRank   getRank()   const { return m_rank; }
    EnemyAIType getAIType() const { return m_aiType; }
    std::string getName()   const { return m_name; }
    std::string getFamily() const { return m_family; }
    std::string getId()     const { return m_id; }
    bool        isAlerted() const { return m_alerted; }
    int         getPreferredRange() const { return m_preferredRange; }

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

    // A* — คืน next step ที่ควรเดิน, {-1,-1} ถ้าไปไม่ได้
    std::pair<int,int> astar(int fromC, int fromR, int toC, int toR,
                              const TileMap& map) const;

    bool stepToward(int toC, int toR, const TileMap& map,
                    const std::vector<Enemy*>& others);

    bool isTileOccupied(int c, int r, const std::vector<Enemy*>& others) const;

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

    bool m_alerted      = false;
    int  m_lastKnownCol = -1;
    int  m_lastKnownRow = -1;
    bool m_searching    = false;  // ถึง lastKnown แล้วยังไม่เจอ → วนหา
    int  m_searchTimer  = 0;

    std::vector<SkillInstance> m_skills;
    std::optional<PendingSkill> m_pendingSkill;

    sf::Texture m_texture;
    sf::Sprite* m_sprite    = nullptr;
    bool        m_hasSprite = false;
};
