#pragma once
#include <string>
#include <vector>
#include <map>

enum class NPCType { Merchant, Companion, QuestGiver };
enum class NPCRole { Warrior, Mage, Healer, Rogue };

// ============================================================
//  NPC  –  NPC Data
// ============================================================
class NPC
{
public:
    // Constructor สำหรับ NPC ทั่วไป
    NPC(const std::string& id, const std::string& name,
        NPCType type, const std::string& desc = "");

    // Constructor สำหรับ Companion (มี stats)
    NPC(const std::string& id, const std::string& name,
        NPCRole role, int level, const std::string& desc = "");

    virtual ~NPC() = default;
    
    

    // Getters
    std::string getId()        const { return m_id; }
    std::string getName()      const { return m_name; }
    std::string getDesc()      const { return m_desc; }
    std::string getSprite()    const { return m_sprite; }
    void        setSprite(const std::string& s) { m_sprite = s; }
    NPCType     getType()      const { return m_type; }
    NPCRole     getRole()      const { return m_role; }
    int         getLevel()     const { return m_level; }

    // Companion Stats
    int  getHP()         const { return m_hp; }
    int  getMaxHP()      const { return m_maxHp; }
    int  getAttack()     const { return m_attack; }
    int  getDefense()    const { return m_defense; }
    int  getExp()        const { return m_exp; }
    int  getExpToLevel() const { return m_expToLevel; }

    // HP Management
    void takeDamage(int dmg) { m_hp = std::max(0, m_hp - dmg); }
    void heal(int amt)       { m_hp = std::min(m_maxHp, m_hp + amt); }
    bool isDead()      const { return m_hp <= 0; }

    // Experience & Leveling
    void addExp(int exp);
    void levelUp();

    // Merchant Inventory
    void setMerchantItems(const std::vector<std::string>& items);
    std::vector<std::string> getMerchantItems() const { return m_merchantItems; }

    // Position (for world placement)
    int  getCol()        const { return m_col; }
    int  getRow()        const { return m_row; }
    void setPos(int col, int row) { m_col = col; m_row = row; }

    // Facing direction (for sprite flip)
    bool getFacingLeft() const { return m_facingLeft; }
    void setFacing(bool left)  { m_facingLeft = left; }

    // ── Aut-based speed (ข้อ 3: ผูก companion เข้าระบบ turn/speed) ──
    // ตอนนี้ทุก role เท่ากันหมด (atkAut = 100 เหมือน player เริ่มต้น)
    // ถ้าอยากแยก speed ต่อ role ทีหลัง ปรับ m_atkAut ใน computeStats() ได้เลย
    long long getNextActTime() const { return m_nextActTime; }
    void      advanceActTime()       { m_nextActTime += m_atkAut; }
    int       getAtkAut()      const { return m_atkAut; }

private:
    std::string m_id;
    std::string m_name;
    std::string m_desc;
    std::string m_sprite;
    NPCType     m_type = NPCType::QuestGiver;
    NPCRole     m_role = NPCRole::Warrior;

    // Companion Stats
    int m_level = 1;
    int m_hp = 30;
    int m_maxHp = 30;
    int m_attack = 5;
    int m_defense = 2;
    int m_exp = 0;
    int m_expToLevel = 100;

    // World Position
    int  m_col        = 0;
    int  m_row        = 0;
    bool m_facingLeft = false;

    // ── Aut-based speed (ข้อ 3) ──
    int       m_atkAut      = 100;  // Aut ต่อการโจมตี 1 ครั้ง (เท่ากันทุก role ตอนนี้)
    long long m_nextActTime = 0;

    // Merchant
    std::vector<std::string> m_merchantItems;

    void computeStats();  // คำนวณ stats ตามระดับและ role
};
