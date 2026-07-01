#include "Party.hpp"
#include <algorithm>
#include <iostream>

bool Party::addMember(std::shared_ptr<NPC> npc)
{
    if (!npc) return false;
    if (isFull())
    {
        std::cerr << "[Party] Party is full! Max size: " << MAX_SIZE << "\n";
        return false;
    }
    if (hasMember(npc->getId()))
    {
        std::cerr << "[Party] " << npc->getName() << " is already in party!\n";
        return false;
    }

    m_members.push_back(npc);
    std::cout << "[Party] " << npc->getName() << " joined the party!\n";
    return true;
}

bool Party::removeMember(const std::string& npcId)
{
    auto it = std::find_if(m_members.begin(), m_members.end(),
        [&npcId](const std::shared_ptr<NPC>& m) { return m->getId() == npcId; });

    if (it == m_members.end())
    {
        std::cerr << "[Party] Member not found: " << npcId << "\n";
        return false;
    }

    std::cout << "[Party] " << (*it)->getName() << " left the party!\n";
    m_members.erase(it);
    return true;
}

std::shared_ptr<NPC> Party::getMember(int idx) const
{
    if (idx < 0 || idx >= (int)m_members.size()) return nullptr;
    return m_members[idx];
}

std::shared_ptr<NPC> Party::getMemberById(const std::string& id) const
{
    auto it = std::find_if(m_members.begin(), m_members.end(),
        [&id](const std::shared_ptr<NPC>& m) { return m->getId() == id; });
    return (it != m_members.end()) ? *it : nullptr;
}

void Party::recordLeaderStep(int col, int row, bool facingLeft)
{
    m_trail.push_front({col, row, facingLeft});

    // เก็บ trail แค่เท่าที่จำเป็น
    size_t maxNeeded = (m_members.size() + 1) * TRAIL_SPACING + 1;
    while (m_trail.size() > maxNeeded)
        m_trail.pop_back();
}

const TrailPoint* Party::getTrailPoint(int idx) const
{
    if (idx < 0 || idx >= (int)m_trail.size()) return nullptr;
    return &m_trail[idx];
}

bool Party::hasMember(const std::string& npcId) const
{
    return std::find_if(m_members.begin(), m_members.end(),
        [&npcId](const std::shared_ptr<NPC>& m) { return m->getId() == npcId; })
        != m_members.end();
}

int Party::getTotalAttack() const
{
    int total = 0;
    for (const auto& m : m_members)
        total += m->getAttack();
    return total;
}

int Party::getTotalDefense() const
{
    int total = 0;
    for (const auto& m : m_members)
        total += m->getDefense();
    return total;
}
