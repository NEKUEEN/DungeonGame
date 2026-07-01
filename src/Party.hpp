#pragma once
#include "NPC.hpp"
#include <vector>
#include <deque>
#include <string>
#include <memory>

// ตำแหน่งที่ leader เคยอยู่ (เก็บใน trail)
struct TrailPoint {
    int  col        = 0;
    int  row        = 0;
    bool facingLeft = false;
};

// ============================================================
//  Party  –  Singleton Party Management System
// ============================================================
class Party
{
public:
    static Party& instance()
    {
        static Party s_inst;
        return s_inst;
    }

    // Max party size is 5-6 (including player)
    static constexpr int MAX_SIZE = 6;

    Party(const Party&) = delete;
    void operator=(const Party&) = delete;

    // Member Management
    bool addMember(std::shared_ptr<NPC> npc);
    bool removeMember(const std::string& npcId);
    std::shared_ptr<NPC> getMember(int idx) const;
    std::shared_ptr<NPC> getMemberById(const std::string& id) const;

    // Getters
    int  getSize()       const { return m_members.size(); }
    int  size()          const { return m_members.size(); }  // Alias for STL-like interface
    int  getMaxSize()    const { return MAX_SIZE; }
    bool isFull()        const { return m_members.size() >= MAX_SIZE; }
    const std::vector<std::shared_ptr<NPC>>& getMembers() const { return m_members; }
    std::vector<std::shared_ptr<NPC>>& getMembers() { return m_members; }

    // Party stats summary
    int getTotalAttack()  const;
    int getTotalDefense() const;

    // Clear party
    void clear() { m_members.clear(); m_trail.clear(); }

    // Check if NPC is in party
    bool hasMember(const std::string& npcId) const;

    // ── Trail system: leader บันทึกทุกก้าวที่ขยับ ──
    // companion i ใช้ trail[i * TRAIL_SPACING]
    static constexpr int TRAIL_SPACING = 1;

    void recordLeaderStep(int col, int row, bool facingLeft);
    const TrailPoint* getTrailPoint(int idx) const;
    void clearTrail() { m_trail.clear(); }

private:
    Party() = default;
    std::vector<std::shared_ptr<NPC>> m_members;
    std::deque<TrailPoint>            m_trail;   // ประวัติตำแหน่ง leader
};
