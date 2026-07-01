#include "NPC.hpp"
#include <algorithm>
#include <iostream>

// Constructor for non-combat NPCs (Merchant, Quest Giver)
NPC::NPC(const std::string& id, const std::string& name,
         NPCType type, const std::string& desc)
    : m_id(id), m_name(name), m_type(type), m_desc(desc)
{
}

// Constructor for Companion NPCs (with stats)
NPC::NPC(const std::string& id, const std::string& name,
         NPCRole role, int level, const std::string& desc)
    : m_id(id), m_name(name), m_type(NPCType::Companion),
      m_role(role), m_level(level), m_desc(desc)
{
    computeStats();
}

void NPC::computeStats()
{
    // Base stats
    int baseHp = 30, baseAtk = 5, baseDef = 2;

    // Adjust per role
    switch (m_role)
    {
        case NPCRole::Warrior:
            baseHp  += 20;
            baseAtk += 5;
            baseDef += 5;
            break;
        case NPCRole::Mage:
            baseHp  += 5;
            baseAtk += 10;  // spells are more powerful
            baseDef -= 2;
            break;
        case NPCRole::Healer:
            baseHp  += 15;
            baseAtk += 0;
            baseDef += 2;
            break;
        case NPCRole::Rogue:
            baseHp  += 0;
            baseAtk += 8;
            baseDef += 0;
            break;
    }

    // Scale per level
    float scale = 1.f + (m_level - 1) * 0.15f;
    m_maxHp  = (int)(baseHp * scale);
    m_hp     = m_maxHp;
    m_attack = (int)(baseAtk * scale);
    m_defense = (int)(baseDef * scale);
    m_expToLevel = 100 + (m_level - 1) * 50;
}

void NPC::addExp(int exp)
{
    if (m_type != NPCType::Companion) return;
    m_exp += exp;
    if (m_exp >= m_expToLevel)
        levelUp();
}

void NPC::levelUp()
{
    if (m_type != NPCType::Companion) return;
    m_level++;
    m_exp = 0;
    computeStats();
    std::cout << "[NPC] " << m_name << " leveled up to " << m_level << "!\n";
}

void NPC::setMerchantItems(const std::vector<std::string>& items)
{
    if (m_type != NPCType::Merchant) return;
    m_merchantItems = items;
}
