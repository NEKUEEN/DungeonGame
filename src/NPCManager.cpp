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
    // ถ้าอยากกรอง fog ด้วยก็เพิ่มตรงนี้ได้
    int col = npc->getCol();
    int row = npc->getRow();
    float px = (float)(col * m_tileSize);
    float py = (float)(row * m_tileSize);
    float ts = (float)m_tileSize;

    // Try to load sprite from TextureManager
    // ตัด .png ออกจาก sprite name
    std::string key = npc->getSprite();
    if (key.size() > 4) key = key.substr(0, key.size() - 4);
    const sf::Texture* tex = TextureManager::instance().get(key);
    if (tex)
    {
        sf::Sprite sprite(*tex);
        auto texSz = tex->getSize();
        float scale = ts / std::max(1.f, (float)texSz.x);

        bool facingLeft = npc->getFacingLeft();
        float scaleX = facingLeft ? -scale : scale;
        sprite.setScale({scaleX, scale});

        // ปรับ origin ให้ flip ไม่หลุดตำแหน่ง (เหมือน Player)
        auto bounds = sprite.getLocalBounds();
        float originX = facingLeft
            ? (bounds.position.x + bounds.size.x)
            : bounds.position.x;
        sprite.setOrigin({originX, bounds.position.y});
        sprite.setPosition({px, py});
        window.draw(sprite);
    }
    else
    {
        // Fallback shape with color based on type
        sf::Color color = sf::Color::White;
        switch (npc->getType())
        {
            case NPCType::Companion: color = sf::Color(100, 200, 100); break;
            case NPCType::Merchant:  color = sf::Color(200, 180, 80);  break;
            case NPCType::QuestGiver: color = sf::Color(150, 100, 200); break;
        }

        float sz = 0.7f;
        sf::RectangleShape body({ts * sz, ts * (sz + 0.05f)});
        body.setFillColor(color);
        body.setOutlineColor(sf::Color::White);
        body.setOutlineThickness(1.f);
        body.setPosition({px + ts * (1.f - sz) / 2.f, py + ts * 0.08f});
        window.draw(body);
    }

    // Render HP bar for Companions
    if (npc->getType() == NPCType::Companion && npc->getMaxHP() > 0)
        renderHPBar(window, npc);

    // Render name label
    // (would need font reference - skip for now)
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
