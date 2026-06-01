#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// rank ยังคงไว้เพื่อใช้ใน render (tint สี)
enum class EnemyRank { Normal, Elite, Boss };

class Enemy
{
public:
    // สร้างจาก monster id ใน MonsterDB
    Enemy(const std::string& monsterId, int col, int row,
          int tileSize, int dungeonFloor);
    ~Enemy() { delete m_sprite; }

    bool updateAI(int playerCol, int playerRow,
                  const class TileMap& map,
                  const std::vector<Enemy*>& others);
    void render(sf::RenderWindow& window);

    int  getAttack()  const { return m_attack; }
    int  getDefense() const { return m_defense; }
    int  getExp()     const { return m_exp; }
    void takeDamage(int dmg) { m_hp -= dmg; }
    bool isDead()     const  { return m_hp <= 0; }

    int         getCol()    const { return m_col; }
    int         getRow()    const { return m_row; }
    EnemyRank   getRank()   const { return m_rank; }
    std::string getName()   const { return m_name; }
    std::string getFamily() const { return m_family; }
    std::string getId()     const { return m_id; }

private:
    void loadSprite(const std::string& spriteName);
    void drawHPBar(sf::RenderWindow& window);
    sf::Color rankColor() const;

    // identity
    std::string m_id, m_name, m_family;
    EnemyRank   m_rank = EnemyRank::Normal;

    // position
    int m_col, m_row, m_tileSize;

    // stats
    int m_hp, m_maxHp, m_attack, m_defense, m_exp;

    // sprite
    sf::Texture m_texture;
    sf::Sprite* m_sprite    = nullptr;
    bool        m_hasSprite = false;
};
