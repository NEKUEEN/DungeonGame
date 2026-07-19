#include "Player.hpp"
#include "SkillDB.hpp"
#include <algorithm>
#include <iostream>
#include "TextureManager.hpp"

Player::Player(int startCol, int startRow, int tileSize)
    : m_col(startCol), m_row(startRow), m_tileSize(tileSize)
{
    m_stats.hp = m_stats.maxHp;

    if (m_texture.loadFromFile("assets/textures/player.png"))
    {
        m_sprite = new sf::Sprite(m_texture);
        sf::Vector2u sz = m_texture.getSize();
        m_sprite->setScale({(float)m_tileSize/sz.x, (float)m_tileSize/sz.y});
        m_hasSprite = true;
    }

    // โหลด skills จาก SkillDB เฉพาะที่มี hotbar_slot >= 0
    for (const auto& sd : SkillDB::instance().getAll())
    {
        if (sd.hotbarSlot >= 0 && sd.hotbarSlot < 9)
        {
            SkillInstance inst;
            inst.data     = sd;
            inst.fromCore = false;
            m_skills.push_back(inst);
            //m_hotbar[sd.hotbarSlot] = sd.id;
        }
    }
}

bool Player::tryMove(int dc, int dr, const TileMap& map)
{
    int nc = m_col + dc, nr = m_row + dr;
    if (!map.isWalkable(nc, nr)) return false;
    m_col = nc; m_row = nr;
    return true;
}

bool Player::addExp(int amount)
{
    m_stats.exp += amount;
    if (m_stats.exp >= m_stats.expToNext) { levelUp(); return true; }
    return false;
}

void Player::levelUp()
{
    m_stats.exp -= m_stats.expToNext;
    m_stats.expToNext *= 2;
    m_stats.level++;

    m_stats.maxHp      += 30;
    m_stats.baseMaxHp  += 30;
    m_stats.maxAtk     += 10;
    m_stats.maxDef     += 1;
    m_stats.maxDodge   += 1;
    m_stats.hp          = m_stats.maxHp;
    m_stats.maxMentality += 10;
}

void Player::render(sf::RenderWindow& window)
{
    float px = (float)(m_col * m_tileSize);
    float py = (float)(m_row * m_tileSize);
    float ts = (float)m_tileSize;

    if (m_hasSprite && m_sprite)
    {
        auto bounds = m_sprite->getLocalBounds();
        float scaleX = m_facingLeft ? -1.f : 1.f;
        m_sprite->setScale({scaleX * std::abs(m_sprite->getScale().x), m_sprite->getScale().y});

        // ปรับ origin ให้ flip ไม่หลุดตำแหน่ง
        float originX = m_facingLeft ? (bounds.position.x + bounds.size.x) : bounds.position.x;
        m_sprite->setOrigin({originX, bounds.position.y});
        //m_sprite->setPosition({px + (m_facingLeft ? ts : 0.f), py});
        m_sprite->setPosition({px, py});
        window.draw(*m_sprite);
    }
    else
    {
        sf::CircleShape body(ts * 0.35f);
        body.setFillColor(sf::Color(70, 130, 200));
        body.setOutlineColor(sf::Color(200, 220, 255));
        body.setOutlineThickness(1.5f);
        body.setPosition({px + ts*0.15f, py + ts*0.1f});
        window.draw(body);
    }

    drawHPBar(window);
}

void Player::drawHPBar(sf::RenderWindow& window)
{
    float px = (float)(m_col * m_tileSize);
    float py = (float)(m_row * m_tileSize);
    float ts = (float)m_tileSize;
    float bw = ts - 4.f, bh = 4.f, bx = px + 2.f, by = py + ts - 6.f;

    sf::RectangleShape bg({bw, bh});
    bg.setFillColor(sf::Color(80, 0, 0));
    bg.setPosition({bx, by});
    window.draw(bg);

    float r = std::clamp((float)m_stats.hp / (float)m_stats.maxHp, 0.f, 1.f);
    sf::Color c = r > 0.5f ? sf::Color(50, 200, 50) :
                  r > 0.25f ? sf::Color(220, 180, 0) : sf::Color(220, 50, 50);

    sf::RectangleShape bar({bw * r, bh});
    bar.setFillColor(c);
    bar.setPosition({bx, by});
    window.draw(bar);


}

void Player::tickStatusEffects(int& hpDelta, std::string& effectName)
{
    hpDelta = 0;
    effectName = "";
    auto& effects = m_stats.statusEffects;
    for (auto& se : effects)
    {
        switch (se.type)
        {
            case StatusType::Bleed:
                hpDelta -= se.power;
                effectName = "bleed";
                break;
            case StatusType::Poison:
                hpDelta -= se.power;
                effectName = "poison";
                break;
            case StatusType::Burn:
                hpDelta -= se.power;
                effectName = "burn";
                break;
            case StatusType::Regen:
                hpDelta += se.power;
                break;
            default: break;
        }
    }
    m_stats.hp += hpDelta;
    m_stats.hp  = std::clamp(m_stats.hp, 0, m_stats.maxHp);
    effects.erase(
        std::remove_if(effects.begin(), effects.end(),
            [](StatusEffect& se){ return !se.tick(); }),
        effects.end());
}

void Player::onTurnPassed()
{
    // ── Tick player status effects ──
    int hpDelta = 0; std::string unused;
    tickStatusEffects(hpDelta, unused);  // Game.cpp จะเรียกเองและ log เอง
    // ลบ block เดิมที่ทำ loop effects ออก

    // ... ส่วนที่เหลือเหมือนเดิม
    // ── tick skills (cooldown + buff duration) ──
    for (auto& sk : m_skills)
        sk.tick();

    // ── Mana regen (ทุก 7 turns) ──
    if (m_stats.mana < m_stats.maxMana)
        m_stats.mana = std::min(m_stats.maxMana, m_stats.mana + m_stats.manaRegen);

    // ── Hunger drain (ทุก 100 turns) ──
    m_stats.hungerTimer++;
    if (m_stats.hungerTimer >= 5)
    {
        m_stats.hungerTimer = 0;
        if (m_stats.hunger > 0) m_stats.hunger--;
    }

    // ── HP regen (ทุก 7 turns) ──
    m_stats.regenTimer++;
    if (m_stats.regenTimer >= 7)
    {
        m_stats.regenTimer = 0;
        if (m_stats.hunger >= 20 && m_stats.hp < m_stats.maxHp)
            m_stats.hp++;
    }

    // ── Hunger starve damage ──
    if (m_stats.hunger < 20 && m_stats.hp > 0)
        m_stats.hp--;

    // AP reset ทำใน Game::recalcSpeed() แล้ว
}
void Player::setSprite(const std::string& textureKey)
{
    const sf::Texture* tex = TextureManager::instance().get(textureKey);
    if (!tex) { std::cerr << "[Player] setSprite: key not found: " << textureKey << "\n"; return; }

    m_texture = *tex;

    if (!m_sprite)
        m_sprite = new sf::Sprite(m_texture);
    else
        m_sprite->setTexture(m_texture, true);

    sf::Vector2u sz = m_texture.getSize();
    m_sprite->setScale({(float)m_tileSize / sz.x, (float)m_tileSize / sz.y});
    m_hasSprite = true;
}
