#include "Player.hpp"
#include <algorithm>
#include <iostream>

Player::Player(int startCol, int startRow, int tileSize)
    : m_col(startCol), m_row(startRow), m_tileSize(tileSize)
{
    m_stats.hp = m_stats.maxHp;  // เริ่มต้น hp = maxHp

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

    // เพิ่ม base stats ตอน level up
    m_stats.maxHp    += 30;
    m_stats.maxAtk   += 10;
    m_stats.maxDef   += 1;
    m_stats.maxDodge += 1;

    // regen hp เต็ม
    m_stats.hp = m_stats.maxHp;

    // mentality เพิ่มเล็กน้อย
    m_stats.maxMentality += 10;
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
    float bw=ts-4.f, bh=4.f, bx=px+2.f, by=py+ts-6.f;

    // background
    sf::RectangleShape bg({bw,bh});
    bg.setFillColor(sf::Color(80,0,0));
    bg.setPosition({bx,by});
    window.draw(bg);

    // hp bar — ratio vs maxHp
    float r = std::clamp((float)m_stats.hp / (float)m_stats.maxHp, 0.f, 1.f);
    sf::Color c = r>0.5f ? sf::Color(50,200,50) :
                  r>0.25f? sf::Color(220,180,0) : sf::Color(220,50,50);
    sf::RectangleShape bar({bw*r,bh});
    bar.setFillColor(c);
    bar.setPosition({bx,by});
    window.draw(bar);

    // mentality drain indicator — แถบม่วงเล็กๆ ด้านบน hp bar
    if (m_stats.hpDepleted)
    {
        float mRatio = std::clamp((float)m_stats.maxMentality / 20.f, 0.f, 1.f);
        sf::RectangleShape mBar({bw*mRatio, 2.f});
        mBar.setFillColor(sf::Color(180,80,220,200));
        mBar.setPosition({bx, by-3.f});
        window.draw(mBar);
    }
}

void Player::onTurnPassed()
{
    // ── Hunger drain (ทุก 100 turns) ──
    m_stats.hungerTimer++;
    if (m_stats.hungerTimer >= 100)
    {
        m_stats.hungerTimer = 0;
        if (m_stats.hunger > 0) m_stats.hunger--;
    }

    // ── HP regen (ทุก 7 turns ถ้าไม่หิวและ hp ไม่เต็ม และไม่อยู่ในสถานะ depleted) ──
    m_stats.regenTimer++;
    if (m_stats.regenTimer >= 7)
    {
        m_stats.regenTimer = 0;
        if (m_stats.hunger >= 20 && m_stats.hp < m_stats.maxHp && !m_stats.hpDepleted)
            m_stats.hp++;
    }

    // ── Hunger starve damage ──
    if (m_stats.hunger < 20 && m_stats.hp > 0)
        m_stats.hp--;

    // ── ตรวจ hp ลงถึง 0 ──
    if (m_stats.hp <= 0)
    {
        m_stats.hp = 0;
        m_stats.hpDepleted = true;
        // drain mentality จัดการใน Game::drainMentality() เพื่อให้ log ได้
    }
}
