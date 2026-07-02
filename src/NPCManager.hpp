#pragma once
#include "NPC.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <SFML/Graphics.hpp>

// ============================================================
//  NPCManager  –  Manage NPCs on the map
// ============================================================
class NPCManager
{
public:
    NPCManager(int tileSize = 32) : m_tileSize(tileSize) {}
    ~NPCManager() = default;

    // Lifecycle
    void clear() { m_npcs.clear(); }
    void add(std::shared_ptr<NPC> npc);
    void remove(const std::string& npcId);

    // Getters
    std::shared_ptr<NPC> getNPC(const std::string& npcId);
    std::shared_ptr<NPC> getNPCAt(int col, int row);
    const std::vector<std::shared_ptr<NPC>>& getAll() const { return m_npcs; }
    int  count() const { return m_npcs.size(); }

    // Rendering
    void render(sf::RenderWindow& window);
    void renderNPC(sf::RenderWindow& window, std::shared_ptr<NPC> npc);  // public — ใช้วาด companion ใน party ด้วย

    // Interaction
    void interact(std::shared_ptr<NPC> npc);

private:
    std::vector<std::shared_ptr<NPC>> m_npcs;
    int m_tileSize;
    // cache sprite ต่อ NPC id — สร้างครั้งเดียว
    std::unordered_map<std::string, sf::Sprite> m_spriteCache;
    void renderHPBar(sf::RenderWindow& window, std::shared_ptr<NPC> npc);
};
