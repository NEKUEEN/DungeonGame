#include "Player.hpp"
#include <algorithm>
#include <iostream>

Player::Player(int startCol, int startRow, int tileSize)
    : m_col(startCol), m_row(startRow), m_tileSize(tileSize)
{
    m_stats.hp = m_stats.body;
    if (m_texture.loadFromFile("assets/textures/player.png"))
    {
        m_sprite = new sf::Sprite(m_texture);
        sf::Vector2u sz = m_texture.getSize();
        m_sprite->setScale({(float)m_tileSize/sz.x, (float)m_tileSize/sz.y});
        m_hasSprite = true;
    }
}

bool Player::tryMove(int dc, int dr, const TileMap& map)
{
    int nc=m_col+dc, nr=m_row+dr;
    if (!map.isWalkable(nc,nr)) return false;
    m_col=nc; m_row=nr; return true;
}

bool Player::addExp(int amount)
{
    m_stats.exp += amount;
    if (m_stats.exp >= m_stats.expToNext) { levelUp(); return true; }
    return false;
}

void Player::levelUp()
{
    m_stats.exp      -= m_stats.expToNext;
    m_stats.expToNext *= 2;
    m_stats.level++;
    m_stats.body        += 5;
    m_stats.mentality   += 3;
    m_stats.ability     += 1;
    m_stats.battleIndex += 10;
    m_stats.hp           = m_stats.body;
}

void Player::render(sf::RenderWindow& window)
{
    float px=(float)(m_col*m_tileSize), py=(float)(m_row*m_tileSize), ts=(float)m_tileSize;
    if (m_hasSprite && m_sprite)
    { m_sprite->setPosition({px,py}); window.draw(*m_sprite); }
    else
    {
        sf::CircleShape body(ts*0.35f);
        body.setFillColor(sf::Color(70,130,200));
        body.setOutlineColor(sf::Color(200,220,255));
        body.setOutlineThickness(1.5f);
        body.setPosition({px+ts*0.15f,py+ts*0.1f});
        window.draw(body);
    }
    drawHPBar(window);
}

void Player::drawHPBar(sf::RenderWindow& window)
{
    float px=(float)(m_col*m_tileSize), py=(float)(m_row*m_tileSize), ts=(float)m_tileSize;
    float bw=ts-4.f,bh=4.f,bx=px+2.f,by=py+ts-6.f;
    sf::RectangleShape bg({bw,bh}); bg.setFillColor(sf::Color(80,0,0)); bg.setPosition({bx,by}); window.draw(bg);
    float r=std::clamp((float)m_stats.hp/m_stats.body,0.f,1.f);
    sf::Color c=r>0.5f?sf::Color(50,200,50):r>0.25f?sf::Color(220,180,0):sf::Color(220,50,50);
    sf::RectangleShape bar({bw*r,bh}); bar.setFillColor(c); bar.setPosition({bx,by}); window.draw(bar);
}

void Player::onTurnPassed()
{
    // ความหิว: ลดทุก 100 turns
    m_stats.hungerTimer++;
    if (m_stats.hungerTimer >= 100)
    {
        m_stats.hungerTimer = 0;
        if (m_stats.hunger > 0) m_stats.hunger--;
    }

    // HP ลดถ้าหิวมาก
    if (m_stats.hunger < 20 && m_stats.hp > 1)
        m_stats.hp--;

    // HP regen: ทุก 7 turns ถ้าไม่หิวมาก
    m_stats.regenTimer++;
    if (m_stats.regenTimer >= 7)
    {
        m_stats.regenTimer = 0;
        if (m_stats.hunger >= 20 && m_stats.hp < m_stats.body)
            m_stats.hp++;
    }
}
