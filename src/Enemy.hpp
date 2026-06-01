#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

enum class EnemyType { Rat, Goblin, Orc };
enum class EnemyRank { Normal, Elite, Boss };

class Enemy
{
public:
    Enemy(EnemyType type, EnemyRank rank, int col, int row, int tileSize, int dungeonFloor);

    bool updateAI(int playerCol,int playerRow,const class TileMap& map,
                  const std::vector<Enemy*>& others);
    void render(sf::RenderWindow& window);

    int  getAttack()  const { return m_attack; }
    int  getDefense() const { return m_defense; }
    int  getExp()     const { return m_exp; }
    void takeDamage(int dmg) { m_hp-=dmg; }
    bool isDead() const { return m_hp<=0; }

    int getCol() const { return m_col; }
    int getRow() const { return m_row; }
    EnemyType getType() const { return m_type; }
    EnemyRank getRank() const { return m_rank; }
    std::string getName() const;

private:
    void initStats(int floor);
    void drawHPBar(sf::RenderWindow& window);
    sf::Color bodyColor() const;
    static std::string spritePath(EnemyType type);

    EnemyType m_type;
    EnemyRank m_rank;
    int m_col,m_row,m_tileSize;
    int m_hp,m_maxHp,m_attack,m_defense,m_exp;

    sf::Texture m_texture;
    sf::Sprite* m_sprite    = nullptr;
    bool        m_hasSprite = false;
};
