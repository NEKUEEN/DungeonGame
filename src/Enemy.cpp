#include "Enemy.hpp"
#include "TileMap.hpp"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <iostream>

Enemy::Enemy(EnemyType type,EnemyRank rank,int col,int row,int tileSize,int floor)
    : m_type(type),m_rank(rank),m_col(col),m_row(row),m_tileSize(tileSize)
{
    initStats(floor);

    // โหลด sprite
    std::string path = spritePath(type);
    if (m_texture.loadFromFile(path))
    {
        m_sprite = new sf::Sprite(m_texture);
        auto sz  = m_texture.getSize();
        float scale = (float)tileSize / std::max(sz.x, sz.y);
        m_sprite->setScale({scale,scale});
        m_hasSprite = true;
    }
    else std::cerr<<"[Enemy] "<<path<<" not found\n";
}

std::string Enemy::spritePath(EnemyType type)
{
    switch(type)
    {
        case EnemyType::Rat:    return "assets/textures/rat.png";
        case EnemyType::Goblin: return "assets/textures/goblin.png";
        case EnemyType::Orc:    return "assets/textures/orc.png";
        default:                return "assets/textures/rat.png";
    }
}

void Enemy::initStats(int floor)
{
    switch(m_type)
    {
        case EnemyType::Rat:    m_maxHp=6;  m_attack=3; m_defense=0; m_exp=1; break;
        case EnemyType::Goblin: m_maxHp=12; m_attack=5; m_defense=1; m_exp=1; break;
        case EnemyType::Orc:    m_maxHp=22; m_attack=8; m_defense=3; m_exp=1; break;
    }
    float scale=1.f+(floor-1)*0.1f;
    m_maxHp=(int)(m_maxHp*scale); m_attack=(int)(m_attack*scale);
    switch(m_rank)
    {
        case EnemyRank::Normal: m_exp=1; break;
        case EnemyRank::Elite:
            m_maxHp=(int)(m_maxHp*1.8f); m_attack=(int)(m_attack*1.5f);
            m_defense+=2; m_exp=2; break;
        case EnemyRank::Boss:
            m_maxHp=(int)(m_maxHp*3.f); m_attack=(int)(m_attack*2.f);
            m_defense+=5; m_exp=3; break;
    }
    m_hp=m_maxHp;
}

bool Enemy::updateAI(int pc,int pr,const TileMap& map,const std::vector<Enemy*>& others)
{
    int dx=pc-m_col,dy=pr-m_row;
    if (std::sqrt((float)(dx*dx+dy*dy))>8.f) return false;
    int mc=m_col,mr=m_row;
    if (std::abs(dx)>=std::abs(dy)&&dx!=0) mc+=(dx>0)?1:-1;
    else if (dy!=0) mr+=(dy>0)?1:-1;
    if (!map.isWalkable(mc,mr)){mc=m_col;mr=m_row;if(dy!=0)mr+=(dy>0)?1:-1;else if(dx!=0)mc+=(dx>0)?1:-1;}
    for (auto*o:others) if(o!=this&&o->m_col==mc&&o->m_row==mr)return false;
    if (mc==pc&&mr==pr) return false;
    if (map.isWalkable(mc,mr)){m_col=mc;m_row=mr;return true;}
    return false;
}

void Enemy::render(sf::RenderWindow& window)
{
    if (isDead()) return;
    float px=(float)(m_col*m_tileSize),py=(float)(m_row*m_tileSize),ts=(float)m_tileSize;

    if (m_hasSprite && m_sprite)
    {
        // Elite/Boss มี color tint
        if      (m_rank==EnemyRank::Boss)  m_sprite->setColor(sf::Color(255,200,80));
        else if (m_rank==EnemyRank::Elite) m_sprite->setColor(sf::Color(200,100,255));
        else                               m_sprite->setColor(sf::Color::White);

        m_sprite->setPosition({px,py});
        window.draw(*m_sprite);
    }
    else
    {
        float sz=m_rank==EnemyRank::Boss?0.85f:m_rank==EnemyRank::Elite?0.72f:0.6f;
        sf::RectangleShape body({ts*sz,ts*(sz+0.05f)});
        body.setFillColor(bodyColor());
        sf::Color oc=m_rank==EnemyRank::Boss?sf::Color(255,180,0):m_rank==EnemyRank::Elite?sf::Color(180,80,220):sf::Color(255,255,255,80);
        body.setOutlineColor(oc); body.setOutlineThickness(m_rank==EnemyRank::Normal?1.f:2.f);
        body.setPosition({px+ts*(1.f-sz)/2.f,py+ts*0.08f}); window.draw(body);
    }
    drawHPBar(window);
}

void Enemy::drawHPBar(sf::RenderWindow& window)
{
    float px=(float)(m_col*m_tileSize),py=(float)(m_row*m_tileSize),ts=(float)m_tileSize;
    float bw=ts-4.f,bh=3.f,bx=px+2.f,by=py+ts-5.f;
    sf::RectangleShape bg({bw,bh}); bg.setFillColor(sf::Color(60,0,0)); bg.setPosition({bx,by}); window.draw(bg);
    float r=std::clamp((float)m_hp/m_maxHp,0.f,1.f);
    sf::Color c=m_rank==EnemyRank::Boss?sf::Color(255,150,0):m_rank==EnemyRank::Elite?sf::Color(180,80,220):sf::Color(200,60,60);
    sf::RectangleShape bar({bw*r,bh}); bar.setFillColor(c); bar.setPosition({bx,by}); window.draw(bar);
}

sf::Color Enemy::bodyColor() const
{
    switch(m_type){case EnemyType::Rat:return sf::Color(130,100,80);case EnemyType::Goblin:return sf::Color(80,140,80);default:return sf::Color(100,80,160);}
}

std::string Enemy::getName() const
{
    std::string p=m_rank==EnemyRank::Boss?"[BOSS] ":m_rank==EnemyRank::Elite?"[Elite] ":"";
    switch(m_type){case EnemyType::Rat:return p+"Rat";case EnemyType::Goblin:return p+"Goblin";default:return p+"Orc";}
}
