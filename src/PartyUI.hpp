#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <vector>

class Party;
class NPC;
class Player;

class PartyUI
{
public:
    PartyUI();

    // render(window, party, leader)
    // leader = m_player (แสดง slot 0 เป็น player จริง)
    // ไม่วาดอะไรเลยถ้า m_visible == false
    void render(sf::RenderWindow& window, const Party& party, const Player* leader = nullptr);

    // เปิด/ปิดพาแนล — เรียกตอนกดปุ่ม (เช่น Tab)
    void toggle()                    { m_visible = !m_visible; }
    void setVisible(bool visible)    { m_visible = visible; }
    bool isVisible() const           { return m_visible; }

    // ── Selection & detail view (ข้อ 5: dismiss / reorder / view stats) ──
    // m_selectedIndex คือ index ใน Party::getMembers() (0-based, "ไม่รวม" leader ที่ slot 0)
    void moveSelection(int dir, int partySize);   // dir = -1 (ขึ้น) / +1 (ลง), clamp กับ partySize
    void resetSelection(int partySize);           // เรียกหลัง dismiss/party เปลี่ยนขนาด เพื่อ clamp cursor ใหม่
    int  getSelectedIndex() const                 { return m_selectedIndex; }
    void setSelectedIndex(int idx)                { m_selectedIndex = idx; }
    void toggleDetails()                          { m_showDetails = !m_showDetails; }
    bool isShowingDetails() const                 { return m_showDetails; }

private:
    // เก็บ text/shape ของแต่ละ slot ไว้ใช้ซ้ำ แทนสร้างใหม่ทุกเฟรม
    struct SlotUI
    {
        sf::Text nameText;
        sf::Text hpText;
        sf::Text lvText;
        sf::RectangleShape box{{(float)BOX_WIDTH, (float)BOX_HEIGHT}};
        sf::RectangleShape hpBg{{1.f, 8.f}};
        sf::RectangleShape hpFill{{1.f, 8.f}};

        // ค่าล่าสุดที่ set ไปแล้ว — ใช้เช็คว่าต้อง setString ใหม่ไหม
        std::string lastName;
        int  lastHp    = -1;
        int  lastMaxHp = -1;
        int  lastLevel = -1;
        bool initialized = false;

        explicit SlotUI(const sf::Font& font)
            : nameText(font), hpText(font), lvText(font) {}
    };

    void renderLeader(sf::RenderWindow& window, const Player* player);
    void renderMember(sf::RenderWindow& window, const std::shared_ptr<NPC>& npc, int slotIndex, bool isSelected);
    void renderDetails(sf::RenderWindow& window, const std::shared_ptr<NPC>& npc, int slotIndex);  // ข้อ 5: panel stats ละเอียด

    // อัปเดตค่า slot เฉพาะส่วนที่เปลี่ยนจริง (dirty-check)
    void updateSlotVisuals(SlotUI& slot, int y, const std::string& name,
                            int hp, int maxHp, int level, const sf::Color& bgColor,
                            const sf::Color& outlineColor = sf::Color::White, float outlineThickness = 2.f);

    sf::Font m_font;
    bool m_visible = false;

    // ── Selection & detail view (ข้อ 5) ──
    int  m_selectedIndex = 0;      // index ของ companion ที่ cursor อยู่ (0 = คนแรกใน Party)
    bool m_showDetails   = false;  // แสดง panel stats ละเอียดของ m_selectedIndex อยู่ไหม

    // cache ของ detail panel — กัน alloc sf::Text ใหม่ทุกเฟรม
    struct DetailUI
    {
        sf::RectangleShape box{{180.f, 90.f}};
        sf::Text roleText;
        sf::Text atkText;
        sf::Text defText;
        sf::Text expText;
        explicit DetailUI(const sf::Font& font)
            : roleText(font), atkText(font), defText(font), expText(font) {}
    };
    std::unique_ptr<DetailUI> m_detail;

    // slot 0 = leader, slot 1..6 = companions (Party::MAX_SIZE = 6)
    static constexpr int MAX_SLOTS = 7;
    std::vector<std::unique_ptr<SlotUI>> m_slots;

    static constexpr int SLOT_HEIGHT = 70;
    static constexpr int START_X     = 20;
    static constexpr int START_Y     = 20;
    static constexpr int BOX_WIDTH   = 200;
    static constexpr int BOX_HEIGHT  = 60;
};
