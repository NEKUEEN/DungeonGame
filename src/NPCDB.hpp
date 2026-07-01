#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include "lib/json.hpp"
#include "NPC.hpp"

// ============================================================
//  NPCData  –  Template Data for NPCs from JSON
// ============================================================
struct NPCData
{
    std::string id;
    std::string name;
    std::string type;        // "Companion", "Merchant", "QuestGiver"
    std::string role;        // "Warrior", "Mage", "Healer", "Rogue" (for Companion)
    int         level = 1;   // for Companion
    std::string desc;
    std::string sprite;
    int         recruitCost = 0;  // cost to recruit (if applicable)
    std::vector<std::string> items;  // merchant items
};

// ============================================================
//  NPCDB  –  Singleton NPC Database
// ============================================================
class NPCDB
{
public:
    static NPCDB& instance()
    {
        static NPCDB s_inst;
        return s_inst;
    }

    NPCDB(const NPCDB&) = delete;
    void operator=(const NPCDB&) = delete;

    // Load NPCs from JSON
    bool load(const std::string& path);

    // Get NPC template data
    const NPCData* get(const std::string& npcId) const;

    // Create NPC instance from template
    std::shared_ptr<NPC> createNPC(const std::string& npcId);

    // Get all NPCs
    const std::map<std::string, NPCData>& getAll() const { return m_npcs; }

    // Count NPCs
    int count() const { return m_npcs.size(); }

private:
    NPCDB() = default;
    std::map<std::string, NPCData> m_npcs;

    // Helper to convert string to NPCRole
    NPCRole stringToRole(const std::string& roleStr) const;
};
