#include "NPCManager.hpp"
#include "TextureManager.hpp"
#include <algorithm>
#include <iostream>

void NPCManager::add(std::shared_ptr<NPC> npc)
{
    if (!npc) return;
    if (getNPC(npc->getId()))
    {
        std::cerr << "[NPCManager] NPC already exists: " << npc->getId() << "\n";
        return;
    }
    m_npcs.push_back(npc);
    std::cout << "[NPCManager] Added NPC: " << npc->getName() << "\n";
}

void NPCManager::remove(const std::string& npcId)
{
    auto it = std::find_if(m_npcs.begin(), m_npcs.end(),
        [&npcId](const std::shared_ptr<NPC>& n) { return n->getId() == npcId; });

    if (it != m_npcs.end())
    {
        std::cout << "[NPCManager] Removed NPC: " << (*it)->getName() << "\n";
        m_npcs.erase(it);
    }
}

std::shared_ptr<NPC> NPCManager::getNPC(const std::string& npcId)
{
    auto it = std::find_if(m_npcs.begin(), m_npcs.end(),
        [&npcId](const std::shared_ptr<NPC>& n) { return n->getId() == npcId; });
    return (it != m_npcs.end()) ? *it : nullptr;
}

std::shared_ptr<NPC> NPCManager::getNPCAt(int col, int row)
{
    for (auto& npc : m_npcs)
    {
        if (npc->getCol() == col && npc->getRow() == row)
            return npc;
    }
    return nullptr;
}

void NPCManager::render(sf::RenderWindow& window)
{
    for (auto& npc : m_npcs)
        renderNPC(window, npc);
}

void NPCManager::renderNPC(sf::RenderWindow& window, std::shared_ptr<NPC> npc)
{
    if (!npc) return;

    float px = (float)(npc->getCol() * m_tileSize);
    float py = (float)(npc->getRow() * m_tileSize);
    float ts = (float)m_tileSize;

    std::string key = npc->getSprite();
    if (key.size() > 4) key = key.substr(0, key.size() - 4);

    // ดึง/สร้าง sprite จาก cache
    auto it = m_spriteCache.find(key);
    if (it == m_spriteCache.end())
    {
        const sf::Texture* tex = TextureManager::instance().get(key);
        if (!tex) { /* fallback เหมือนเดิม */ return; }

        sf::Sprite spr(*tex);
        float scale = ts / std::max(1.f, (float)tex->getSize().x);
        spr.setScale({scale, scale});
        auto result = m_spriteCache.emplace(key, std::move(spr));
        it = result.first;
    }

    sf::Sprite& sprite = it->second;

    bool facingLeft = npc->getFacingLeft();
    auto bounds = sprite.getLocalBounds();
    float baseScale = ts / std::max(1.f, (float)bounds.size.x);
    sprite.setScale({facingLeft ? -baseScale : baseScale, baseScale});
    float originX = facingLeft ? (bounds.position.x + bounds.size.x) : bounds.position.x;
    sprite.setOrigin({originX, bounds.position.y});
    sprite.setPosition({px, py});
    window.draw(sprite);

    if (npc->getType() == NPCType::Companion && npc->getMaxHP() > 0)
        renderHPBar(window, npc);
}

void NPCManager::renderHPBar(sf::RenderWindow& window, std::shared_ptr<NPC> npc)
{
    if (!npc) return;

    float px = (float)(npc->getCol() * m_tileSize);
    float py = (float)(npc->getRow() * m_tileSize);
    float ts = (float)m_tileSize;
    float bw = ts - 4.f, bh = 3.f;
    float bx = px + 2.f, by = py + ts - 5.f;

    // Background
    sf::RectangleShape bg({bw, bh});
    bg.setFillColor(sf::Color(60, 0, 0));
    bg.setPosition({bx, by});
    window.draw(bg);

    // HP bar
    float ratio = (float)npc->getHP() / npc->getMaxHP();
    sf::RectangleShape bar({bw * ratio, bh});
    bar.setFillColor(sf::Color(100, 200, 100));
    bar.setPosition({bx, by});
    window.draw(bar);
}

void NPCManager::interact(std::shared_ptr<NPC> npc)
{
    if (!npc) return;

    std::cout << "[NPCManager] Interacting with: " << npc->getName() << "\n";
    std::cout << "  Desc: " << npc->getDesc() << "\n";

    if (npc->getType() == NPCType::Companion)
    {
        std::cout << "  Role: ";
        switch (npc->getRole())
        {
            case NPCRole::Warrior: std::cout << "Warrior"; break;
            case NPCRole::Mage: std::cout << "Mage"; break;
            case NPCRole::Healer: std::cout << "Healer"; break;
            case NPCRole::Rogue: std::cout << "Rogue"; break;
        }
        std::cout << " (Lvl " << npc->getLevel() << ")\n";
    }
    else if (npc->getType() == NPCType::Merchant)
    {
        std::cout << "  Merchant Items: ";
        auto items = npc->getMerchantItems();
        for (const auto& item : items)
            std::cout << item << ", ";
        std::cout << "\n";
    }
}
